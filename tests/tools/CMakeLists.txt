# Tests of the scripts in tools/. jk2bench.ps1 runs a fake game, a shell
# script: PowerShell 7 (pwsh) off Windows only.

find_package(Python3 COMPONENTS Interpreter)
find_program(PWSH_EXECUTABLE pwsh)
if(WIN32 OR NOT Python3_Interpreter_FOUND OR NOT PWSH_EXECUTABLE)
  message(STATUS "The jk2bench.ps1 test needs pwsh and Python 3 off Windows, skipping it")
  return()
endif()

add_test(NAME tools_jk2bench
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/jk2bench_test.py
    ${PWSH_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/bench/jk2bench.ps1 ${CMAKE_CURRENT_BINARY_DIR}/jk2bench)
set_tests_properties(tools_jk2bench PROPERTIES LABELS tools TIMEOUT 300)
