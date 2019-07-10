# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(ip_tests)

include_directories(../../../)
include_directories(../../../core)
include_directories(../../../../Yggdrasil-LowLevelLib)
#include_directories(/usr/include)
#include_directories(/usr/include/uuid)
#include_directories(/usr/include/libnl3)


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -O0")
#set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -lm -lnl-genl-3 -lnl-3 -luuid -lrt  -lssl  -lcrypto")



link_libraries(yggcore_ip)

#ip
add_executable(test_hyparview hyparview_test.c)
target_link_libraries(test_hyparview PUBLIC hyparview)

add_executable(test_plumtree plumtree_test.c)
target_link_libraries(test_plumtree plumtree hyparview)

add_executable(test_xbot xbot_test.c)
target_link_libraries(test_xbot xbot)

add_executable(test_tls tls_test.c)
target_link_libraries(test_tls tls_dispatcher)

add_executable(test_multi_tcp_dispatcher multi_dispatcher_test.c)
target_link_libraries(test_multi_tcp_dispatcher plumtree hyparview multi_tcp_dispatcher)

add_executable(file_server plumtree_file_transfer.c)
target_link_libraries(file_server plumtree hyparview multi_tcp_dispatcher)

add_executable(simple_transfer_server simple_transfer_test.c)
target_link_libraries(simple_transfer_server simple_data_transfer plumtree hyparview multi_tcp_dispatcher)

add_executable(block_transfer_server block_transfer_test.c)
target_link_libraries(block_transfer_server block_data_transfer plumtree hyparview multi_tcp_dispatcher)

add_executable(block_transfer_opt_server block_transfer_mem_opt_test.c)
target_link_libraries(block_transfer_opt_server block_data_transfer_plumtree plumtree_mem_opt hyparview multi_tcp_dispatcher)

add_executable(block_transfer_opt_flow_server block_transfer_mem_flow_opt_test.c)
target_link_libraries(block_transfer_opt_flow_server block_data_transfer_mem_opt plumtree_mem_opt_flow_control hyparview multi_tcp_dispatcher)


add_executable(block_transfer block_transfer_full_test.c)
target_link_libraries(block_transfer block_data_transfer_mem_opt plumtree_mem_opt_flow_control plumtree_mem_opt flood flood_flow_control hyparview multi_tcp_dispatcher)


add_custom_target(ip_tests)
add_dependencies(ip_tests test_hyparview test_plumtree test_xbot test_multi_tcp_dispatcher file_server simple_transfer_server block_transfer_server block_transfer_opt_server)