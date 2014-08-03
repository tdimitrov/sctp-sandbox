cmake_minimum_required ( VERSION 2.8 )

find_package ( Threads REQUIRED )

set( CMAKE_CXX_FLAGS  "-std=c++11 -g" )

add_executable(server server.cpp)
add_executable(client client.cpp)

target_link_libraries ( server ${CMAKE_THREAD_LIBS_INIT} )