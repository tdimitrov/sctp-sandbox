cmake_minimum_required ( VERSION 2.8 )

project (sctp_sandbox)

set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -std=c99")
find_package(Threads REQUIRED)

add_executable(server server.c common.h)
add_executable(client client.c common.h)
target_link_libraries(server ${CMAKE_THREAD_LIBS_INIT})
