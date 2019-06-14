# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(Tests)

include_directories(../../)
include_directories(../../core)
include_directories(../../../Yggdrasil-LowLevelLib)
include_directories(/usr/include)
include_directories(/usr/include/uuid)
include_directories(/usr/include/libnl3)


#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -O0 -lm -pthread -lnl-genl-3 -lnl-3 -luuid -lrt  -lssl  -lcrypto")
#set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -lm -lnl-genl-3 -lnl-3 -luuid -lrt  -lssl  -lcrypto")


#ip

add_executable(test_hyparview hyparview_test.c)
target_link_libraries(test_hyparview PUBLIC hyparview yggcore_ip)


add_executable(block_transfer_server block_transfer_test.c)
target_link_libraries(block_transfer_server block_data_transfer plumtree hyparview multi_tcp_dispatcher yggcore_ip)
#target_compile_definitions(block_transfer_server PUBLIC TEST_LOCAL_TRANSFER)

add_executable(test_plumtree plumtree_test.c)
target_link_libraries(test_plumtree plumtree hyparview yggcore_ip)

add_executable(test_xbot xbot_test.c)
target_link_libraries(test_xbot xbot yggcore_ip)
#add_executable(plumtree $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces>   applications/tests/plumtree_test.c)
#target_link_libraries(plumtree yggcore_ip)
#add_executable(xbot $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces>   applications/tests/xbot_test.c)
#target_link_libraries(xbot yggcore_ip)
#[[
#add_executable(tls $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglib_base_obj> $<TARGET_OBJECTS:ygglib_ip_obj> $<TARGET_OBJECTS:ygglib_wireless_obj> applications/tests/tls_test.c)
add_executable(dispatch $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglib_base_obj> $<TARGET_OBJECTS:ygglib_ip_obj> $<TARGET_OBJECTS:ygglib_wireless_obj> applications/tests/multi_dispatcher_test.c)
add_executable(file_server $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglib_base_obj> $<TARGET_OBJECTS:ygglib_ip_obj> $<TARGET_OBJECTS:ygglib_wireless_obj> applications/tests/plumtree_file_transfer.c)
add_executable(file_server_client applications/tests/plumtree_file_transfer_client.c)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D TEST_LOCAL_TRANSFER")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D DEBUG_HYPERVIEW -D DEBUG_MULTI_DISPATCHER")
add_executable(simple_transfer_server $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglib_base_obj> $<TARGET_OBJECTS:ygglib_ip_obj> $<TARGET_OBJECTS:ygglib_wireless_obj> applications/tests/simple_transfer_test.c)
add_executable(block_transfer_server $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglib_base_obj> $<TARGET_OBJECTS:ygglib_ip_obj> $<TARGET_OBJECTS:ygglib_wireless_obj> applications/tests/block_transfer_test.c)
add_executable(block_transfer_opt_server $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglib_base_obj> $<TARGET_OBJECTS:ygglib_ip_obj> $<TARGET_OBJECTS:ygglib_wireless_obj> applications/tests/block_transfer_mem_opt_test.c)
]]