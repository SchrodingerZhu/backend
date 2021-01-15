cmake_minimum_required(VERSION 3.11)
project(virtual-cfg)
set(CMAKE_CXX_STANDARD 11)
add_library(vcfg src/virtual_mips.cpp)
include_directories(include)

