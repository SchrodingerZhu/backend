cmake_minimum_required(VERSION 3.11)
project(backend)
set(CMAKE_CXX_STANDARD 11)
include_directories(include)

add_library(gcolor src/heap.cpp src/graph.cpp)
add_library(vcfg src/virtual_mips.cpp)
add_executable(draft tests/test.cpp)
add_executable(heap_test tests/heap_test.cpp)
add_executable(color_test tests/color_test.cpp)

enable_testing()
add_test(heap_test heap_test)
add_test(color_test color_test)

target_link_libraries(heap_test gcolor)
target_link_libraries(color_test gcolor)
target_link_libraries(draft vcfg gcolor)