cmake_minimum_required(VERSION 3.16)
project(libglr VERSION 1.0.0 LANGUAGES C)
include(GNUInstallDirs)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
find_package(Threads REQUIRED)

# Options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_DOCUMENTATION "Build documentation" ON)
option(EMBED_MD_DOCS "Embed and install markdown documentation" ON)
option(BUILD_DISAMBSTD "Build the standard disambiguation library" ON)
option(BUILD_REWRITELIB "Install the standard rewrite library" ON)
option(BUILD_SWIG_BINDINGS "Build SWIG bindings when SWIG is available" OFF)
option(ENABLE_TEST_SANITIZERS "Enable address/undefined sanitizers for tests" OFF)
option(BUILD_SHAREDLIB "Build libglr as a shared library" OFF)
option(INSTALL_LOCAL "Install libglr into ~/.local instead of /usr/local" OFF)
option(ENABLE_CACHE "Enable libmdbx-based incremental parsing cache" ON)
option(INSTALL_GLRpp "Enable installation of GLRpp" ON)

set(HAVE_LMDB 0)
set(HAVE_LIBMDBX 0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(INSTALL_LOCAL)
    if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
        set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Install prefix" FORCE)
    else()
        message(FATAL_ERROR "INSTALL_LOCAL=ON requires the HOME environment variable to be set.")
    endif()
endif()

if(ENABLE_CACHE)
    # libglr vendors libmdbx as a library only. Its own test/examples
    # subdirectory registers ctest entries (c_api, c++_api) for binaries
    # that are never built under EXCLUDE_FROM_ALL, which would leave
    # permanently "Not Run" entries in this project's ctest run.
    set(MDBX_ENABLE_TESTS OFF CACHE BOOL "Disable libmdbx self-tests" FORCE)
    add_subdirectory(third_party/libmdbx EXCLUDE_FROM_ALL)

    if(TARGET mdbx-static)
        set(HAVE_LMDB 1)
        set(HAVE_LIBMDBX 1)
        set(LIBMDBX_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libmdbx)
        set(LIBMDBX_LIBRARY mdbx-static)
    elseif(TARGET mdbx)
        set(HAVE_LMDB 1)
        set(HAVE_LIBMDBX 1)
        set(LIBMDBX_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libmdbx)
        set(LIBMDBX_LIBRARY mdbx)
    else()
        message(WARNING "libmdbx target not available. Incremental parsing cache will be disabled.")
    endif()

    # Find OpenSSL for SHA-256
    find_package(OpenSSL)
    if(HAVE_LMDB AND OpenSSL_FOUND)
        message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
    endif()
else()
    message(STATUS "Incremental cache disabled via ENABLE_CACHE=OFF")
endif()

if(ENABLE_CACHE AND HAVE_LMDB AND NOT OpenSSL_FOUND)
    message(WARNING "OpenSSL not found. Incremental parsing cache will be disabled.")
    set(HAVE_LMDB 0)
    set(HAVE_LIBMDBX 0)
endif()

# Bundle the local S-expression parser used by the rewrite subsystem.
file(GLOB SFSEXP_SOURCES CONFIGURE_DEPENDS
    third_party/sfsexp/src/*.c
)
add_library(sfsexp STATIC ${SFSEXP_SOURCES})
target_include_directories(sfsexp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/sfsexp/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/libglr/third_party/sfsexp>)
target_compile_options(sfsexp PRIVATE -w)
set_target_properties(sfsexp PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
install(TARGETS sfsexp
    EXPORT ${PROJECT_NAME}-exports
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY third_party/sfsexp/src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libglr/third_party/sfsexp
    FILES_MATCHING PATTERN "*.h")

# Library sources
#
# The incremental-support sources that do not require libmdbx
# (diff, serialization, forest-merge, dependency, parser-incr) are always
# built so glr_parser_parse_incremental() is available in every
# configuration. Only cache.c needs the libmdbx backing store.
set(GLR_SOURCES
    src/glr/grammar.c
    src/glr/stack.c
    src/glr/fork.c
    src/glr/forest.c
    src/glr/reduction.c
    src/glr/graph.c
    src/glr/parser.c
    src/glr/parsetbl.c
    src/glr/lrtable.c
    src/glr/disambiguate.c
    src/glr/lexer-hooks.c
    src/glr/reader.c
    src/glr/rewrite.c
    src/glr/diff.c
    src/glr/serialization.c
    src/glr/forest-merge.c
    src/glr/dependency.c
    src/glr/parser-incr.c
    src/glr/live-parsing.c
    src/glr/querylisp.c
    src/glr/queryexec.c
    src/glr/scannerless.c
    src/glr/select.c
    src/glr/semantic-action.c
    src/glr/stringpool.c
    src/glr/thread.c
    third_party/klib/kalloc.c
    third_party/klib/kthread.c
)

set_source_files_properties(third_party/klib/kalloc.c third_party/klib/kthread.c
    PROPERTIES COMPILE_OPTIONS "-w")

# Add cache sources if LMDB is available
if(HAVE_LMDB)
    list(APPEND GLR_SOURCES
        src/glr/cache.c
    )
endif()

# Create library
set(GLR_LIBRARY_KIND STATIC)
if(BUILD_SHAREDLIB)
    set(GLR_LIBRARY_KIND SHARED)
endif()
add_library(${PROJECT_NAME} ${GLR_LIBRARY_KIND} ${GLR_SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME glr
)

# Configure header with HAVE_LMDB
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/glr/config.h"
)

# Set include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ctl
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ctl/ctl)
target_link_libraries(${PROJECT_NAME} PUBLIC sfsexp Threads::Threads)

if(ENABLE_TEST_SANITIZERS)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${PROJECT_NAME} PUBLIC -fsanitize=address,undefined)
endif()

# Link libmdbx and OpenSSL if available
if(HAVE_LMDB)
    target_include_directories(${PROJECT_NAME} PRIVATE ${LIBMDBX_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBMDBX_LIBRARY})
    if(OpenSSL_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::Crypto)
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        HAVE_LMDB=1
        HAVE_LIBMDBX=1)
    # EXCLUDE_FROM_ALL also excludes the vendored installation rules. Export
    # the cache dependency with relocatable paths for static-library clients.
    set_property(TARGET ${LIBMDBX_LIBRARY} PROPERTY INTERFACE_INCLUDE_DIRECTORIES
        $<BUILD_INTERFACE:${LIBMDBX_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/libglr/third_party/mdbx>)
    get_target_property(GLR_MDBX_PUBLIC_HEADERS ${LIBMDBX_LIBRARY} PUBLIC_HEADER)
    set(GLR_MDBX_INSTALL_HEADERS)
    foreach(GLR_MDBX_HEADER IN LISTS GLR_MDBX_PUBLIC_HEADERS)
        get_filename_component(GLR_MDBX_HEADER "${GLR_MDBX_HEADER}" ABSOLUTE
            BASE_DIR "${LIBMDBX_INCLUDE_DIR}")
        list(APPEND GLR_MDBX_INSTALL_HEADERS "${GLR_MDBX_HEADER}")
    endforeach()
    set_property(TARGET ${LIBMDBX_LIBRARY} PROPERTY PUBLIC_HEADER
        "${GLR_MDBX_INSTALL_HEADERS}")
    install(TARGETS ${LIBMDBX_LIBRARY}
        EXPORT ${PROJECT_NAME}-exports
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libglr/third_party/mdbx)
endif()

# Set compile options
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)

# Create alias for use in subdirectories
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# Install targets
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-exports
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(INSTALL_LOCAL)
    install(CODE "file(MAKE_DIRECTORY \"$ENV{HOME}/.local/lib\")")
endif()

install(FILES
    include/glr/glr.h
    include/glr/grammar.h
    include/glr/stack.h
    include/glr/fork.h
    include/glr/forest.h
    include/glr/reduction.h
    include/glr/graph.h
    include/glr/parser.h
    include/glr/disambiguate.h
    include/glr/lexer-hooks.h
    include/glr/parsetbl.h
    include/glr/reader.h
    include/glr/rewrite.h
    include/glr/dependency.h
    include/glr/diff.h
    include/glr/forest-merge.h
    include/glr/serialization.h
    include/glr/live-parsing.h
    include/glr/query.h
    include/glr/scannerless.h
    include/glr/select.h
    include/glr/semantic-action.h
    include/glr/stringpool.h
    include/glr/thread.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/glr
)

# Install cache header and config if LMDB is available
if(HAVE_LMDB)
    install(FILES
        include/glr/cache.h
        ${CMAKE_CURRENT_BINARY_DIR}/include/glr/config.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/glr
    )
endif()

# Documentation generation and optional Markdown installation
if(BUILD_DOCUMENTATION)
    add_subdirectory(docs)
endif()

if(INSTALL_GLRpp)
   add_subdirectory(GLRpp)
endif()

if(EMBED_MD_DOCS)
    install(DIRECTORY docs/
        DESTINATION share/doc/${PROJECT_NAME}/markdown
        FILES_MATCHING
            PATTERN "*.md"
            PATTERN "build" EXCLUDE
    )
endif()

# Export targets
install(EXPORT ${PROJECT_NAME}-exports
    FILE ${PROJECT_NAME}-targets.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

if(BUILD_DISAMBSTD)
    add_subdirectory(disambstd)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_REWRITELIB)
    add_subdirectory(rewritelib)
endif()

if(BUILD_SWIG_BINDINGS)
    add_subdirectory(bindings)
endif()

# Generate package config file
include(CMakePackageConfigHelpers)

configure_package_config_file(cmake/libglr-config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/libglr-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libglr)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libglr-config.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libglr)
install(FILES third_party/klib/LICENSE.txt third_party/ctl/LICENSE
    DESTINATION share/doc/libglr/licenses)

set(GLR_PC_PRIVATE_LIBS "-lsfsexp -pthread")
if(HAVE_LMDB)
    get_target_property(GLR_MDBX_OUTPUT_NAME ${LIBMDBX_LIBRARY} OUTPUT_NAME)
    string(APPEND GLR_PC_PRIVATE_LIBS " -l${GLR_MDBX_OUTPUT_NAME} -lcrypto")
endif()
file(RELATIVE_PATH GLR_PC_PREFIX_RELATIVE
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    "${CMAKE_INSTALL_PREFIX}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libglr.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libglr.pc"
    @ONLY
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libglr.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
