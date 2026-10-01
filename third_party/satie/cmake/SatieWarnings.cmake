# SatieWarnings.cmake — strict warning set shared by library targets.
#
# Usage:
#   include(SatieWarnings)
#   satie_apply_warnings(<target>)

function (satie_apply_warnings target)
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options (${target} PRIVATE -Wall -Wextra -Wpedantic)
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options (${target} PRIVATE /W4)
  endif ()
endfunction ()
