# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project("file transfer")

include_directories(../../)
include_directories(../../core)
include_directories(../../../Yggdrasil-LowLevelLib)
#include_directories(/usr/include)
#include_directories(/usr/include/uuid)
#include_directories(/usr/include/libnl3)

set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O3")
#set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -lm -lnl-genl-3 -lnl-3 -luuid -lrt  -lssl  -lcrypto")


link_libraries(yggcore_ip)

add_executable(yggFileServer main.c)
target_link_libraries(yggFileServer block_data_transfer_mem_opt plumtree_mem_opt_flow_control hyparview multi_tcp_dispatcher)

add_executable(yggTransfer tools/transfer.c)


add_custom_target(yggFileTransfer)
add_dependencies(yggFileTransfer yggFileServer yggTransfer)