cmake_minimum_required(VERSION 3.11)
project(backend)
set(CMAKE_CXX_STANDARD 11)
include_directories(include)

add_subdirectory(parallel-hashmap)
include_directories(parallel-hashmap/parallel_hashmap)

add_library(gcolor STATIC src/heap.cpp src/graph.cpp)
add_library(vcfg SHARED src/virtual_mips.cpp)
add_executable(draft tests/test.cpp)
add_executable(test_module tests/test_module.cpp)
add_executable(heap_test tests/heap_test.cpp)
add_executable(color_test tests/color_test.cpp)

enable_testing()
add_test(heap_test heap_test)
add_test(color_test color_test)

target_link_libraries(heap_test gcolor)
target_link_libraries(color_test gcolor)
target_link_libraries(vcfg gcolor)
target_link_libraries(draft vcfg)
target_link_libraries(test_module vcfg)