/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <zmq.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


#include <eosio/zmq_plugin/zmq_plugin.hpp>
#include <eosio/chain/eosio_contract.hpp>
#include <eosio/chain/config.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/transaction.hpp>
#include <eosio/chain/types.hpp>

#include <fc/io/json.hpp>
#include <fc/utf8.hpp>
#include <fc/variant.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/chrono.hpp>
#include <boost/signals2/connection.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>





namespace fc { class variant; }
namespace eosio {
	
using chain::account_name;
using chain::action_name;
using chain::block_id_type;
using chain::permission_name;
using chain::transaction;
using chain::signed_transaction;
using chain::signed_block;
using chain::transaction_id_type;
using chain::packed_transaction;


static appbase::abstract_plugin& _zmq_plugin = app().register_plugin<zmq_plugin>();
class zmq_plugin_impl {
public:
	zmq_plugin_impl();
	~zmq_plugin_impl();
	struct action_trace_struct{
		chain::action_trace	base;
		string trx_status;
	};
   typedef vector<action_trace_struct>  bulk_write;
   fc::optional<boost::signals2::scoped_connection> irreversible_block_connection;

   void applied_irreversible_block(const chain::block_state_ptr&);

   void init();

   zmq::context_t * context;
   zmq::socket_t * publisher;

   bool configured{false};
   bool is_store_table{false};
   uint32_t start_block_num = 0;
   std::atomic_bool start_block_reached{false};

   bool is_producer = false;
   bool update_blocks_via_block_num = false;
   bool store_blocks = true;
   bool store_block_states = true;
   bool store_transactions = true;
   bool store_transaction_traces = true;
   bool store_action_traces = true;


   // consum thread
   size_t max_queue_size = 0;
   int queue_sleep_time = 0;
   boost::mutex mtx;
   std::atomic_bool done{false};
   std::atomic_bool startup{true};
   fc::optional<chain::chain_id_type> chain_id;


   static const action_name newaccount;
   static const action_name setabi;
   static const action_name updateauth;
   static const action_name deleteauth;
   static const permission_name owner;
   static const permission_name active;

};

const action_name zmq_plugin_impl::newaccount = chain::newaccount::get_name();
const action_name zmq_plugin_impl::setabi = chain::setabi::get_name();
const action_name zmq_plugin_impl::updateauth = chain::updateauth::get_name();
const action_name zmq_plugin_impl::deleteauth = chain::deleteauth::get_name();
const permission_name zmq_plugin_impl::owner = chain::config::owner_name;
const permission_name zmq_plugin_impl::active = chain::config::active_name;




void zmq_plugin_impl::applied_irreversible_block( const chain::block_state_ptr& bs ) {
   try {
      if( store_blocks || store_block_states || store_transactions ) {
	 auto json = "applied_irreversible_block " + fc::json::to_string(*bs);
      	 zmq::message_t message(json.size());
	 memcpy (message.data(), json.data(), json.size());
	 publisher->send(message);
	 ilog(json);
      }
   } catch (fc::exception& e) {
      elog("FC Exception while applied_irreversible_block ${e}", ("e", e.to_string()));
   } catch (std::exception& e) {
      elog("STD Exception while applied_irreversible_block ${e}", ("e", e.what()));
   } catch (...) {
      elog("Unknown exception while applied_irreversible_block");
   }
}

zmq_plugin_impl::zmq_plugin_impl() //:context(1),publisher(zmq::socket_t(context, ZMQ_PUB)){
{
 
}

zmq_plugin_impl::~zmq_plugin_impl() {
	// delete context;
   if (!startup) {
   	delete publisher;
   	delete context;
      try {
         ilog( "zmq_plugin shutdown in process please be patient this can take a few minutes" );
         done = true;

      } catch( std::exception& e ) {
         elog( "Exception on zmq_plugin shutdown of consume thread: ${e}", ("e", e.what()));
      }
   }
}

void zmq_plugin_impl::init() {
   // Create the native contract accounts manually; sadly, we can't run their contracts to make them create themselves
   // See native_contract_chain_initializer::prepare_database()

   ilog("init zmq");
   ilog("starting db plugin thread");
   startup = false;
}


zmq_plugin::zmq_plugin():my(new zmq_plugin_impl()){}
zmq_plugin::~zmq_plugin(){}

void zmq_plugin::set_program_options(options_description&, options_description& cfg) {
   cfg.add_options()
         ("zmq-addr", bpo::value<string>()->default_value("localhost:4321"),
          "zero mq pub socket address")
         ("zmq-store-table", bpo::value<string>()->default_value("eosio:voters"),
          "the table to share, format is <contract account>:<table name>, the info is listed in abi code, pls refer to that.")
         ("zmq-block-start", bpo::value<uint32_t>()->default_value(0),
          "If specified then on data pushed to zmq until specified block is reached.")
         ("zmq-store-blocks", bpo::value<bool>()->default_value(true),
          "Enables storing blocks to zmq.")
         ("zmq-store-block-states", bpo::value<bool>()->default_value(true),
          "Enables storing block states to zmq.")
         ("zmq-store-transactions", bpo::value<bool>()->default_value(true),
          "Enables storing transactions to zmq.")
         ("zmq-store-transactions-traces", bpo::value<bool>()->default_value(true),
          "Enables storing transaction traces to zmq.")
         ("zmq-store-action-traces", bpo::value<bool>()->default_value(true),
          "Enables storing action traces to zmq.")
         ;
}

void zmq_plugin::plugin_initialize(const variables_map& options) {
   try {
    if( options.count( "zmq-addr" ) == 0) {
         // Handle the option
	 wlog("zmq-addr must be specified when enabling plugin zmq");
    } else{
      if( options.count( "zmq-store-table" ) == 0) {
         // Handle the option
	 my->is_store_table = false;

      }else{
      	 my->is_store_table = true;
	 // to do 
      }
      if( options.count( "zmq-block-start" ) == 0) {
         // Handle the option
	 my->start_block_num = 0;
      }else{
      	 my->start_block_num = options.at( "zmq-block-start" ).as<uint32_t>();
      }
      if( options.count( "zmq-store-blocks" ) ) {
         // Handle the option
	 my->store_blocks = options.at( "zmq-store-blocks" ).as<bool>();
      }else{
	       my->store_blocks = false;
      }
      if( options.count( "zmq-store-block-states" )) {
         // Handle the option
	 my->store_block_states = options.at( "zmq-store-block-states" ).as<bool>();
      }else {
      	 my->store_block_states = false;
      }
      if( options.count( "zmq-store-transactions" ) ) {
         // Handle the option
	 my->store_transactions = options.at( "zmq-store-transactions" ).as<bool>();
      }else {
      	 my->store_transactions = false;
      }
      if( options.count( "zmq-store-transaction-traces" ) ) {
         // Handle the option
	 my->store_transaction_traces = options.at( "zmq-store-transaction-traces" ).as<bool>();
      }else {
      	 my->store_transaction_traces = false;
      }
      if( options.count( "zmq-store-action-traces" ) ) {
         // Handle the option
	 my->store_action_traces = options.at( "zmq-store-action-traces" ).as<bool>();
      }else {
      	 my->store_action_traces = false;
      }
      if( options.count( "producer-name" ) ) {
         // Handle the option
	 wlog( "zmq plugin not recommended on producer node" );
	 my->is_producer = true;
      }
      if( my->start_block_num == 0 ) {
         my->start_block_reached = true;
      }
      std::string addr_str = options.at( "zmq-addr" ).as<std::string>();

	ilog("connecting to ${u}", ("u",addr_str));
	// my->publisher.bind("tcp://" + addr_str);
	// try{
	my->context = new zmq::context_t(1);
	my->publisher = new zmq::socket_t(*my->context , ZMQ_PUB);
	my->publisher->bind(addr_str);

	std::string topic_base = "EOS";

	chain_plugin* chain_plug = app().find_plugin<chain_plugin>();
	EOS_ASSERT( chain_plug, chain::missing_chain_plugin_exception, ""  );
	auto& chain = chain_plug->chain();
        my->chain_id.emplace( chain.get_chain_id());

	my->irreversible_block_connection.emplace(
		chain.irreversible_block.connect( [&]( const chain::block_state_ptr& bs ) {
			my->applied_irreversible_block( bs );
	} ));
	my->init();
    }
   } FC_LOG_AND_RETHROW()
}

void zmq_plugin::plugin_startup() {
   // Make the magic happen
   ilog("zmq plugin start...");
}

void zmq_plugin::plugin_shutdown() {
   // OK, that's enough magic
   ilog("zmq plugin shutdown...");
        my->irreversible_block_connection.reset();
	my.reset();
}

}

FC_REFLECT( eosio::zmq_plugin_impl::action_trace_struct, (base)(trx_status) )
