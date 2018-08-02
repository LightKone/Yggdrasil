# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(Yggdrasil)

# get all *.c files recursively
file(GLOB_RECURSE SRC_FILES_CORE ${PROJECT_SOURCE_DIR}/core/*.c)
file(GLOB_RECURSE SRC_FILES_PROTOS ${PROJECT_SOURCE_DIR}/protocols/*.c)
file(GLOB_RECURSE SRC_FILES_INTERFACES ${PROJECT_SOURCE_DIR}/interfaces/*.c)
file(GLOB_RECURSE SRC_FILES_DATA_STRUCT ${PROJECT_SOURCE_DIR}/data_structures/*.c)
file(GLOB_RECURSE SRC_FILES_APP_COMPONENTS ${PROJECT_SOURCE_DIR}/application_components/*.c)
file(GLOB_RECURSE SRC_FILES_CONTROL_PROTOCOLS ${PROJECT_SOURCE_DIR}/remote_control/protocols/*.c)
file(GLOB_RECURSE SRC_FILES_CONTROL_UTILS ${PROJECT_SOURCE_DIR}/remote_control/utils/*.c)


# include headers
include_directories(.)

include_directories(../Yggdrasil-LowLevelLib/)
include_directories(../Yggdrasil-LowLevelLib/src)

include_directories(/usr/include)
include_directories(/usr/include/uuid)
include_directories(/usr/include/libnl3)

#set(CMAKE_BUILD_TYPE RelWithDebInfo)
#set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -O0 -lm -pthread -lnl-genl-3 -lnl-3 -luuid -lrt")
set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -lm -lnl-genl-3 -lnl-3 -luuid -lrt")

#exclude files
#list(REMOVE_ITEM SRC_FILES_PROTOS ${PROJECT_SOURCE_DIR}/protocols/incomplete/under_development.c)

#add libraries
add_library(yggprotos OBJECT ${SRC_FILES_PROTOS})
add_library(yggcore OBJECT ${SRC_FILES_CORE})
add_library(ygginterfaces OBJECT ${SRC_FILES_INTERFACES})
add_library(yggdatastruct OBJECT ${SRC_FILES_DATA_STRUCT})

add_dependencies(yggprotos yggcore)

add_executable(simple_discovery $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/tests/simple_discovery_test.c)
add_executable(fault_detector_discovery $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/tests/fault_detector_discovery_test.c)
add_executable(mirage $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/tests/mirage_test.c)
add_executable(gap $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/tests/gap_test.c)
add_executable(limosense $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/tests/limosense_test.c)
add_executable(flow_updating $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/tests/flow_updating_test.c)
add_executable(topology $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/tests/topology_test.c)
add_executable(batman $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/tests/batman_test.c)
add_executable(reliable $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/tests/reliable_test.c)
add_executable(push_gossip $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/tests/push_gossip_test.c)
add_executable(dispatcher_logger $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/tests/dispatcher_logger_test.c)
add_executable(dynamic $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/tests/dynamic_protocols_test.c)

#REMOTE CONTROL
add_library(yggcontrol_protocols OBJECT ${SRC_FILES_CONTROL_PROTOCOLS})
add_library(yggcontrol_utils OBJECT ${SRC_FILES_CONTROL_UTILS})
add_executable(YggdrasilControlProcess  $<TARGET_OBJECTS:yggcontrol_protocols> $<TARGET_OBJECTS:yggcontrol_utils> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> remote_control/applications/ControlStructure.c)
add_executable(cmdbuildtree $<TARGET_OBJECTS:yggcontrol_utils>  $<TARGET_OBJECTS:yggcontrol_protocols> $<TARGET_OBJECTS:yggcontrol_utils> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> remote_control/applications/tools/buildtree.c)
add_executable(cmdchecktree $<TARGET_OBJECTS:yggcontrol_protocols> $<TARGET_OBJECTS:yggcontrol_utils> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> remote_control/applications/tools/checktree.c)
add_executable(cmdgetneighbors $<TARGET_OBJECTS:yggcontrol_protocols> $<TARGET_OBJECTS:yggcontrol_utils> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> remote_control/applications/tools/getneighbors.c)
add_executable(cmdexecuteexperience $<TARGET_OBJECTS:yggcontrol_protocols> $<TARGET_OBJECTS:yggcontrol_utils> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> remote_control/applications/tools/beginexperience.c)
add_executable(cmdterminateexperience $<TARGET_OBJECTS:yggcontrol_protocols> $<TARGET_OBJECTS:yggcontrol_utils> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> remote_control/applications/tools/terminateexperience.c)
add_executable(cmdchangevalue $<TARGET_OBJECTS:yggcontrol_protocols> $<TARGET_OBJECTS:yggcontrol_utils> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> remote_control/applications/tools/changevalue.c)
add_executable(cmdchangelink $<TARGET_OBJECTS:yggcontrol_protocols> $<TARGET_OBJECTS:yggcontrol_utils> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> remote_control/applications/tools/changelink.c)
add_executable(cmdreboot $<TARGET_OBJECTS:yggcontrol_protocols> $<TARGET_OBJECTS:yggcontrol_utils> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> remote_control/applications/tools/reboot.c)
add_executable(cmdshutdown $<TARGET_OBJECTS:yggcontrol_protocols> $<TARGET_OBJECTS:yggcontrol_utils> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> remote_control/applications/tools/shutdown.c)
add_executable(cmdenablediscovery $<TARGET_OBJECTS:yggcontrol_protocols> $<TARGET_OBJECTS:yggcontrol_utils> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> remote_control/applications/tools/enableannounces.c)
add_executable(cmddisablediscovery $<TARGET_OBJECTS:yggcontrol_protocols> $<TARGET_OBJECTS:yggcontrol_utils> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> remote_control/applications/tools/disableannounces.c)
add_executable(cmddebug $<TARGET_OBJECTS:yggcontrol_protocols> $<TARGET_OBJECTS:yggcontrol_utils> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> remote_control/applications/tools/debugneighbors.c)

# add executables of demos
add_executable(MessageExchange $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/demos/MessageExchange.c)
add_executable(OneHopBcast $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/demos/OneHopBcast.c)
add_executable(OneHopBcastWithTimer $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:ygginterfaces> $<TARGET_OBJECTS:yggdatastruct> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> applications/demos/OneHopBcastWithTimer.c)
