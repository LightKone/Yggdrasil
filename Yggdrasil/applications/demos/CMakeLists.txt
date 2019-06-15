# Specify the minimum version for CMake

cmake_minimum_required(VERSION 2.8)

# Project's name
project(Demos)

include_directories(../../)

add_executable(MessageExchange MessageExchange.c)
target_link_libraries(MessageExchange yggcore_wireless)

add_executable(OneHopBcast OneHopBcast.c)
target_link_libraries(OneHopBcast yggcore_wireless)

add_executable(OneHopBcastWithTimer OneHopBcastWithTimer.c)
target_link_libraries(OneHopBcastWithTimer yggcore_wireless)

