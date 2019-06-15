# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(Wireless_Protocols)


include_directories(../../)
include_directories(../../core)
include_directories(../../../Yggdrasil-LowLevelLib)

link_libraries(yggcore_wireless)

#aggregation
add_library(flow_updating
        aggregation/flow_updating.c
        )

target_link_libraries(flow_updating PUBLIC aggregation_interface)

add_library(gap
        aggregation/gap.c
        )

target_link_libraries(gap PUBLIC aggregation_interface)

add_library(limosense
        aggregation/LiMoSense.c
        )

target_link_libraries(limosense PUBLIC aggregation_interface PRIVATE neighbour_list)

add_library(mirage
        aggregation/MiRAge.c
        )

target_link_libraries(mirage PUBLIC aggregation_interface)

#communication

add_library(push_gossip
        communication/broadcast/push_gossip.c
        )

add_library(reliable_p2p
        communication/point-to-point/reliable.c
        )

add_library(batman
        communication/routing/batman.c
        )

target_link_libraries(batman PUBLIC routing_interface PRIVATE neighbour_list)

#discovery

add_library(simple_discovery
        discovery/simple_discovery.c
        )
target_link_libraries(simple_discovery PUBLIC discovery_interface PRIVATE neighbour_list)

add_library(fault_detection_discovery
        discovery/fault_detector_discovery.c
        )

target_link_libraries(fault_detection_discovery PUBLIC discovery_interface PRIVATE neighbour_list)

#dispatcher

add_library(dispatcher_logger
        dispatcher/dispatcher_logger.c
        )

#utility
add_library(topology_manager
        utility/topologyManager.c
        )

