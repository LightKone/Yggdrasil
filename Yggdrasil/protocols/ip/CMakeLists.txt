# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(IP_Protocols)

include_directories(../../)
include_directories(../../core)
include_directories(../../../Yggdrasil-LowLevelLib)

link_libraries(yggcore_ip)

#membership
add_library(hyparview

        membership/hyparview.c
        )

add_library(xbot

        membership/xbot.c
        utility/oracles/udp_oracle.c
        )

target_link_libraries(xbot PUBLIC hyparview PRIVATE hashfunctions)

#dissemination

add_library(flood
        dissemination/flood.c
        dissemination/common_dissemination_interface.c)

add_library(flood_flow_control
        dissemination/flood_flow_control.c)

add_library(plumtree
        dissemination/plumtree.c
        )

add_library(plumtree_mem_opt
        dissemination/plumtree_mem_optimized.c
        dissemination/common_dissemination_interface.c
        )

add_library(plumtree_mem_opt_flow_control
        dissemination/plumtree_mem_optimized_flow_control.c
        )

target_link_libraries(flood PRIVATE hashfunctions)
target_link_libraries(flood_flow_control PUBLIC flood PRIVATE hashfunctions)
#target_compile_definitions(flood_flow_control PUBLIC DEBUG_FLOOD)

target_link_libraries(plumtree PRIVATE hashfunctions)
target_link_libraries(plumtree_mem_opt PUBLIC plumtree PRIVATE hashfunctions)

#target_compile_definitions(plumtree_mem_opt_flow_control PUBLIC DEBUG_PLUMTREE)
target_link_libraries(plumtree_mem_opt_flow_control PUBLIC plumtree_mem_opt multi_tcp_dispatcher PRIVATE hashfunctions)

#data transfer
add_library(simple_data_transfer

        data_transfer/simple_data_transfer.c
        )

add_library(block_data_transfer

        data_transfer/block_data_transfer.c
        )



add_library(block_data_transfer_mem_opt
        data_transfer/block_data_transfer_mem_opt.c
        dissemination/common_dissemination_interface.c
        )

target_link_libraries(block_data_transfer_mem_opt PUBLIC block_data_transfer PRIVATE hashfunctions)


target_compile_definitions(block_data_transfer PUBLIC TEST_LOCAL_TRANSFER)
target_compile_definitions(block_data_transfer_mem_opt PUBLIC TEST_LOCAL_TRANSFER)
target_compile_definitions(simple_data_transfer PUBLIC TEST_LOCAL_TRANSFER)

#dispatcher
add_library(multi_tcp_dispatcher

        dispatcher/multi_tcp_socket_dispatcher.c
        )


add_library(tls_dispatcher

        dispatcher/simple_tls_dispatcher.c
        )

target_link_libraries(tls_dispatcher ssl crypto)
