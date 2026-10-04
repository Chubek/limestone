# Frontends — compiled D modules plus the C++ consumer interface.
# C++ implementations remain part of satie; the D archive is built whenever
# a D compiler is available, independently of BUILD_TESTING.

set(SATIE_D_MODULES
  Common.d CNF.d IPASIR.d OPB.d SatieLisp.d SMT-LIB2.d WCNF.d)
set(SATIE_D_MODULE_NAMES
  common cnf ipasir opb satielisp smt_lib2 wcnf)

install(FILES ${SATIE_D_MODULES} ipasir_smoke.d
  DESTINATION ${CMAKE_INSTALL_DATADIR}/satie/frontends)

# Single-module compilation and D consumers need the import paths to match
# the module declarations (satie.common, satie.smt_lib2, and so on).
set(satie_d_sources)
foreach(index RANGE 0 6)
  list(GET SATIE_D_MODULES ${index} source)
  list(GET SATIE_D_MODULE_NAMES ${index} module_name)
  set(module_source "${CMAKE_CURRENT_BINARY_DIR}/satie/${module_name}.d")
  configure_file("${source}" "${module_source}" COPYONLY)
  list(APPEND satie_d_sources "${module_source}")
endforeach()
install(FILES ${satie_d_sources}
  DESTINATION ${CMAKE_INSTALL_DATADIR}/satie/frontends/satie)

find_program(D_COMPILER NAMES ldc2 dmd gdc)
set(satie_d_default OFF)
if(D_COMPILER)
  set(satie_d_default ON)
endif()
option(BUILD_D_FRONTENDS "Build the D frontend library" ${satie_d_default})

if(BUILD_D_FRONTENDS)
  if(NOT D_COMPILER)
    message(FATAL_ERROR "BUILD_D_FRONTENDS requires ldc2, dmd, or gdc; set D_COMPILER to its path")
  endif()
  execute_process(COMMAND "${D_COMPILER}" --version
    OUTPUT_VARIABLE d_version ERROR_VARIABLE d_version_error
    RESULT_VARIABLE d_version_result)
  if(NOT d_version_result EQUAL 0)
    message(FATAL_ERROR "Cannot run D_COMPILER: ${d_version_error}")
  endif()
  if(d_version MATCHES "gdc.*GCC")
    set(d_output_prefix -o)
    set(d_unittest_flags -funittest -fmain)
    set(d_link_prefix "")
  elseif(d_version MATCHES "LDC|DMD")
    set(d_output_prefix -of=)
    set(d_unittest_flags -unittest -main)
    set(d_link_prefix -L)
  else()
    message(FATAL_ERROR "Unsupported D_COMPILER: ${d_version}")
  endif()
  message(STATUS "Building D frontends with ${D_COMPILER}")

  set(satie_d_objects)
  foreach(index RANGE 0 6)
    list(GET SATIE_D_MODULE_NAMES ${index} module_name)
    list(GET satie_d_sources ${index} module_source)
    set(object "${CMAKE_CURRENT_BINARY_DIR}/${module_name}${CMAKE_CXX_OUTPUT_EXTENSION}")
    add_custom_command(OUTPUT "${object}"
      COMMAND "${D_COMPILER}" -c "-I${CMAKE_CURRENT_BINARY_DIR}"
        "${module_source}" "${d_output_prefix}${object}"
      DEPENDS ${satie_d_sources} "${D_COMPILER}"
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
      COMMENT "Compiling D frontend satie.${module_name}"
      VERBATIM)
    list(APPEND satie_d_objects "${object}")
  endforeach()
  set_source_files_properties(${satie_d_objects} PROPERTIES
    GENERATED TRUE EXTERNAL_OBJECT TRUE)
  add_library(satie_frontends STATIC ${satie_d_objects})
  set_target_properties(satie_frontends PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries(satie_frontends PUBLIC satie::satie)
else()
  message(STATUS "D frontend library disabled (BUILD_D_FRONTENDS=OFF)")
  add_library(satie_frontends INTERFACE)
  target_link_libraries(satie_frontends INTERFACE satie::satie)
endif()
add_library(satie::frontends ALIAS satie_frontends)
set_target_properties(satie_frontends PROPERTIES EXPORT_NAME frontends)
install(TARGETS satie_frontends EXPORT SatieTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(BUILD_D_FRONTENDS AND BUILD_TESTING)
  set(satie_d_pure_sources ${satie_d_sources})
  list(REMOVE_ITEM satie_d_pure_sources "${CMAKE_CURRENT_BINARY_DIR}/satie/ipasir.d")
  set(d_unittest_binary "${CMAKE_CURRENT_BINARY_DIR}/satie_d_unittest_run${CMAKE_EXECUTABLE_SUFFIX}")
  add_custom_command(OUTPUT "${d_unittest_binary}"
    COMMAND "${D_COMPILER}" ${d_unittest_flags} ${satie_d_pure_sources}
      "${d_output_prefix}${d_unittest_binary}"
    DEPENDS ${satie_d_pure_sources} "${D_COMPILER}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Building D frontend unittests"
    VERBATIM)
  add_custom_target(d_frontends_unittest ALL DEPENDS "${d_unittest_binary}")
  add_test(NAME d_frontends_unittest COMMAND "${d_unittest_binary}")

  set(d_cpp_runtime stdc++)
  if("c++" IN_LIST CMAKE_CXX_IMPLICIT_LINK_LIBRARIES)
    set(d_cpp_runtime c++)
  endif()
  set(d_smoke_binary "${CMAKE_CURRENT_BINARY_DIR}/satie_ipasir_smoke${CMAKE_EXECUTABLE_SUFFIX}")
  add_custom_command(OUTPUT "${d_smoke_binary}"
    COMMAND "${D_COMPILER}" "-I${CMAKE_CURRENT_BINARY_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}/ipasir_smoke.d"
      "$<TARGET_FILE:satie_frontends>" "$<TARGET_FILE:satie>"
      "$<TARGET_FILE:satie_glr>" "${d_link_prefix}-l${d_cpp_runtime}"
      "${d_output_prefix}${d_smoke_binary}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ipasir_smoke.d"
      satie_frontends satie satie_glr "${D_COMPILER}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Building D IPASIR link smoke test"
    VERBATIM)
  add_custom_target(d_ipasir_smoke ALL DEPENDS "${d_smoke_binary}")
  add_test(NAME d_ipasir_smoke COMMAND "${d_smoke_binary}")
endif()
