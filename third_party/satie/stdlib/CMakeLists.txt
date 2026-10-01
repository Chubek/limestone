# Satie standard library — header-only C++ helpers plus Lua modules.
#
# C++: each `satie::stdlib::*` module is one header (`arith/Arith.hpp`,
# ...). Consumers link `satie::stdlib`, which puts this directory on the
# include path, so `#include "arith/Arith.hpp"` (or the `Stdlib.hpp`
# umbrella) resolves in-tree. Installed headers mirror the same layout
# under `${CMAKE_INSTALL_INCLUDEDIR}/satie/stdlib`.
#
# Lua: each module with a `.lua` file is a dependency-free chunk returning
# its table (the QaMRpp dialect cannot `require`, so files are loaded by
# path via `satie::stdlib::import::load_lua_file` or inlined by tests).
# Installed Lua preserves the source tree under
# `${CMAKE_INSTALL_DATADIR}/satie/stdlib`.

add_library(satie_stdlib INTERFACE)
add_library(satie::stdlib ALIAS satie_stdlib)
target_include_directories(satie_stdlib INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/satie/stdlib>)
target_link_libraries(satie_stdlib INTERFACE satie::satie)

install(FILES Stdlib.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/satie/stdlib)
install(DIRECTORY
  arith array constants datalog encoding ffi fmt graphlib graphviz html
  import io json latex list log logsynth map openai os path regex sched
  simplifier sys variables yaml
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/satie/stdlib
  FILES_MATCHING PATTERN "*.hpp")
install(DIRECTORY
  arith array constants datalog encoding fmt graphlib graphviz html
  json latex list log logsynth map openai sched simplifier sys variables yaml
  DESTINATION ${CMAKE_INSTALL_DATADIR}/satie/stdlib
  FILES_MATCHING PATTERN "*.lua")
