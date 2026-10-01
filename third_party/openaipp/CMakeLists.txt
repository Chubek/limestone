
cmake_minimum_required(VERSION 3.20)

project(openaipp VERSION 0.1.0 LANGUAGES CXX)

option(OPENAIPP_ENABLE_OPENSSL "Enable HTTPS support via OpenSSL (cpp-httplib)" ON)
option(OPENAIPP_BUILD_SMOKE "Build a small compile-only smoke test" OFF)
option(OPENAIPP_BUILD_DOCS "Build the documentation and manual" OFF)

add_library(openaipp INTERFACE)
add_library(openaipp::openaipp ALIAS openaipp)

target_compile_features(openaipp INTERFACE cxx_std_20)

target_include_directories(
  openaipp
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-httplib>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>
    $<INSTALL_INTERFACE:include>
)

set(OPENAIPP_WITH_OPENSSL OFF)

if(OPENAIPP_ENABLE_OPENSSL)
  find_package(OpenSSL QUIET)
  if(OpenSSL_FOUND)
    set(OPENAIPP_WITH_OPENSSL ON)
    target_compile_definitions(openaipp INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(openaipp INTERFACE OpenSSL::SSL OpenSSL::Crypto)
  else()
    message(WARNING "OpenSSL not found; building without CPPHTTPLIB_OPENSSL_SUPPORT (HTTPS may not work)")
  endif()
endif()

if(OPENAIPP_BUILD_SMOKE)
  add_executable(openaipp_smoke ${CMAKE_CURRENT_SOURCE_DIR}/.agents/smoke.cpp)
  target_link_libraries(openaipp_smoke PRIVATE openaipp)
endif()

include(GNUInstallDirs)

if (OPENAIPP_BUILD_DOCS)
   add_subdirectory(docs)
endif()

install(TARGETS openaipp EXPORT openaippTargets)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-httplib/httplib.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/MetaTk/DSLtk DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/MetaTk)

install(
  EXPORT openaippTargets
  FILE openaippTargets.cmake
  NAMESPACE openaipp::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/openaipp
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/openaippConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/openaippConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/openaippConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/openaipp
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/openaippConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/openaippConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/openaipp
)
