# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(remote_control)

add_subdirectory(utils)
add_subdirectory(protocols)
add_subdirectory(applications)

