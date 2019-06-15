# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(other_tests)

include_directories(../../../)
include_directories(../../../core)
include_directories(../../../../Yggdrasil-LowLevelLib)


#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -O0 -lm -pthread -lnl-genl-3 -lnl-3 -luuid -lrt  -lssl  -lcrypto")
#set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -lm -lnl-genl-3 -lnl-3 -luuid -lrt  -lssl  -lcrypto")


add_executable(file_server_client plumtree_file_transfer_client.c)


