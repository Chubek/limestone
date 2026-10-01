# Examples — compiled automatically by the root CMakeLists.txt.
# Each C++ example links the engine library; DSL text files are data only.

add_executable(example_basic_solve  API/basic_solve.cpp)
target_link_libraries(example_basic_solve  PRIVATE satie::satie)

add_executable(example_build_ast   NativeDSL/build_ast.cpp)
target_link_libraries(example_build_ast   PRIVATE satie::satie)

add_executable(example_frontends Frontends/demo.cpp)
target_link_libraries(example_frontends PRIVATE satie::satie)
add_custom_target(example_frontends_data ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/Frontends
    ${CMAKE_CURRENT_BINARY_DIR}/Frontends)

if(BUILD_TESTING)
  add_test(NAME example_frontends
    COMMAND example_frontends ${CMAKE_CURRENT_BINARY_DIR}/Frontends)
endif()

# DSL text examples (*.satie) are not compiled; install them as data so
# users can load them via the REPL/CLI.
install(FILES DSL/nqueens.satie DSL/pigeonhole.satie
  DESTINATION ${CMAKE_INSTALL_DATADIR}/satie/examples)

install(DIRECTORY Lua Frontends
  DESTINATION ${CMAKE_INSTALL_DATADIR}/satie/examples)
