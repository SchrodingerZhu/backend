cmake_minimum_required(VERSION 3.11)
project(graph_coloring)

set(CMAKE_CXX_STANDARD 11)

add_library(gcolor STATIC src/heap.cpp src/graph.cpp)

add_executable(heap_test tests/heap_test.cpp)
add_executable(color_test tests/color_test.cpp)

include_directories(include)

enable_testing()
add_test(heap_test heap_test)
add_test(color_test color_test)

target_link_libraries(heap_test gcolor)
target_link_libraries(color_test gcolor)
