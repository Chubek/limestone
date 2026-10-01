cmake_minimum_required(VERSION 3.18)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(DomPyBind VERSION 1.0.0 LANGUAGES C CXX)
endif()
find_package(Python 3.13 REQUIRED COMPONENTS Interpreter Development)
file(GLOB _dompybind_q CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.q"
  "${CMAKE_CURRENT_SOURCE_DIR}/../domqlib/q/*.py")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include/dompybind/capi.h"
  COMMAND "${Python_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate.py"
    "${CMAKE_CURRENT_BINARY_DIR}/include/dompybind/capi.h"
  DEPENDS generate.py ${_dompybind_q} VERBATIM)
add_custom_target(dompybind_headers ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/include/dompybind/capi.h")
add_library(dompybind INTERFACE)
set_target_properties(dompybind PROPERTIES EXPORT_NAME DomPyBind)
add_library(DomPyBind::DomPyBind ALIAS dompybind)
add_dependencies(dompybind dompybind_headers)
target_compile_features(dompybind INTERFACE cxx_std_17)
target_include_directories(dompybind INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(dompybind INTERFACE Python::Python)
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/dompybind/capi.h"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dompybind)
install(TARGETS dompybind EXPORT DomPyBindTargets)
install(EXPORT DomPyBindTargets NAMESPACE DomPyBind:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DomPyBind)
install(FILES cmake/DomPyBindConfig.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DomPyBind)
