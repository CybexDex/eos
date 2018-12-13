/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/claim_mongo/claim_mongo.hpp>

namespace eosio {
   static appbase::abstract_plugin& _claim_mongo = app().register_plugin<claim_mongo>();

class claim_mongo_impl {
   public:
};

claim_mongo::claim_mongo():my(new claim_mongo_impl()){}
claim_mongo::~claim_mongo(){}

void claim_mongo::set_program_options(options_description&, options_description& cfg) {
   cfg.add_options()
         ("option-name", bpo::value<string>()->default_value("default value"),
          "Option Description")
         ;
}

void claim_mongo::plugin_initialize(const variables_map& options) {
   try {
      if( options.count( "option-name" )) {
         // Handle the option
      }
   }
   FC_LOG_AND_RETHROW()
}

void claim_mongo::plugin_startup() {
   // Make the magic happen
}

void claim_mongo::plugin_shutdown() {
   // OK, that's enough magic
}

}
