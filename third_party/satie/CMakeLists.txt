cmake_minimum_required(VERSION 3.16)

project(Satie
  VERSION 0.1.0
  LANGUAGES C CXX
  DESCRIPTION "Small, header-only, multi-algorithm SAT solver")

# ------------------------------------------------------------
# Global settings
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Default install locations (overridable via GNUInstallDirs).
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(SatieWarnings)
include(SatieVersion)
include(SatieInstall)

# ------------------------------------------------------------
# Options
# ------------------------------------------------------------
option(INSTALL_CLI   "Build and install the satie CLI/REPL"            ON)
option(BUILD_TESTING "Build the Catch2 unit-test suite"                ON)
option(BUILD_EXAMPLES "Build the example programs"                     ON)
option(GENERATE_DOCS "Generate Doxygen documentation"                  OFF)
option(BUILD_STDPLUGIN "Build the standard Satie plugins"              ON)

# Registered before any subdirectory so add_test() calls in examples,
# frontends, and tests all take effect.
if(BUILD_TESTING)
  enable_testing()
endif()

# ------------------------------------------------------------
# Engine library  (namespaced: satie::satie)
# ------------------------------------------------------------
add_library(satie STATIC)
add_library(satie::satie ALIAS satie)

target_include_directories(satie
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(satie PUBLIC cxx_std_20)
target_compile_options(satie PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wpedantic>)

# Headers are part of the target for IDE/Doxygen discovery.
file(GLOB SATIE_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)
target_sources(satie PRIVATE ${SATIE_HEADERS})

add_subdirectory(src)

# ------------------------------------------------------------
# Subdirectories (conditional)
# ------------------------------------------------------------
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(INSTALL_CLI)
  add_subdirectory(cli)
endif()

add_subdirectory(frontends)
add_subdirectory(stdlib)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

if(BUILD_STDPLUGIN)
  add_subdirectory(stdplugin)
endif()

add_subdirectory(distrib)
add_subdirectory(docs)
add_subdirectory(cmake)

# ------------------------------------------------------------
# Install rules for the library
# ------------------------------------------------------------
install(TARGETS satie
  EXPORT SatieTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

# Ship only bundled dependency headers that actually exist in this archive.
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/QaMRpp/include/QaMRpp.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/QaMRpp/include/QaMRpp-Plugin.hpp
  DESTINATION third_party/QaMRpp/include)

install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/QaMRpp/qlib/C/QaMRpp-Library.h
  DESTINATION third_party/QaMRpp/qlib/C)

install(EXPORT SatieTargets
  FILE SatieTargets.cmake
  NAMESPACE satie::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Satie)

satie_install_package_config ()
