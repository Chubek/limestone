# Frontends — D language modules plus the C++ consumer interface.
#
# The compiled C++ frontend headers live in include/SatieFrontend*.hpp
# (built into the satie library via src/). This directory owns the D ports:
# installed as sources and verified by unittest + link smoke tests below
# whenever a D compiler is available.

add_library(satie_frontends INTERFACE)
add_library(satie::frontends ALIAS satie_frontends)
target_link_libraries(satie_frontends INTERFACE satie::satie)

set(SATIE_D_MODULES
  Common.d CNF.d IPASIR.d OPB.d SatieLisp.d SMT-LIB2.d WCNF.d)

install(FILES ${SATIE_D_MODULES} ipasir_smoke.d
  DESTINATION ${CMAKE_INSTALL_DATADIR}/satie/frontends)

# D verification (optional): unittest run over the pure modules plus an
# IPASIR link smoke test against the built static library.
find_program(D_COMPILER ldc2)
if(NOT D_COMPILER)
  find_program(D_COMPILER dmd)
endif()
if(NOT D_COMPILER)
  find_program(D_COMPILER gdc)
endif()

if(D_COMPILER AND BUILD_TESTING)
  add_test(NAME d_frontends_unittest
    COMMAND sh -c
      "${D_COMPILER} -unittest -main ${CMAKE_CURRENT_SOURCE_DIR}/Common.d ${CMAKE_CURRENT_SOURCE_DIR}/CNF.d ${CMAKE_CURRENT_SOURCE_DIR}/WCNF.d ${CMAKE_CURRENT_SOURCE_DIR}/OPB.d ${CMAKE_CURRENT_SOURCE_DIR}/SMT-LIB2.d ${CMAKE_CURRENT_SOURCE_DIR}/SatieLisp.d -of=${CMAKE_CURRENT_BINARY_DIR}/satie_d_unittest_run && ${CMAKE_CURRENT_BINARY_DIR}/satie_d_unittest_run"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  add_test(NAME d_ipasir_smoke
    COMMAND sh -c
      "${D_COMPILER} ${CMAKE_CURRENT_SOURCE_DIR}/IPASIR.d ${CMAKE_CURRENT_SOURCE_DIR}/ipasir_smoke.d $<TARGET_FILE:satie> -L-lstdc++ -of=${CMAKE_CURRENT_BINARY_DIR}/satie_ipasir_smoke && ${CMAKE_CURRENT_BINARY_DIR}/satie_ipasir_smoke"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
