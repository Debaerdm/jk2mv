# tests/CMakeLists.txt - Google Test configuration for JK2MV server tests

cmake_minimum_required(VERSION 3.14)

# Google Test setup
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()
include(GoogleTest)

# Test utilities library (header-only)
add_library(test_utils INTERFACE)
target_include_directories(test_utils INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/server
)

# Common test settings
function(add_jk2mv_test test_name)
  add_executable(${test_name} ${ARGN})
  target_link_libraries(${test_name}
    PRIVATE
    GTest::gtest_main
    test_utils
  )
  target_compile_definitions(${test_name} PRIVATE
    ${GlobalDefines}
    ${DebugDefines}
  )
  gtest_discover_tests(${test_name})
endfunction()

# Unit tests
add_subdirectory(unit)

# Integration tests (future)
# add_subdirectory(integration)

# Performance tests (future)
# add_subdirectory(performance)
