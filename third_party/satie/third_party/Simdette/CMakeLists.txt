# ============================================================================
# Simdette CMake Configuration
# ============================================================================

cmake_minimum_required(VERSION 3.14)
project(Simdette
    VERSION 0.1.0
    DESCRIPTION "Header-only C++20 SIMD abstraction library"
    LANGUAGES CXX
)

# ============================================================================
# Options
# ============================================================================

option(SIMDETTE_BUILD_TESTS "Build test suite" OFF)
option(SIMDETTE_BUILD_EXAMPLES "Build example programs" OFF)
option(SIMDETTE_BUILD_BENCHMARKS "Build benchmark programs" OFF)
option(SIMDETTE_BUILD_DOCS "Build documentation" ON)
option(SIMDETTE_INSTALL "Install library and documentation" ON)
option(SIMDETTE_INSTALL_DOCS "Install generated documentation" ON)

# ============================================================================
# Project Configuration
# ============================================================================

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific settings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
endif()

# ============================================================================
# Library Target
# ============================================================================

# Add interface library (header-only)
add_library(simdette INTERFACE)
add_library(Simdette::simdette ALIAS simdette)

# Include directories
target_include_directories(simdette INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Compile features
target_compile_features(simdette INTERFACE cxx_std_20)

# Compiler flags for SIMD
target_compile_options(simdette INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-msse2;-mavx2;-mavx512f>
)

# ============================================================================
# Tests (optional)
# ============================================================================

if(SIMDETTE_BUILD_TESTS)
    enable_testing()

    add_subdirectory(tests)

    # Find test executables
    file(GLOB TEST_EXES "${CMAKE_BINARY_DIR}/tests/test_*")

    # Register each test
    foreach(TEST_EXE ${TEST_EXES})
        get_filename_component(TEST_NAME ${TEST_EXE} NAME_WE)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_EXE})
    endforeach()
endif()

# ============================================================================
# Examples (optional)
# ============================================================================

if(SIMDETTE_BUILD_EXAMPLES)
    file(GLOB EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")

    foreach(EXAMPLE_SRC ${EXAMPLE_SOURCES})
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_SRC} NAME_WE)

        add_executable(${EXAMPLE_NAME} ${EXAMPLE_SRC})
        target_link_libraries(${EXAMPLE_NAME} PRIVATE simdette)

        # Copy to build directory for easy access
        set_target_properties(${EXAMPLE_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
        )
    endforeach()
endif()

# ============================================================================
# Benchmarks (optional)
# ============================================================================

if(SIMDETTE_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# ============================================================================
# Documentation
# ============================================================================

if(SIMDETTE_BUILD_DOCS)
    find_program(DOXYGEN_EXECUTABLE doxygen)

    set(SIMDETTE_DOXYGEN_INPUT_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    set(SIMDETTE_DOXYGEN_MANUAL_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/manual"
    )

    set(SIMDETTE_DOXYGEN_WORK_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/docs"
    )

    set(SIMDETTE_DOXYGEN_OUTPUT_DIR
        "${SIMDETTE_DOXYGEN_WORK_DIR}/doxygen"
    )

    if(DOXYGEN_EXECUTABLE)
        file(MAKE_DIRECTORY "${SIMDETTE_DOXYGEN_WORK_DIR}")

        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in"
            "${SIMDETTE_DOXYGEN_WORK_DIR}/Doxyfile"
            @ONLY
        )

        add_custom_command(
            OUTPUT
                "${SIMDETTE_DOXYGEN_OUTPUT_DIR}/html/index.html"
            COMMAND
                ${DOXYGEN_EXECUTABLE}
                "${SIMDETTE_DOXYGEN_WORK_DIR}/Doxyfile"
            WORKING_DIRECTORY
                "${SIMDETTE_DOXYGEN_WORK_DIR}"
            DEPENDS
                "${SIMDETTE_DOXYGEN_WORK_DIR}/Doxyfile"
                "${CMAKE_CURRENT_SOURCE_DIR}/docs/manual/index.md"
            COMMENT
                "Generating Doxygen documentation"
            VERBATIM
        )

        add_custom_target(doc
            DEPENDS
                "${SIMDETTE_DOXYGEN_OUTPUT_DIR}/html/index.html"
        )

        add_custom_target(docs
            DEPENDS doc
        )
    else()
        message(WARNING "Doxygen not found: documentation targets will be unavailable")
    endif()
endif()

# ============================================================================
# Installation
# ============================================================================

if(SIMDETTE_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install headers
    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT simdette_dev
    )

    # Install CMake config
    install(TARGETS simdette
        EXPORT SimdetteTargets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT SimdetteTargets
        FILE SimdetteTargets.cmake
        NAMESPACE Simdette::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Simdette
        COMPONENT simdette_dev
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/SimdetteConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SimdetteConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/SimdetteConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Simdette
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/SimdetteConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/SimdetteConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Simdette
        COMPONENT simdette_dev
    )

    # Install documentation
    if(SIMDETTE_BUILD_DOCS)
        install(DIRECTORY docs/manual/
            DESTINATION ${CMAKE_INSTALL_DOCDIR}
            COMPONENT docs
        )

        if(
            SIMDETTE_INSTALL_DOCS
            AND DOXYGEN_EXECUTABLE
            AND EXISTS "${SIMDETTE_DOXYGEN_OUTPUT_DIR}/html"
        )
            install(
                DIRECTORY
                    "${SIMDETTE_DOXYGEN_OUTPUT_DIR}/html/"
                DESTINATION
                    ${CMAKE_INSTALL_DOCDIR}/api
                COMPONENT
                    docs
            )
        endif()
    endif()
endif()

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "Simdette ${PROJECT_VERSION} Configuration Summary")
message(STATUS "================================================")
message(STATUS "  Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build tests:       ${SIMDETTE_BUILD_TESTS}")
message(STATUS "  Build examples:    ${SIMDETTE_BUILD_EXAMPLES}")
message(STATUS "  Build benchmarks:  ${SIMDETTE_BUILD_BENCHMARKS}")
message(STATUS "  Build docs:        ${SIMDETTE_BUILD_DOCS}")
message(STATUS "  Install:           ${SIMDETTE_INSTALL}")
message(STATUS "  Compiler:          ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "================================================")
message(STATUS "")
