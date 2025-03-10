cmake_minimum_required(VERSION 3.21)

project(kernel_alloc_microbench)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_compile_options(${PROJ_COMPILE_OPTIONS})
add_link_options(${PROJ_LINK_OPTIONS} -latomic)

add_executable(kalloc_bench ${CMAKE_CURRENT_SOURCE_DIR}/allocator_bench.cpp)
target_link_libraries(kalloc_bench PRIVATE cxl_driver_api_byte)
