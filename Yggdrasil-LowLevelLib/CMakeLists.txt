# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(Yggdrasil)

# get all *.cpp files recursively
file(GLOB_RECURSE SRC_FILES_SRC ${PROJECT_SOURCE_DIR}/src/*.c)

# include headers
include_directories(src)
include_directories(demos)
include_directories(/usr/include)
include_directories(/usr/include/uuid)
include_directories(/usr/include/libnl3)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O1 -pthread -lnl-genl-3 -lnl-3 -luuid -lrt")
set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -lnl-genl-3 -lnl-3 -luuid -lrt")

# add executables
add_library(ygglib ${SRC_FILES_SRC})
add_library(ygglllib_obj OBJECT ${SRC_FILES_SRC})

add_executable(Receiver demos/Receiver.c)
add_executable(ReceiverWithReply demos/ReceiverWithReply.c)
add_executable(Sender demos/Sender.c)
add_executable(SenderBrute demos/SenderBrute.c)
add_executable(SenderWithReply demos/SenderWithReply.c)

target_link_libraries(Receiver ygglib)
target_link_libraries(ReceiverWithReply  ygglib)
target_link_libraries(Sender ygglib)
target_link_libraries(SenderBrute ygglib)
target_link_libraries(SenderWithReply ygglib)

