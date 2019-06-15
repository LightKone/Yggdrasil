# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(wireless_test)

include_directories(../../../)
include_directories(../../../core)
include_directories(../../../../Yggdrasil-LowLevelLib)
#include_directories(/usr/include)
#include_directories(/usr/include/uuid)
#include_directories(/usr/include/libnl3)


#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -O0 -lm -pthread -lnl-genl-3 -lnl-3 -luuid -lrt  -lssl  -lcrypto")
#set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -lm -lnl-genl-3 -lnl-3 -luuid -lrt  -lssl  -lcrypto")

link_libraries(yggcore_wireless)


add_executable(test_batman batman_test.c)
target_link_libraries(test_batman batman topology_manager )

add_executable(test_dispatch_log dispatcher_logger_test.c)
target_link_libraries(test_dispatch_log batman topology_manager dispatcher_logger)

add_executable(test_dynamic_protos dynamic_protocols_test.c)
target_link_libraries(test_dynamic_protos fault_detection_discovery flow_updating mirage)

add_executable(test_fault_detector fault_detector_discovery_test.c)
target_link_libraries(test_fault_detector fault_detection_discovery)

add_executable(test_flow_updating flow_updating_test.c)
target_link_libraries(test_flow_updating flow_updating fault_detection_discovery)

add_executable(test_gap gap_test.c)
target_link_libraries(test_gap gap fault_detection_discovery)

add_executable(test_limosense limosense_test.c)
target_link_libraries(test_limosense limosense fault_detection_discovery)

add_executable(test_mirage mirage_test.c)
target_link_libraries(test_mirage mirage fault_detection_discovery)

add_executable(test_push_gossip push_gossip_test.c)
target_link_libraries(test_push_gossip push_gossip topology_manager)

add_executable(test_reliable_p2p reliable_test.c)
target_link_libraries(test_reliable_p2p reliable_p2p topology_manager batman)

add_executable(test_simple_discovery simple_discovery_test.c)
target_link_libraries(test_simple_discovery simple_discovery)

add_executable(test_topology topology_test.c)
target_link_libraries(test_topology topology_manager simple_discovery)
