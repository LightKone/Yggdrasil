# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(remote_control_applications)

include_directories(../../)
include_directories(../../../Yggdrasil-LowLevelLib)


add_executable(YggdrasilControlProcess ControlStructure.c)
target_link_libraries(YggdrasilControlProcess control_tree control_server control_discovery)




#tools
add_executable(cmdbuildtree tools/buildtree.c)
target_link_libraries(cmdbuildtree cmdio)
add_executable(cmdchecktree tools/checktree.c)
target_link_libraries(cmdchecktree cmdio)
add_executable(cmdgetneighbors tools/getneighbors.c)
target_link_libraries(cmdgetneighbors cmdio)
add_executable(cmdexecuteexperience tools/beginexperience.c)
target_link_libraries(cmdexecuteexperience cmdio)
add_executable(cmdterminateexperience tools/terminateexperience.c)
target_link_libraries(cmdterminateexperience cmdio)
add_executable(cmdchangevalue tools/changevalue.c)
target_link_libraries(cmdchangevalue cmdio)
add_executable(cmdchangelink tools/changelink.c)
target_link_libraries(cmdchangelink cmdio)
add_executable(cmdreboot tools/reboot.c)
target_link_libraries(cmdreboot cmdio)
add_executable(cmdshutdown tools/shutdown.c)
target_link_libraries(cmdshutdown cmdio)
add_executable(cmdenablediscovery tools/enableannounces.c)
target_link_libraries(cmdenablediscovery cmdio)
add_executable(cmddisablediscovery tools/disableannounces.c)
target_link_libraries(cmddisablediscovery cmdio)
add_executable(cmddebug tools/debugneighbors.c)
target_link_libraries(cmddebug cmdio)


