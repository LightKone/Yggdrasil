# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(remote_control_protocols)

include_directories(../../)

link_libraries(yggcore_wireless)

add_library(control_server control_protocol_server.c)

add_library(control_discovery control_discovery.c)

add_library(control_tree control_command_tcp_tree.c ../utils/control_protocol_utils.c)
target_link_libraries(control_tree commands)



