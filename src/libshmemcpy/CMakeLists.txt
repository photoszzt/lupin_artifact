project(libshmemcpy)
cmake_minimum_required(VERSION 3.21)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration (Release/Debug)")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build configuration: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install directory: ${CMAKE_INSTALL_PREFIX}")

set(LIBRARY_OUTPUT_PATH    ${CMAKE_BINARY_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

# Always include debug info
add_compile_options("$<$<COMPILE_LANGUAGE:C>:-g>")
set(CMAKE_C_EXTENSIONS ON) #...with compiler extensions like gnu++11
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(shmemcpy_source
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/out.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/memops_generic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/cpu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memcpy/memcpy_nt_avx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memcpy/memcpy_nt_sse2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memset/memset_nt_avx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memset/memset_nt_sse2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memcpy/memcpy_t_avx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memcpy/memcpy_t_sse2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memset/memset_t_avx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memset/memset_t_sse2.c
)

include(CheckCSourceRuns)

set(CMAKE_REQUIRED_FLAGS)
set(CMAKE_REQUIRED_FLAGS "-mavx512f")
check_c_source_runs("
    #include <immintrin.h>
    #include <stdint.h>
    int main(){
        uint64_t v[8];
        __m512i zmm0 = _mm512_loadu_si512((__m512i *)&v);
        return 0;
    }"
    AVX512F_AVAILABLE)

set(CMAKE_REQUIRED_FLAGS "-mmovdir64b")
check_c_source_runs("
    #include <immintrin.h>
    #include <stdint.h>

    int main(){ 
        uint64_t v, w; 
        _movdir64b(&v, &w); 
        return 0;
    }"
    MOVDIR64B_AVAILABLE)

if(${AVX512F_AVAILABLE})
    set(shmemcpy_source ${shmemcpy_source}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memcpy/memcpy_nt_avx512f.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memset/memset_nt_avx512f.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memcpy/memcpy_t_avx512f.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memset/memset_t_avx512f.c)
endif()

if (${MOVDIR64B_AVAILABLE})
    set(shmemcpy_source ${shmemcpy_source}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memcpy/memcpy_nt_movdir64b.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64/memset/memset_nt_movdir64b.c)
endif()

add_library(shmemcpy STATIC ${shmemcpy_source})

target_include_directories(shmemcpy PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/x86_64;${CMAKE_CURRENT_SOURCE_DIR}/src/include/internal>;${CMAKE_CURRENT_SOURCE_DIR}/src/include;${CMAKE_CURRENT_SOURCE_DIR}/../common"
)

# no -Wvla for clang; clang doesn't have the permit small options
# the assert macro uses vla
if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(COMPILE_OPTIONS -Werror -Walloca -Wformat=2 
        -Wformat-security -Wnull-dereference
       -Wstack-protector -Warray-bounds -Warray-bounds-pointer-arithmetic -Wassign-enum
       -Wbad-function-cast -Wconditional-uninitialized -Wconversion -Wfloat-equal -Wformat-type-confusion
       -Widiomatic-parentheses -Wimplicit-fallthrough -Wloop-analysis -Wpointer-arith -Wshift-sign-overflow
       -Wshorten-64-to-32 -Wswitch-enum -Wtautological-constant-in-range-compare
       -Wunreachable-code-aggressive -Wthread-safety -Wthread-safety-beta -Wcomma -D_FORTIFY_SOURCE=3
       -fPIE -fsanitize=bounds -fsanitize-undefined-trap-on-error
    )
elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(COMPILE_OPTIONS -Werror -Wall -Wextra -Wpedantic -Wformat=2 -Wformat-overflow=2
        -Wformat-truncation=2 -Wformat-security -Wnull-dereference -Wstack-protector -Wtrampolines
        -Walloca -Wvla-larger-than=1048576 -Warray-bounds=2 -Wimplicit-fallthrough=3 -Wshift-overflow=2
        -Wstringop-overflow=4 -Wconversion -Warith-conversion 
        -Wlogical-op -Wduplicated-cond -Wduplicated-branches
        -Wformat-signedness -Wstrict-overflow=4 -Wundef -Wstrict-prototypes -Wswitch-default
        -Wswitch-enum -Wstack-usage=1000000 
        -fPIE -fsanitize=bounds -fsanitize-undefined-trap-on-error
        -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -Wl,-z,separate-code)
endif()
set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -mavx -DVALGRIND_ENABLED=0 
    -DVG_MEMCHECK_ENABLED=0 -DVG_DRD_ENABLED=0 -DVG_HELGRIND_ENABLED=0 -DVG_PMEMCHECK_ENABLED=0)
if (${MOVDIR64B_AVAILABLE})
    set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -mavx -mavx512f -mmovdir64b -DMOVDIR64B_AVAILABLE=1)
else()
    set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -DMOVDIR64B_AVAILABLE=0)
endif()
if(${AVX512F_AVAILABLE})
    set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -mavx -mavx512f -DAVX512F_AVAILABLE=1)
else()
    set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -DAVX512F_AVAILABLE=0)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
        set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -fsanitize=address 
            -fsanitize=leak -fno-omit-frame-pointer 
            -fsanitize=undefined  -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow 
            -fsanitize=integer)
    elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -fsanitize=address 
            -fsanitize=pointer-compare -fsanitize=pointer-subtract 
            -fsanitize=leak -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=bounds-strict 
            -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow)
    endif()
else()
    set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -O3 -flto)
endif()
# set(COMPILE_OPTIONS ${COMPILE_OPTIONS} -O3 -flto)

set(LINK_OPTIONS -flto -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -Wl,-z,separate-code)
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(LINK_OPTIONS ${LINK_OPTIONS} -fsanitize=address 
      -fsanitize=leak -fno-omit-frame-pointer 
      -fsanitize=undefined  -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow 
      -fsanitize=integer)
  elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(LINK_OPTIONS ${LINK_OPTIONS} -fsanitize=address 
      -fsanitize=pointer-compare -fsanitize=pointer-subtract 
      -fsanitize=leak -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=bounds-strict 
      -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow)
  endif()
endif()

target_compile_options(shmemcpy PUBLIC "$<$<CONFIG:Debug>:${COMPILE_OPTIONS}>")
target_compile_options(shmemcpy PUBLIC "$<$<CONFIG:Release>:${COMPILE_OPTIONS}>")
target_link_options(shmemcpy PUBLIC "$<$<CONFIG:Debug>:${LINK_OPTIONS}>")
target_link_options(shmemcpy PUBLIC "$<$<CONFIG:Release>:${LINK_OPTIONS}>")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(unittest_lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test/unittest/ut_alloc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test/unittest/ut_file.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test/unittest/ut_backtrace.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test/unittest/ut.c
    )
    add_library(unittest ${unittest_lib})
    target_include_directories(unittest PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/test/unittest;${CMAKE_CURRENT_SOURCE_DIR}/src/include/internal;${CMAKE_CURRENT_SOURCE_DIR}/../common>"
    )
    target_compile_options(unittest PUBLIC "$<$<CONFIG:Debug>:${COMPILE_OPTIONS}>")
    target_compile_options(unittest PUBLIC "$<$<CONFIG:Release>:${COMPILE_OPTIONS}>")

    add_executable(shmem_memcpy 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test/memcpy/memcpy_common.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test/memcpy/shmem_memcpy.c
    )
    target_include_directories(shmem_memcpy PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/test/unittest;${CMAKE_CURRENT_SOURCE_DIR}/src/include/internal;${CMAKE_CURRENT_SOURCE_DIR}/../common>"
    )
    target_compile_options(shmem_memcpy PUBLIC "$<$<CONFIG:Debug>:${COMPILE_OPTIONS}>")
    target_compile_options(shmem_memcpy PUBLIC "$<$<CONFIG:Release>:${COMPILE_OPTIONS}>")
    target_link_libraries(shmem_memcpy PUBLIC unittest shmemcpy)
    target_link_options(shmem_memcpy PUBLIC "$<$<CONFIG:Debug>:${LINK_OPTIONS}>")
    target_link_options(shmem_memcpy PUBLIC "$<$<CONFIG:Release>:${LINK_OPTIONS}>")

    add_executable(shmem_memmove 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test/memmove/memmove_common.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test/memmove/shmem_memmove.c
    )
    target_include_directories(shmem_memmove PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/test/unittest;${CMAKE_CURRENT_SOURCE_DIR}/src/include/internal;${CMAKE_CURRENT_SOURCE_DIR}/../common>"
    )
    target_compile_options(shmem_memmove PUBLIC "$<$<CONFIG:Debug>:${COMPILE_OPTIONS}>")
    target_compile_options(shmem_memmove PUBLIC "$<$<CONFIG:Release>:${COMPILE_OPTIONS}>")
    target_link_libraries(shmem_memmove PUBLIC unittest shmemcpy)
    target_link_options(shmem_memmove PUBLIC "$<$<CONFIG:Debug>:${LINK_OPTIONS}>")
    target_link_options(shmem_memmove PUBLIC "$<$<CONFIG:Release>:${LINK_OPTIONS}>")

    add_executable(shmem_memset 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test/memset/memset_common.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test/memset/shmem_memset.c
    )
    target_include_directories(shmem_memset PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/test/unittest;${CMAKE_CURRENT_SOURCE_DIR}/src/include/internal;${CMAKE_CURRENT_SOURCE_DIR}/../common>"
    )
    target_compile_options(shmem_memset PUBLIC "$<$<CONFIG:Debug>:${COMPILE_OPTIONS}>")
    target_compile_options(shmem_memset PUBLIC "$<$<CONFIG:Release>:${COMPILE_OPTIONS}>")
    target_link_libraries(shmem_memset PUBLIC unittest shmemcpy)
    target_link_options(shmem_memset PUBLIC "$<$<CONFIG:Debug>:${LINK_OPTIONS}>")
    target_link_options(shmem_memset PUBLIC "$<$<CONFIG:Release>:${LINK_OPTIONS}>")

    add_executable(shmem_movnt 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test/movnt/shmem_movnt.c
    )
    target_include_directories(shmem_movnt PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/test/unittest;${CMAKE_CURRENT_SOURCE_DIR}/src/include/internal;${CMAKE_CURRENT_SOURCE_DIR}/../common>"
    )
    target_compile_options(shmem_movnt PUBLIC "$<$<CONFIG:Debug>:${COMPILE_OPTIONS}>")
    target_compile_options(shmem_movnt PUBLIC "$<$<CONFIG:Release>:${COMPILE_OPTIONS}>")
    target_link_libraries(shmem_movnt PUBLIC unittest shmemcpy)
    target_link_options(shmem_movnt PUBLIC "$<$<CONFIG:Debug>:${LINK_OPTIONS}>")
    target_link_options(shmem_movnt PUBLIC "$<$<CONFIG:Release>:${LINK_OPTIONS}>")

    add_executable(shmem_movnt_align 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test/movnt_align/shmem_movnt_align.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test/movnt_align/movnt_align_common.c
    )
    target_include_directories(shmem_movnt_align PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/test/unittest;${CMAKE_CURRENT_SOURCE_DIR}/src/include/internal;${CMAKE_CURRENT_SOURCE_DIR}/../common>"
    )
    target_compile_options(shmem_movnt_align PUBLIC "$<$<CONFIG:Debug>:${COMPILE_OPTIONS}>")
    target_compile_options(shmem_movnt_align PUBLIC "$<$<CONFIG:Release>:${COMPILE_OPTIONS}>")
    target_link_libraries(shmem_movnt_align PUBLIC unittest shmemcpy)
    target_link_options(shmem_movnt_align PUBLIC "$<$<CONFIG:Debug>:${LINK_OPTIONS}>")
    target_link_options(shmem_movnt_align PUBLIC "$<$<CONFIG:Release>:${LINK_OPTIONS}>")
endif()
