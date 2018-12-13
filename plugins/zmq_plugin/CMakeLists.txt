file(GLOB HEADERS "include/eosio/zmq_plugin/*.hpp")


SET( ZeroMQ_LIBRARY_DIRS "/usr/lib/x86_64-linux-gnu/" )

add_library( zmq_plugin
             zmq_plugin.cpp
             ${HEADERS} )
find_library(ZeroMQ_LIBRARY
             NAMES zmq
             PATHS ${ZeroMQ_LIBRARY_DIRS}
	     )



target_link_libraries( zmq_plugin 
	PUBLIC chain_plugin eosio_chain appbase zmq )
target_include_directories( zmq_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )



