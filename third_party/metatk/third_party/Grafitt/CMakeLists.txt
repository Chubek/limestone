cmake_minimum_required(VERSION 3.20)

project(Grafitt
    VERSION 0.4.0
    DESCRIPTION "Header-first C++20 graph toolkit inspired by OCamlGraph"
    LANGUAGES C CXX
)

include(GNUInstallDirs)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
# Grafitt is a header-only library, so the public headers are always exposed.
# The two optional integrations (MetaTk DSLtk and SerdeTk) are auto-detected
# at compile time via __has_include inside Grafitt.hpp, mirroring the
# single-header drop-in experience. These options let a consuming CMake
# project pull the dependencies in explicitly (e.g. when bundling them as
# subdirectories under third_party/).
option(GRAFITT_USE_DSLTK "Link MetaTk DSLtk for Queryfitt PEG parsing / pattern matching" ON)
option(GRAFITT_USE_SERDETK "Expose SerdeTk on the include path (reserved; Grafitt does not yet consume it)" OFF)
option(GRAFITT_BUILD_EXAMPLES "Build the Grafitt example programs" OFF)
option(GRAFITT_INSTALL "Generate Grafitt install rules" ON)

# ---------------------------------------------------------------------------
# Core interface library
# ---------------------------------------------------------------------------
add_library(Grafitt INTERFACE)
add_library(Grafitt::Grafitt ALIAS Grafitt)

target_include_directories(Grafitt
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(Grafitt INTERFACE cxx_std_20)

# ---------------------------------------------------------------------------
# Optional dependency wiring
# ---------------------------------------------------------------------------
# The presence of the third_party subdirectories is the canonical way to make
# DSLtk/SerdeTk visible. If they are absent, Grafitt still compiles; the
# optional features simply degrade to their stubs (see Grafitt.hpp).
set(_grafitt_extra_cflags "")

if(GRAFITT_USE_DSLTK)
    if(TARGET DSLtk)
        # Already defined (e.g. add_subdirectory(third_party/MetaTk/DSLtk)).
        target_link_libraries(Grafitt INTERFACE DSLtk)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/MetaTk/DSLtk/DSLtk.hpp")
        # Put the MetaTk root (NOT just DSLtk/) on the path so the qualified
        # "MetaTk/DSLtk/DSLtk.hpp" include in Grafitt.hpp resolves. The
        # qualified path is preferred because some vendored trees ship a
        # bare "DSLtk.hpp" stub that forwards to IPCtk/DSLUtils.hpp, which
        # lacks the PEG API Grafitt relies on.
        target_include_directories(Grafitt INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/MetaTk>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/MetaTk/DSLtk>
        )
        set(_grafitt_extra_cflags "${_grafitt_extra_cflags} -DGRAFITT_HAS_DSLTK=1")
    endif()
endif()

if(GRAFITT_USE_SERDETK)
    # Off by default: SerdeTk.hpp transitively pulls in a bundled "DSLtk.hpp"
    # stub that can shadow the real MetaTk DSLtk and break Grafitt's PEG
    # machinery. Grafitt does not yet consume SerdeTk, so this is reserved
    # for future schema hooks. Enable only when you know your SerdeTk bundle
    # resolves the real DSLtk (e.g. MetaTk/DSLtk is also on the path).
    if(TARGET SerdeTk)
        target_link_libraries(Grafitt INTERFACE SerdeTk)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SerdeTk/include/SerdeTk.hpp")
        target_include_directories(Grafitt INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/SerdeTk/include>
        )
        set(_grafitt_extra_cflags "${_grafitt_extra_cflags} -DGRAFITT_HAS_SERDETK=1")
    endif()
endif()

# ---------------------------------------------------------------------------
# Examples (opt-in)
# ---------------------------------------------------------------------------
if(GRAFITT_BUILD_EXAMPLES)
    enable_testing()
    file(GLOB _grafitt_examples CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")
    foreach(_ex ${_grafitt_examples})
        get_filename_component(_name ${_ex} NAME_WE)
        add_executable(${_name} ${_ex})
        target_link_libraries(${_name} PRIVATE Grafitt)
        target_compile_features(${_name} PRIVATE cxx_std_20)
    endforeach()

    # Plain-C module example.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/module-minimal.c")
        add_executable(grafitt-module-minimal examples/module-minimal.c)
        target_include_directories(grafitt-module-minimal PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include")
    endif()
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------
if(GRAFITT_INSTALL)
    install(
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
    )

    install(
        TARGETS Grafitt
        EXPORT GrafittTargets
    )

    install(
        EXPORT GrafittTargets
        FILE GrafittTargets.cmake
        NAMESPACE Grafitt::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Grafitt
    )

    include(CMakePackageConfigHelpers)

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GrafittConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/GrafittConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Grafitt
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/GrafittConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/GrafittConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/GrafittConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Grafitt
    )

    # pkg-config file
    set(GRAFITT_PKGCONFIG_EXTRA_CFLAGS "${_grafitt_extra_cflags}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/grafitt.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/grafitt.pc"
        @ONLY
    )
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/grafitt.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
endif()
