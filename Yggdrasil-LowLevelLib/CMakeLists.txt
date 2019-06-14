# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(Yggdrasil_ll)

# get all *.cpp files recursively
file(GLOB_RECURSE SRC_FILES_SRC ${PROJECT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE SRC_IP_FILES_SRC ${PROJECT_SOURCE_DIR}/src_ip/*.c)
file(GLOB_RECURSE SRC_WIRELESS_FILES_SRC ${PROJECT_SOURCE_DIR}/src_wireless/*.c)



add_library(ygglib_base ${SRC_FILES_SRC})
add_library(ygglib_ip ${SRC_IP_FILES_SRC})
add_library(ygglib_wireless ${SRC_WIRELESS_FILES_SRC})

#target_compile_options(ygglib_wireless PUBLIC "-lnl-genl-3 -lnl-3")
#set_property(TARGET ygglib_wireless APPEND_STRING PROPERTY LINK_FLAGS "-lnl-genl-3 -lnl-3")

target_link_libraries(ygglib_ip PUBLIC ygglib_base)
target_link_libraries(ygglib_wireless PUBLIC ygglib_base nl-genl-3 nl-3)

#target_link_options(ygglib_wireless PUBLIC "-lnl-genl-3 -lnl-3")

# include headers
#[[
include_directories(/usr/include)
include_directories(/usr/include/uuid)
include_directories(/usr/include/libnl3)

set(CMAKE_C_FLAGS_BASE "${CMAKE_C_FLAGS} -Wall -O1 -pthread")
set(CMAKE_C_FLAGS_WIRELESS "${CMAKE_C_FLAGS} -Wall -O1 -pthread -lnl-genl-3 -lnl-3 -luuid -lrt")
set(CMAKE_C_LINK_EXECUTABLE_BASE "${CMAKE_C_LINK_EXECUTABLE}")
set(CMAKE_C_LINK_EXECUTABLE_WIRELESS "${CMAKE_C_LINK_EXECUTABLE} -lnl-genl-3 -lnl-3 -luuid -lrt")

# add executables
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_BASE}")
set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE_BASE}")
add_library(ygglib_base ${SRC_FILES_SRC})
add_library(ygglib_base_obj OBJECT ${SRC_FILES_SRC})
add_library(ygglib_ip ${SRC_IP_FILES_SRC})
add_library(ygglib_ip_obj OBJECT ${SRC_IP_FILES_SRC})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_WIRELESS}")
set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE_WIRELESS}")
add_library(ygglib_wireless ${SRC_WIRELESS_FILES_SRC})
add_library(ygglib_wireless_obj OBJECT ${SRC_WIRELESS_FILES_SRC})

add_executable(Receiver demos/Receiver.c)
add_executable(ReceiverWithReply demos/ReceiverWithReply.c)
add_executable(Sender demos/Sender.c)
add_executable(SenderBrute demos/SenderBrute.c)
add_executable(SenderWithReply demos/SenderWithReply.c)

target_link_libraries(Receiver ygglib_base ygglib_wireless)
target_link_libraries(ReceiverWithReply  ygglib_base ygglib_wireless)
target_link_libraries(Sender ygglib_base ygglib_wireless)
target_link_libraries(SenderBrute ygglib_base ygglib_wireless)
target_link_libraries(SenderWithReply ygglib_base ygglib_wireless)
]]
