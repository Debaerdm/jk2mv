# macos_gl_types.cmake - run by ctest (cmake -P): the renderer compiles
# without the PFNGL...PROC types that macOS doesn't have.
#
# macOS's <OpenGL/gl.h> covers GL 1.2 to 2.1: it defines GL_VERSION_1_2 to
# GL_VERSION_2_1 with those versions' enums and functions, but none of
# glext.h's PFNGL...PROC types (Apple names its own glXxxProcPtr). The bundled
# glext.h then skips those versions, types included, so a renderer source
# naming PFNGLGENQUERIESPROC builds on Linux and Windows and fails on macOS.
# Here the GL headers come first, as qgl.h includes them on Linux, then every
# PFNGL type of glext.h's GL 1.2 to 2.1 blocks is poisoned, and each renderer
# source is compiled (syntax only) after that.
#
# CONFIG: the file macos_gl_types_config.cmake written by CMakeLists.txt,
# with CXX, SOURCE_DIR, WORK_DIR and FLAGS

include("${CONFIG}")

file(READ "${SOURCE_DIR}/src/renderer/glext.h" glext)
string(FIND "${glext}" "#ifndef GL_VERSION_1_2" first)
string(FIND "${glext}" "#ifndef GL_VERSION_3_0" last)
if(first LESS 0 OR NOT last GREATER first)
  message(FATAL_ERROR "glext.h has no GL_VERSION_1_2 to GL_VERSION_2_1 blocks")
endif()
math(EXPR length "${last} - ${first}")
string(SUBSTRING "${glext}" ${first} ${length} legacy)
string(REGEX MATCHALL "PFNGL[A-Z0-9_]+PROC" hidden "${legacy}")
list(REMOVE_DUPLICATES hidden)
# qgl.h and tr_init.cpp name these two only off macOS (#if !defined(__APPLE__));
# there, qglTexImage3D is glTexImage3D itself
list(REMOVE_ITEM hidden PFNGLTEXIMAGE3DPROC PFNGLTEXSUBIMAGE3DPROC)
list(LENGTH hidden count)
if(count LESS 50)
  message(FATAL_ERROR "only ${count} PFNGL types found in glext.h's GL 1.2 to 2.1 blocks")
endif()

string(REPLACE ";" " " names "${hidden}")
set(prelude "${WORK_DIR}/macos_gl_prelude.h")
file(WRITE "${prelude}"
"// written by macos_gl_types.cmake: the GL headers as qgl.h includes them on
// Linux, then the PFNGL types that macOS's <OpenGL/gl.h> leaves out
#define GL_GLEXT_LEGACY
#include <GL/gl.h>
#include \"renderer/glext.h\"
#pragma GCC poison ${names}
")

file(GLOB sources "${SOURCE_DIR}/src/renderer/*.cpp")
set(failed "")
foreach(source ${sources})
  execute_process(
    COMMAND "${CXX}" ${FLAGS} -include "${prelude}" -fsyntax-only "${source}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output
  )
  if(NOT result EQUAL 0)
    get_filename_component(name "${source}" NAME)
    list(APPEND failed "${name}")
    message("${output}")
  endif()
endforeach()

list(LENGTH sources total)
if(total EQUAL 0)
  message(FATAL_ERROR "no renderer source in ${SOURCE_DIR}/src/renderer")
endif()
if(failed)
  message(FATAL_ERROR "with the ${count} PFNGL types of GL 1.2 to 2.1 poisoned (macOS has none of them), these renderer sources don't compile: ${failed}")
endif()
message(STATUS "${total} renderer sources build without the ${count} PFNGL types of GL 1.2 to 2.1")
