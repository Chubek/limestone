# Unit-test suite — self-contained Catch2-compatible runner.
# tests/catch_shim.hpp provides the macro surface so no external Catch2
# dependency or network fetch is required.

add_executable(satie_tests
  satie_tests.cpp
  catch_shim.hpp
  test_common.hpp
  test_dsltk.hpp
  test_plugins.hpp
  test_solvers.hpp
  test_capi.hpp
  test_module.hpp
  test_dt.hpp
  test_frontends.hpp
  test_stdlib.hpp)

target_link_libraries(satie_tests PRIVATE satie::satie satie::stdlib)
target_include_directories(satie_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(satie_tests PRIVATE
  SATIE_STDLIB_DIR="${CMAKE_SOURCE_DIR}/stdlib")

# Compile flags inherited from the satie target; keep tests strict too.
target_compile_options(satie_tests PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wpedantic>)

include(CTest)
add_test(NAME satie_tests COMMAND satie_tests)

# SAT core tests can run even when optional plugin headers are unavailable.
add_executable(satie_sat_tests satie_sat_tests.cpp test_theory.hpp)
target_link_libraries(satie_sat_tests PRIVATE satie::satie)
add_test(NAME satie_sat_tests COMMAND satie_sat_tests)

# GCC's optimized PMR devirtualization used to produce false array-bounds
# warnings. Keep the reproducing allocation patterns covered at compile time.
add_executable(satie_memory_tests satie_memory_tests.cpp)
target_link_libraries(satie_memory_tests PRIVATE satie::satie)
satie_apply_warnings(satie_memory_tests)
target_compile_options(satie_memory_tests PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:-Werror=array-bounds>)
add_test(NAME satie_memory_tests COMMAND satie_memory_tests)
