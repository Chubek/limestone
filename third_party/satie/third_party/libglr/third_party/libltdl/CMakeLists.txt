cmake_minimum_required(VERSION 3.15)

project(libltdl C)

set(CMAKE_C_STANDARD 99)

# Core sources
set(LTDL_CORE
    ltdl.c
    lt_dlloader.c
    lt_error.c
    lt__alloc.c
    lt__argz.c
    lt__dirent.c
    lt__strl.c
    slist.c
)

# Platform loaders
set(LTDL_LOADERS)

if(UNIX AND NOT APPLE)
    list(APPEND LTDL_LOADERS loaders/dlopen.c)
endif()

if(APPLE)
    list(APPEND LTDL_LOADERS loaders/dyld.c)
endif()

if(WIN32)
    list(APPEND LTDL_LOADERS loaders/loadlibrary.c)
endif()

# Optional legacy loaders
option(LTDL_ENABLE_SHL_LOAD "Enable HP-UX shl_load loader" OFF)
if(LTDL_ENABLE_SHL_LOAD)
    list(APPEND LTDL_LOADERS loaders/shl_load.c)
endif()

option(LTDL_ENABLE_DLD "Enable GNU dld loader" OFF)
if(LTDL_ENABLE_DLD)
    list(APPEND LTDL_LOADERS loaders/dld_link.c)
endif()

option(LTDL_ENABLE_BEOS "Enable BeOS loader" OFF)
if(LTDL_ENABLE_BEOS)
    list(APPEND LTDL_LOADERS loaders/load_add_on.c)
endif()

# Preopen loader (portable fallback)
list(APPEND LTDL_LOADERS loaders/preopen.c)

add_library(ltdl
    ${LTDL_CORE}
    ${LTDL_LOADERS}
)

target_include_directories(ltdl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libltdl>
        $<INSTALL_INTERFACE:include>
)

# libdl if required
if(UNIX AND NOT APPLE)
    find_library(DL_LIBRARY dl)
    if(DL_LIBRARY)
        target_link_libraries(ltdl PRIVATE ${DL_LIBRARY})
    endif()
endif()

set_target_properties(ltdl PROPERTIES
    OUTPUT_NAME ltdl
    VERSION 2.4
    SOVERSION 7
)

# Installation
include(GNUInstallDirs)

install(TARGETS ltdl
    EXPORT ltdlTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    FILES
        ltdl.h
        libltdl/lt_system.h
        libltdl/lt_dlloader.h
        libltdl/lt_error.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libltdl
)

install(EXPORT ltdlTargets
    FILE ltdlTargets.cmake
    NAMESPACE ltdl::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ltdl
)
