file(GLOB HEADERS "include/eosio/claim_mongo/*.hpp")
add_library( claim_mongo
             claim_mongo.cpp
             ${HEADERS} )

target_link_libraries( claim_mongo appbase fc )
target_include_directories( claim_mongo PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
