# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(Yggdrasil)

#add subprojects
add_subdirectory(core)

add_subdirectory(protocols)

add_subdirectory(interfaces)
add_subdirectory(utils)
add_subdirectory(applications)

add_subdirectory(remote_control)

# get all *.c files recursively
#file(GLOB_RECURSE SRC_FILES_CORE ${PROJECT_SOURCE_DIR}/core/*.c)
#file(GLOB_RECURSE SRC_FILES_PROTOS ${PROJECT_SOURCE_DIR}/protocols/*.c)
#file(GLOB_RECURSE SRC_FILES_INTERFACES ${PROJECT_SOURCE_DIR}/interfaces/*.c)
#file(GLOB_RECURSE SRC_FILES_DATA_STRUCT ${PROJECT_SOURCE_DIR}/data_structures/*.c)
#file(GLOB_RECURSE SRC_FILES_APP_COMPONENTS ${PROJECT_SOURCE_DIR}/application_components/*.c)
#file(GLOB_RECURSE SRC_FILES_CONTROL_PROTOCOLS ${PROJECT_SOURCE_DIR}/remote_control/protocols/*.c)
#file(GLOB_RECURSE SRC_FILES_CONTROL_UTILS ${PROJECT_SOURCE_DIR}/remote_control/utils/*.c)


# include headers
#include_directories(.)

#include_directories(core)
#include_directories(../Yggdrasil-LowLevelLib/)
#include_directories(../Yggdrasil-LowLevelLib/src)

#include_directories(/usr/include)
#include_directories(/usr/include/uuid)
#include_directories(/usr/include/libnl3)

#set(CMAKE_BUILD_TYPE RelWithDebInfo)
#set(CMAKE_BUILD_TYPE Debug)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -O0 -lm -pthread -lnl-genl-3 -lnl-3 -luuid -lrt  -lssl  -lcrypto")
#set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -lm -lnl-genl-3 -lnl-3 -luuid -lrt  -lssl  -lcrypto")

#exclude files
#list(REMOVE_ITEM SRC_FILES_PROTOS ${PROJECT_SOURCE_DIR}/protocols/incomplete/under_development.c)
