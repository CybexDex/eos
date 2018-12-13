#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/transaction.hpp>
#include <utility>
#include <vector>
#include <string>
// #include <map>
#include <iostream>

using eosio::key256;
using eosio::indexed_by;
using eosio::const_mem_fun;
using eosio::asset;
using eosio::permission_level;
using eosio::action;
using eosio::print;
using eosio::name;


using namespace eosio;
using std::string;
class redpacket: public eosio::contract {
using contract::contract;
  public:
      // const uint32_t ONE_MONTH = 30*24*60*60;
      const uint32_t ONE_MONTH = 30;

      redpacket(account_name s)
	      :eosio::contract(s), offers(_self,_self),takes(_self,_self){
      }

      [[eosio::action]]
      void hi( account_name user , string memo = "hello!") {
         print( "Hello, ", name{user} );
      }
      [[eosio::action]]
      void clear(account_name user){
	require_auth(_self);
	auto it = offers.begin();
	while(it != offers.end()){
		print("Delete ", it->id, ";maker: ",name{it->maker},"; key: " ,it->key,  "\n");
		offers.erase(it);
		it = offers.begin();
	};
	auto it2 = takes.begin();
	while(it2 != takes.end()){
		print("Delete ", it2->id, ";taker:", name{it2->taker} , ";asset: ",it2->red,"; owner_id: " ,it2->owner_id,"\n");
		takes.erase(it2);
		it2 = takes.begin();
	}
      }
      [[eosio::action]]
      void make( uint64_t key, account_name maker, asset pool, uint32_t people_num, uint32_t expiration_time = 12*3600) {
	require_auth(maker);
	eosio_assert( expiration_time <= 24*3600 , "expiretime should be less than 24h" );
	eosio_assert( pool.is_valid(), "invalid currency" );
	eosio_assert( pool.amount > 0, "must positive quantity" );
	clear_expire();
	eosio_assert(!is_exist(maker,key), "no dup entry!" );
	auto ofitr = offers.emplace(_self, [&](auto& offer){
	  offer.id = offers.available_primary_key();
	  offer.maker = maker;
	  offer.key = key;
	  offer.pool = pool;
	  offer.expiration_time = eosio::time_point_sec(now() + expiration_time);
	  offer.left = pool;
	  offer.total_p_num = people_num;
	  offer.left_p_num = people_num;
	} );
	action(
	  permission_level{ maker, N(active) }, 
	  N(eosio.token), N(transfer),
	  std::make_tuple(maker, _self, pool, string("send a redpacket"))
	).send();

	transaction out;
	out.actions.emplace_back( permission_level{ maker, N(active) }, _self, N(take), std::make_tuple(maker, maker, key) );
	out.delay_sec = expiration_time + 1; // ensure to be expiered to trigger withdraw
	// out.send( _self, _self, false);
	out.send( key , _self, false);

	print("Redpacket from ", name{maker}, " with key ", key, " to ", people_num, " people with total " , pool);
	
      }

      [[eosio::action]]
      void take( account_name taker, account_name maker, uint64_t id) {
	require_auth(taker);
	eosio_assert( is_exist(maker,id), "no key exist!" );
	
	auto idx =  offers.template get_index<N(ofcommitment)>();
	auto k = get_commitment(maker, id);
	auto citr = idx.find(k);
	/*if(taker != maker){
		eosio_assert( !is_taken(taker, citr->id), "id conflict!");
	}else{
		print("Take the redpacket from self; ");
	}*/
	auto itr = offers.find(citr->id);
	eosio_assert( itr->left.amount > 0, "packet run out" );
	if( has_expired(itr->expiration_time )|| itr->left_p_num == 0 ) {
	    withdraw(maker, id);
	    
	    return;
	}
	eosio_assert( !is_taken(taker, citr->id), "id conflict!");
	// eosio_assert( !has_expired(itr->expiration_time),"red expired!");
	eosio_assert(itr->left_p_num >= 1, "enough people");
	eosio_assert(itr->left.amount > 0, "run out of money");
	asset _ast;
	offers.modify(itr, 0, [&](auto& ele){
	    auto tmp_ast  = itr->left;
	    _ast = tmp_ast/ itr->left_p_num;
	    ele.left = itr->left - _ast;
	    ele.left_p_num = itr->left_p_num - 1;
	});
	auto take_itr = takes.emplace(_self, [&](auto& ele){
	    ele.id =  takes.available_primary_key();
	    ele.red =  _ast;		
	    ele.owner_id = itr->id;		
	    ele.taker = taker;		
	    ele.take_time = eosio::time_point_sec(now());		
	});
	action(
	  permission_level{ _self, N(active) },
	  N(eosio.token), N(transfer),
	  std::make_tuple(_self, taker, _ast, string("take a redpacket"))
	).send();
	print("RedTaker ", name{taker}, " take packet ",_ast ," from ", name{maker}," with key ", id, "\n");
	print("Redpacket id(", itr->id, ") maker(", name{maker},") with key(", id, ") has left(", itr->left, ") \n"   );
      }
      [[eosio::action]]
      void view(account_name maker, uint64_t id){
	require_auth( maker );
      	auto k = get_commitment(maker, id);
	auto ofidx =  offers.template get_index<N(ofcommitment)>();
	auto idx =  takes.template get_index<N(owner)>();
	auto itr  = ofidx.find(k);
	eosio_assert(itr != ofidx.end(), " not found");
	auto owner_id = itr->id;
	print("view maker: ", name{itr->maker}, "; owner_id: ", itr->id, "\n ");
	auto itr2 = idx.find(owner_id);
	while(itr2 != idx.end() && itr2->owner_id == owner_id){
		print("maker_id: ", itr2->owner_id , "; taker: ",name{itr2->taker} , "; red: ", itr2->red ," \n" );
		itr2 ++ ;
	}
      
      }

  private:
	struct  [[eosio::table]] offer {
	    uint64_t id;
	    account_name maker;
	    uint64_t key;
	    asset pool;
	    eosio::time_point_sec expiration_time;
	    asset left;
	    uint32_t total_p_num;
	    uint32_t left_p_num;
	    uint64_t primary_key()const { return id;}
	    key256 by_commitment()const { return get_commitment(maker, key ); }
	    uint64_t by_time()const { return expiration_time.sec_since_epoch(); }
	    EOSLIB_SERIALIZE( offer, (id)(maker)(key)(pool)(expiration_time)(left)(total_p_num)(left_p_num) )
	};
	struct [[eosio::table]] taketype{
	    uint64_t id;
	    account_name taker;
	    asset red;
	    uint64_t owner_id;
	    eosio::time_point_sec take_time;

	    uint64_t primary_key()const { return id;}
	    // key256 by_commitment()const { return get_commitment(owner.maker ,taker ,owner.key ); }
	    key256 by_commitment()const { return get_commitment(0,taker ,owner_id ); }
	    uint64_t by_owner()const { return owner_id ; }
	    EOSLIB_SERIALIZE(taketype , (id)(taker)(red)(owner_id)(take_time) )
	};

	typedef eosio::multi_index< N(offer), redpacket::offer ,
		indexed_by< N(ofcommitment), const_mem_fun<offer, key256,  &offer::by_commitment> >,
		indexed_by< N(time), const_mem_fun<offer, uint64_t ,  &offer::by_time> >
	> offer_index;
	typedef eosio::multi_index< N(taketype), redpacket::taketype,
		indexed_by< N(tkcommitment), const_mem_fun<taketype, key256,  &taketype::by_commitment> >,
		indexed_by< N(owner), const_mem_fun<taketype, uint64_t,  &taketype::by_owner> >
	> takes_index;
	offer_index offers;
	takes_index takes;
	bool is_exist(account_name owner, uint64_t pub_key){
		auto k = get_commitment(owner, pub_key);
		auto idx = offers.template get_index<N(ofcommitment)>();
		return idx.find(k) != idx.end();
	}
	bool is_taken(account_name taker, uint64_t owner_id){
		auto k = get_commitment(0, taker ,owner_id);
		auto idx = takes.template get_index<N(tkcommitment)>();
		return idx.find(k) != idx.end();

	}
	bool has_expired( eosio::time_point_sec t ){
		return eosio::time_point_sec(now()) > t;
	}
	void withdraw(const account_name& maker, uint64_t id){
		auto k = get_commitment(maker, id);
		auto idx = offers.template get_index<N(ofcommitment)>();
		auto itr = idx.find(k);
		auto mitr = offers.find(itr->id);
		auto tmp_ast  = itr->left;
		if(itr->left.amount > 0){	
		  action(
		   permission_level{ _self , N(active) },
		   N(eosio.token), N(transfer),
		   std::make_tuple(_self, maker, tmp_ast, string("back to redpacket owner"))
		  ).send();
		}
		offers.modify(mitr, 0, [&](auto& ele){
		  ele.left = itr->left - tmp_ast;
		  ele.left_p_num = 0;
		});
		// offers.erase(mitr); // whether or not?????????
		print("Withdraw redpacket from ", name{maker}, " with key ", id, " with left " , tmp_ast);
	}
	static key256 get_commitment(const checksum256& commitment) {
		const uint64_t *p64 = reinterpret_cast<const uint64_t *>(&commitment);
		return key256::make_from_word_sequence<uint64_t>(p64[0], p64[1], p64[2], p64[3]);
	}
	static key256 get_commitment(account_name maker, uint64_t id) {
		return key256::make_from_word_sequence<uint64_t>(maker, uint64_t(0) , id );
	}


	static key256 get_commitment(account_name maker, account_name taker, uint64_t id) {
		return key256::make_from_word_sequence<uint64_t>(maker, taker, id );
	}
	void clear_expire(){
		print("Run clear expire.");
		auto idx = offers.template get_index<N(time)>();
		auto takeidx = takes.template get_index<N(owner)>();
		auto nnow = now() - ONE_MONTH;
		auto itr = idx.begin();
		while(itr!=idx.end() && itr->by_time() < nnow){
			auto tmpitr = itr;
			itr ++ ;
			auto oid = tmpitr->id;
			idx.erase(tmpitr);
			auto tmpitr2 = takeidx.find(oid);
			while(tmpitr2 != takeidx.end() && tmpitr2->owner_id == oid ){
				auto tmpitr3 = tmpitr2;
				tmpitr2 ++;
				takeidx.erase(tmpitr3);
			}
		}	
	
	}

};


// EOSIO_ABI( redpacket, (make)(take)(view) )
EOSIO_ABI( redpacket, (hi)(clear)(make)(take)(view) )
