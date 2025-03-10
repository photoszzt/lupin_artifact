cmake_minimum_required (VERSION 3.5)
project (lupin_lock_benchmark VERSION 1.0 LANGUAGES CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration (Release/Debug)")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options("$<$<COMPILE_LANGUAGE:C>:-g>")
set(CMAKE_C_EXTENSIONS ON) #...with compiler extensions like gnu++11
set(CMAKE_CXX_EXTENSIONS ON) #...with compiler extensions like gnu++11
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Qunused-arguments")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pthread -fpermissive \
                    -fno-omit-frame-pointer -fPIC \
                    -Qunused-arguments -lnuma -latomic")
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif()

set(benchmark_include_dir ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/common
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/common_cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linux 
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/include
  ${CMAKE_CURRENT_SOURCE_DIR}/tatas_align)

# add_executable(cxl_test_rmcs cxl_test_rmcs.cpp)
# target_include_directories(cxl_test_rmcs PRIVATE ${benchmark_include_dir})
# target_link_libraries(cxl_test_rmcs PRIVATE ${LINK_LIBRARIES})

add_executable(dram_test_rmcs dram_test_rmcs.cpp)
target_include_directories(dram_test_rmcs PRIVATE ${benchmark_include_dir})
target_link_libraries(dram_test_rmcs PRIVATE ${LINK_LIBRARIES})

# add_executable(cxl_test_lpttas cxl_test_lpttas.cpp)
# target_include_directories(cxl_test_lpttas PRIVATE ${benchmark_include_dir})
# target_link_libraries(cxl_test_lpttas PRIVATE ${LINK_LIBRARIES})
# target_compile_definitions(cxl_test_lpttas PRIVATE BYTE_GRANULAR)

add_executable(dram_test_lpttas dram_test_lpttas.cpp)
target_include_directories(dram_test_lpttas PRIVATE ${benchmark_include_dir})
target_link_libraries(dram_test_lpttas PRIVATE ${LINK_LIBRARIES})
target_compile_definitions(dram_test_lpttas PRIVATE BYTE_GRANULAR)

add_executable(dram_test_ttas dram_test_ttas.cpp)
target_include_directories(dram_test_ttas PRIVATE ${benchmark_include_dir})

# add_executable(cxl_test_ttas cxl_test_ttas.cpp)
# target_include_directories(cxl_test_ttas PRIVATE ${benchmark_include_dir})
# target_link_libraries(cxl_test_ttas PRIVATE ${LINK_LIBRARIES})

add_executable(dram_test_osq dram_test_osq.cpp)
target_include_directories(dram_test_osq PRIVATE ${benchmark_include_dir})
target_link_libraries(dram_test_osq PRIVATE ${LINK_LIBRARIES})

add_executable(dram_test_qspin dram_test_qspin.cpp)
target_include_directories(dram_test_qspin PRIVATE ${benchmark_include_dir})
target_link_libraries(dram_test_qspin PRIVATE ${LINK_LIBRARIES})

# add_executable(cxl_test_qspin cxl_test_qspin.cpp)
# target_include_directories(cxl_test_qspin PRIVATE ${benchmark_include_dir})
# target_link_libraries(cxl_test_qspin PRIVATE ${LINK_LIBRARIES})

add_executable(dram_test_jj dram_test_jj.cpp)
target_include_directories(dram_test_jj PRIVATE ${benchmark_include_dir})
target_link_libraries(dram_test_jj PRIVATE ${LINK_LIBRARIES})
target_compile_definitions(dram_test_jj PRIVATE BYTE_GRANULAR)

# add_executable(cxl_test_jj cxl_test_jj.cpp)
# target_include_directories(cxl_test_jj PRIVATE ${benchmark_include_dir})
# target_link_libraries(cxl_test_jj PRIVATE ${LINK_LIBRARIES})
# target_compile_definitions(cxl_test_jj PRIVATE BYTE_GRANULAR)

add_executable(dram_test_jj_32port dram_test_jj_32port.cpp)
target_include_directories(dram_test_jj_32port PRIVATE ${benchmark_include_dir})
target_link_libraries(dram_test_jj_32port PRIVATE ${LINK_LIBRARIES})
target_compile_definitions(dram_test_jj_32port PRIVATE BYTE_GRANULAR)

# add_executable(cxl_test_jj_32port cxl_test_jj_32port.cpp)
# target_include_directories(cxl_test_jj_32port PRIVATE ${benchmark_include_dir})
# target_link_libraries(cxl_test_jj_32port PRIVATE ${LINK_LIBRARIES})
# target_compile_definitions(cxl_test_jj_32port PRIVATE BYTE_GRANULAR)
