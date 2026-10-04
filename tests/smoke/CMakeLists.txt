# Engine smoke tests: start the real binaries on a generated stub game
# directory (see make_stub_base.py). Linux only: the client test relies on
# SDL's offscreen video driver and Mesa's software OpenGL.

find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_Interpreter_FOUND OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(STATUS "Smoke tests need Python 3 on Linux, skipping them")
  return()
endif()
# the stub game directory takes the game module from assetsmv2.pk3
if(NOT TARGET assetsmv2)
  message(STATUS "Smoke tests need the mvsdk assets (BuildMVSDK), skipping them")
  return()
endif()

if(TARGET ${MVDED})
  add_test(NAME smoke_server
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/server_smoke.py
      $<TARGET_FILE:${MVDED}> $<TARGET_FILE_DIR:${MVDED}> ${CMAKE_CURRENT_BINARY_DIR}/server)
  set_tests_properties(smoke_server PROPERTIES LABELS smoke TIMEOUT 400)
endif()

if(TARGET ${MVMP})
  add_test(NAME smoke_client
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/client_smoke.py
      $<TARGET_FILE:${MVMP}> $<TARGET_FILE_DIR:${MVMP}> ${CMAKE_CURRENT_BINARY_DIR}/client)
  set_tests_properties(smoke_client PROPERTIES LABELS smoke TIMEOUT 300)
endif()

if(TARGET ${MVMP})
  add_test(NAME smoke_render
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/render_smoke.py
      $<TARGET_FILE:${MVMP}> $<TARGET_FILE_DIR:${MVMP}> ${CMAKE_CURRENT_BINARY_DIR}/render)
  set_tests_properties(smoke_render PROPERTIES LABELS smoke TIMEOUT 600)
endif()
