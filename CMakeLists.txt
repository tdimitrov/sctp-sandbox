cmake_minimum_required ( VERSION 2.8 )

project (sctp_sandbox)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11")
find_package(Threads REQUIRED)

add_executable(server server.cpp common.h)
add_executable(client client.cpp common.h)
target_link_libraries(server ${CMAKE_THREAD_LIBS_INIT})
