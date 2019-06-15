# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(Yggdrasil_core)

include_directories(.)
include_directories(../../Yggdrasil-LowLevelLib)


file(GLOB_RECURSE SRC_FILES_DATA_STRUCT ./data_structures/generic/*.c)
add_library(gen_data_structs ${SRC_FILES_DATA_STRUCT})

add_library(neighbour_list data_structures/specialized/neighbour_list.c)
target_link_libraries(neighbour_list gen_data_structs)

add_library(yggcore_wireless

        ygg_runtime.c
        proto_data_struct.c
        utils/utils.c
        utils/queue.c
        utils/queue_elem.c

        protos/timer.c
        protos/executor.c
        protos/wireless/dispatcher.c

        )


target_compile_definitions(yggcore_wireless PUBLIC WIRELESS_NETWORKS)
#target_compile_options(yggcore_wireless BEFORE PUBLIC  -lm -pthread -luuid -lrt)
#set_property(TARGET yggcore_wireless APPEND_STRING PROPERTY LINK_FLAGS "-lm -luuid -lrt")
#target_link_options(yggcore_wireless PUBLIC "-lm -luuid -lrt")
target_link_libraries(yggcore_wireless PUBLIC ygglib_wireless PUBLIC gen_data_structs m pthread uuid rt)
target_include_directories(yggcore_wireless PUBLIC . ../../Yggdrasil-LowLevelLib)

add_library(yggcore_ip

        ygg_runtime.c
        proto_data_struct.c
        utils/utils.c
        utils/queue.c
        utils/queue_elem.c

        protos/timer.c
        protos/executor.c
        protos/ip/simple_tcp_dispatcher.c

        )

target_compile_definitions(yggcore_ip PUBLIC IP_NETWORKS)
#target_compile_options(yggcore_ip BEFORE PUBLIC  -lm -pthread -luuid -lrt)
#set_property(TARGET yggcore_ip APPEND_STRING PROPERTY LINK_FLAGS "-lm -luuid -lrt")
#target_link_options(yggcore_ip PUBLIC "-lm -luuid -lrt")

target_link_libraries(yggcore_ip PUBLIC ygglib_ip PUBLIC gen_data_structs m pthread uuid rt)
target_include_directories(yggcore_ip PUBLIC . ../../Yggdrasil-LowLevelLib)