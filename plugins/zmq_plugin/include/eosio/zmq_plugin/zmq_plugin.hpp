/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include <eosio/chain_plugin/chain_plugin.hpp>
#include <appbase/application.hpp>

#include <memory>

namespace eosio {

using zmq_plugin_impl_ptr = std::shared_ptr<class zmq_plugin_impl>;


using namespace appbase;

/**
 *  This is a template plugin, intended to serve as a starting point for making new plugins
 */
class zmq_plugin : public appbase::plugin<zmq_plugin> {
public:
   zmq_plugin();
   virtual ~zmq_plugin();
 
   APPBASE_PLUGIN_REQUIRES((chain_plugin))
   virtual void set_program_options(options_description&, options_description& cfg) override;
 
   void plugin_initialize(const variables_map& options);
   void plugin_startup();
   void plugin_shutdown();

private:
   // std::unique_ptr<class zmq_plugin_impl> my;
   zmq_plugin_impl_ptr my;
};

}
