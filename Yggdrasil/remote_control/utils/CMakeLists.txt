# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(remote_control_utils)

include_directories(../../)

add_library(commands commands.c)
target_link_libraries(commands topology_manager aggregation_interface)


add_library(cmdio cmdio.c control_protocol_utils.c)
target_link_libraries(cmdio uuid)
