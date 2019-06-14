# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(IP_Protocols)

include_directories(../../)
include_directories(../../core)
include_directories(../../../Yggdrasil-LowLevelLib)

#membership
add_library(hyparview

        membership/hyparview.c
        )

add_library(xbot

        membership/xbot.c
        utility/oracles/udp_oracle.c
        )


#dissemination
#TODO change this
add_library(plumtree
        dissimination/plumtree.c
        )

add_library(plumtree_mem_opt
        dissimination/plumtree_mem_optimized.c
        )

target_link_libraries(plumtree PRIVATE hashfunctions)
target_link_libraries(plumtree_mem_opt PRIVATE hashfunctions)

#data transfer
add_library(block_data_transfer

        data_transfer/block_data_transfer.c
        )

add_library(block_data_transfer_plumtree

        data_transfer/block_data_transfer_plumtree.c
        )

add_library(simple_data_transfer

        data_transfer/simple_data_transfer.c
        )

target_compile_definitions(block_data_transfer PUBLIC TEST_LOCAL_TRANSFER)
target_compile_definitions(block_data_transfer_plumtree PUBLIC TEST_LOCAL_TRANSFER)
target_compile_definitions(simple_data_transfer PUBLIC TEST_LOCAL_TRANSFER)

#dispatcher
add_library(multi_tcp_dispatcher

        dispatcher/multi_tcp_socket_dispatcher.c
        )

add_library(tls_dispatcher

        dispatcher/simple_tls_dispatcher.c
        )

target_compile_options(tls_dispatcher BEFORE PUBLIC -lssl  -lcrypto)

#this must be at the end
link_libraries(yggcore_ip)