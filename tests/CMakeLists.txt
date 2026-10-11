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
  # Use POST_BUILD discovery mode to avoid running tests during cmake configuration
  gtest_discover_tests(${test_name}
    DISCOVERY_MODE POST_BUILD
  )
endfunction()

# Unit tests
add_subdirectory(unit)

# Every remaining use of the client and entity limit symbols in src/ is classified
# (tests/limits/allowlist.txt, docs/PROTOCOL-1.05.md step 1)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME limits_census
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/limits/limits_census.py ${CMAKE_SOURCE_DIR})
  # the census's own rules, on trees written for each case
  add_test(NAME limits_census_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/limits/test_limits_census.py)
else()
  message(STATUS "The limits census needs Python 3, skipping it")
endif()

# Smoke tests: the real engine binaries on a generated stub game directory
if(SmokeTests)
  add_subdirectory(smoke)
endif()

# Tests of the scripts in tools/
add_subdirectory(tools)

# Checks that the code builds on other platforms (macOS's GL headers)
add_subdirectory(portability)

# Performance tests (future)
# add_subdirectory(performance)
