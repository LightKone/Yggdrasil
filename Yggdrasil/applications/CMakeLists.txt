# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(Applications)

#add_compile_options( -g -Wall -O0 -lm -pthread -luuid -lrt)
#set( CMAKE_VERBOSE_MAKEFILE on )

add_subdirectory(demos)
add_subdirectory(tests)

add_subdirectory("file transfer")