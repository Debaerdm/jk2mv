# Checks that something builds on another platform, run where developers
# build. Each one is a ctest test labelled portability.

# macos_gl_types: the renderer compiles without the PFNGL types of GL 1.2 to
# 2.1, which macOS's <OpenGL/gl.h> leaves out (see macos_gl_types.cmake).
# Needs GCC or Clang (#pragma GCC poison) and GL headers like Linux's: on
# macOS the build itself uses the real ones, and Windows' gl.h stops at GL
# 1.1, so glext.h declares every type there.
if(APPLE OR WIN32 OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  message(STATUS "The macos_gl_types test needs GCC or Clang off macOS and Windows, skipping it")
  return()
endif()
find_package(OpenGL)
if(NOT OPENGL_FOUND)
  message(STATUS "The macos_gl_types test needs the OpenGL headers, skipping it")
  return()
endif()

set(flags -std=gnu++11 -w)
foreach(define ${GlobalDefines})
  list(APPEND flags "-D${define}")
endforeach()
# tr_image.cpp includes <jpeglib.h> and <png.h>: the bundled ones, which macOS
# builds with by default, so that the test needs no libjpeg or libpng package
# whether the client is built or not (tests.yml doesn't build it)
set(dirs ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR} ${OPENGL_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS} ${MINIZIP_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/libs/jpeg-9a/include ${CMAKE_SOURCE_DIR}/libs/libpng/include)
list(REMOVE_DUPLICATES dirs)
foreach(dir ${dirs})
  # not /usr/include: the compiler searches it anyway, after its own headers
  if(NOT dir IN_LIST CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
    list(APPEND flags "-I${dir}")
  endif()
endforeach()

set(config "${CMAKE_CURRENT_BINARY_DIR}/macos_gl_types_config.cmake")
file(WRITE "${config}"
  "set(CXX \"${CMAKE_CXX_COMPILER}\")\n"
  "set(SOURCE_DIR \"${CMAKE_SOURCE_DIR}\")\n"
  "set(WORK_DIR \"${CMAKE_CURRENT_BINARY_DIR}\")\n"
  "set(FLAGS \"${flags}\")\n"
)
add_test(NAME macos_gl_types
  COMMAND ${CMAKE_COMMAND} -DCONFIG=${config} -P ${CMAKE_CURRENT_SOURCE_DIR}/macos_gl_types.cmake)
set_tests_properties(macos_gl_types PROPERTIES LABELS portability TIMEOUT 300)
