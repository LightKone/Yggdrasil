
cmake_minimum_required(VERSION 2.8)

# Project's name
project(Utilities)

add_library(bloomfilters bloomfilter/bloom.c)

add_library(hashfunctions hashfunctions.c)

add_library(Json cJSON.c)