cmake_minimum_required(VERSION 3.16)
project(nuperf VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/Dependencies.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(NUPERF_BUILD_TESTS "Build tests" OFF)
option(NUPERF_BUILD_FUZZERS "Build fuzzers" OFF)
option(NUPERF_BUILD_LUA "Build Lua bindings" OFF)
option(NUPERF_BUILD_SWIG_BINDINGS "Build SWIG bindings" OFF)
option(NUPERF_BUILD_EXAMPLES "Build examples" OFF)
option(NUPERF_BUILD_CLI "Build CLI application" ON)
option(NUPERF_BUILD_DOCS "Build documentation target" ON)
option(NUPERF_INSTALL_DOCS "Install generated documentation" ON)
option(NUPERF_INSTALL_STDMETH "Install stdmeth plugin sources" ON)
option(NUPERF_INSTALL_STDDEF "Install stddef plugin sources" ON)

# Core library sources
set(NUPERF_CORE_SOURCES
    src/core/api.cpp
    src/core/builder.cpp
    src/core/keyset.cpp
    src/util/hash.cpp
    src/util/io.cpp
)

# Plugin sources
set(NUPERF_PLUGIN_SOURCES
    src/plugins/stddef.cpp
    src/plugins/stdmeth.cpp
)

# Create main library
add_library(nuperf ${NUPERF_CORE_SOURCES} ${NUPERF_PLUGIN_SOURCES})
add_library(nuperf::nuperf ALIAS nuperf)

target_include_directories(nuperf
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(nuperf PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -pedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

if(NUPERF_BUILD_DOCS)
    add_subdirectory(docs)
endif()

# CLI application
if(NUPERF_BUILD_CLI)
    add_executable(nuperf-cli src/main.cpp)
    target_link_libraries(nuperf-cli PRIVATE nuperf)
    if(TARGET nuperf_cli11)
        target_link_libraries(nuperf-cli PRIVATE nuperf_cli11)
    endif()
    set_target_properties(nuperf-cli PROPERTIES OUTPUT_NAME nuperf)
endif()

# Lua bindings
if(NUPERF_BUILD_LUA)
    find_package(Lua REQUIRED)
    
    add_library(lnuperf MODULE
        src/lua/lnuperf.cpp
        src/lua/lua-bindings.cpp
    )
    
    target_link_libraries(lnuperf PRIVATE nuperf ${LUA_LIBRARIES})
    target_include_directories(lnuperf PRIVATE ${LUA_INCLUDE_DIR})
    
    # Use vendored sol2 if available
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sol2/include)
        if(TARGET nuperf_sol2)
            target_link_libraries(lnuperf PRIVATE nuperf_sol2)
        else()
            target_include_directories(lnuperf PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sol2/include
            )
        endif()
    endif()
    
    set_target_properties(lnuperf PROPERTIES
        PREFIX ""
        OUTPUT_NAME "lnuperf"
    )
endif()

# Tests
if(NUPERF_BUILD_TESTS)
    enable_testing()
    
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Catch2)
        add_subdirectory(third_party/Catch2)
    else()
        find_package(Catch2 REQUIRED)
    endif()
    
    file(GLOB TEST_SOURCES tests/*.cpp)
    if(TEST_SOURCES)
        add_executable(nuperf-tests ${TEST_SOURCES})
        target_link_libraries(nuperf-tests PRIVATE nuperf Catch2::Catch2)
        add_test(NAME nuperf-tests COMMAND nuperf-tests)
    endif()
endif()

# Examples
if(NUPERF_BUILD_EXAMPLES)
    file(GLOB EXAMPLE_SOURCES examples/*.c examples/*.cpp)
    foreach(example_src ${EXAMPLE_SOURCES})
        get_filename_component(example_name ${example_src} NAME_WE)
        add_executable(${example_name} ${example_src})
        target_link_libraries(${example_name} PRIVATE nuperf)
    endforeach()
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/nuperf.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/nuperf.pc
    @ONLY
)

# Installation
install(TARGETS nuperf
    EXPORT nuperfTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/nuperf
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/nuperf.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

if(NUPERF_BUILD_CLI)
    install(TARGETS nuperf-cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(EXPORT nuperfTargets
    FILE nuperfTargets.cmake
    NAMESPACE nuperf::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nuperf
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/nuperfConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/nuperfConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nuperf
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/nuperfConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/nuperfConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/nuperfConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/nuperf
)

if(NUPERF_INSTALL_STDMETH)
    install(DIRECTORY stdmeth/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/nuperf/stdmeth
        FILES_MATCHING PATTERN "*.cpp"
    )
endif()

if(NUPERF_INSTALL_STDDEF)
    install(DIRECTORY stddef/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/nuperf/stddef
        FILES_MATCHING PATTERN "*.cpp"
    )
endif()
