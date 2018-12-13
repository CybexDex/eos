#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosio.token/eosio.token.hpp>

#include <utility>
#include <vector>
#include <string>
#include <map>
#include <iostream>


using namespace eosio;
using std::string;
class hello : public eosio::contract {
  public:
      using contract::contract;

      [[eosio::action]]
      // __attribute__((eosio_action)) 
      void hi( account_name user , string s1, asset as1, uint32_t aaa) {
      // void hi( account_name user ) {
      	// require_auth( user );
	      std::map<std::string , int> mmp;
	 mmp["asdfdeafd"] = 9088;     
         print( "Hello, ", name{user} );
	/*action(
	action(
		permission_level{ user, N(active) },
		N(eosio.token), N(transfer),
		std::make_tuple(user, user, as1, s1)
	 ).send();
		permission_level{ _self, N(active) },
		N(eosio.token), N(transfer),
		std::make_tuple(_self, user, as1, s1)
	 ).send();*/
	 INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {user ,N(active)}, { user , _self, as1, s1} );
	 // INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {_self, N(active)}, { _self , user, as1, s1} );
      }
  private:
      struct [[eosio::table]] test{
      	account_name s1;
	EOSLIB_SERIALIZE(test,(s1))
      };
};

EOSIO_ABI( hello, (hi) )
