cmake_minimum_required(VERSION 4.2)

find_package(SWIG REQUIRED)
include(UseSWIG)

set(CMAKE_SWIG_OUTDIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set_source_files_properties(libglr.i PROPERTIES CPLUSPLUS OFF)

swig_add_library(libglr_swig
    TYPE MODULE
    LANGUAGE python
    SOURCES libglr.i)

swig_link_libraries(libglr_swig PRIVATE libglr)
target_include_directories(${SWIG_MODULE_libglr_swig_REAL_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include)
