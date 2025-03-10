if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  add_executable(buddy_test
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/tests_buddy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bitset.c
  )
  set(buddy_test_inc
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../common_cpp
  )
  target_include_directories(buddy_test PUBLIC
    "$<BUILD_INTERFACE:${buddy_test_inc}>"
  )
  target_compile_definitions(buddy_test PUBLIC DRAM)
  target_link_options(buddy_test PUBLIC "${PROJ_LINK_OPTIONS}")

endif()
