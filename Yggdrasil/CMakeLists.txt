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
#list(REMOVE_ITEM SRC_FILES_PROTOS ${PROJECT_SOURCE_DIR}/protos/aggregation/push_sum.c)
list(REMOVE_ITEM SRC_FILES_PROTOS ${PROJECT_SOURCE_DIR}/protos/control/control_command_tree.c)
list(REMOVE_ITEM SRC_FILES_PROTOS ${PROJECT_SOURCE_DIR}/protos/routing/aodv.c)
list(REMOVE_ITEM SRC_FILES_PROTOS ${PROJECT_SOURCE_DIR}/protos/routing/olsr.c)
list(REMOVE_ITEM SRC_FILES_PROTOS ${PROJECT_SOURCE_DIR}/protos/addons/air_traffic_control.c)

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

# add executables of tests
#add_executable(bloomfilterTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/bloomfilterTest.c)
#add_executable(discoveryTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/discoveryTest.c)
#add_executable(dispatcherTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/dispatcherTest.c)
#add_executable(GLS_test $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/GLS_test.c)
#add_executable(pushGossipDemo $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/pushGossipDemo.c)
#add_executable(QueueTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/QueueTest.c)
#add_executable(randomSelectTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/randomSelectTest.c)
#add_executable(YggdrasilControlProcess $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/ControlStructureTest.c)
#add_executable(reliableTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/reliableTest.c)
#add_executable(pushsumTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/push_sum_test.c)
#add_executable(aggFlowTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/flow_updating_test.c)
#add_executable(drgTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/drg_test.c)
#add_executable(aggGapTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/gap_test.c)
#add_executable(aggGapBcastTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/gap_bcast_test.c)
#add_executable(aggLimoTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/limosense_test.c)
#add_executable(aggMultiTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/multi_root_test.c)

#add_executable(theREALthing $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/compileTest.c)
#add_executable(mirage $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/mirageTest.c)
#add_executable(json $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/jsonTest.c)
#add_executable(batman $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/routingTest.c)
#add_executable(batman_1 $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/UDPTest.c)
#add_executable(control $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/controlTest.c)
#add_executable(experience $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/toSendTest.c)
#add_executable(batman $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/toSendTest.c)
#add_executable(gap $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/gapTest.c)
#add_executable(messageCounter $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/messageCountApp.c)

#EXECUTABLES of ANDRE
#add_executable(myDiscovTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/discovery_test.c)
#add_executable(bcastv0 $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_v0_test.c)
#add_executable(bcastv1 $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_v1_test.c)
#add_executable(bcastv2 $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_v2_test.c)
#add_executable(bcastv4 $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_v4_test.c)
#add_executable(pg $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_gossip_test.c)

#-------------------------------------
#MY_TESTS

#file(GLOB TEST_COMMON ${PROJECT_SOURCE_DIR}/tests/myTests/bcast_test_common.c)
#add_library(test_common OBJECT ${TEST_COMMON})
#add_executable(bcast_v1_test $<TARGET_OBJECTS:lkprotos> $<TARGET_OBJECTS:lkcore> $<TARGET_OBJECTS:lkbaseobj> $<TARGET_OBJECTS:test_common> tests/myTests/bcast_v1_test.c)

#add_executable(discovery_test $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/discovery_test.c)
#add_executable(flooding_test $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_gossip_test.c)
#add_executable(flooding_rcvTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_gossip_rcvTest.c)
#add_executable(bcast_v0_test $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_v0_test.c)
#add_executable(bcast_v0_rcvTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_v0_rcvTest.c)
#add_executable(bcast_v1_test $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_v1_test.c)
#add_executable(bcast_v1_rcvTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_v1_rcvTest.c)
#add_executable(bcast_v2_test $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_v2_test.c)
#add_executable(bcast_v2_rcvTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_v2_rcvTest.c)
#add_executable(bcast_v4_test $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_v4_test.c)
#add_executable(bcast_v4_rcvTest $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_v4_rcvTest.c)

#add_executable(flooding_cache_test $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_gossip_cache_test.c)

#add_executable(bcast_generic_test $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/myTests/bcast_generic_test.c)

#-------------------------------------


#add executables of utils
#add_executable(aggtcpclient $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/aggregation_tcpclient.c)
#add_executable(tcpclient $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tests/tcpclient.c)

# add executables of tools
#add_executable(ControlProtocolClient $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tools/control_protocol_client.c)
#add_executable(cmdbuildtree $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tools/buildtree.c)
#add_executable(cmdchecktree $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tools/checktree.c)
#add_executable(cmdgetneighbors $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tools/getneighbors.c)
#add_executable(cmdexecuteexperience $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tools/beginexperience.c)
#add_executable(cmdterminateexperience $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tools/terminateexperience.c)
#add_executable(cmdchangevalue $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tools/changevalue.c)
#add_executable(cmdchangelink $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tools/changelink.c)
#add_executable(cmdreboot $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tools/reboot.c)
#add_executable(cmdshutdown $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tools/shutdown.c)
#add_executable(cmdenablediscovery $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tools/enableannounces.c)
#add_executable(cmddisablediscovery $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tools/disableannounces.c)
#add_executable(cmddebug $<TARGET_OBJECTS:yggprotos> $<TARGET_OBJECTS:yggcore> $<TARGET_OBJECTS:ygglllib_obj> tools/debugneighbors.c)
