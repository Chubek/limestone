cmake_minimum_required(VERSION 3.10)

project(EqvsgProject LANGUAGES C CXX)

# Optimization level similar to OPT in the script
set(OPT_LEVEL 2 CACHE STRING "Optimization level (like gcc -O<level>)")
add_compile_options(-O${OPT_LEVEL})

set(QAMRPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/QaMRpp)
include_directories(${QAMRPP_DIR})

# Resolve HOME for install destinations
if(NOT DEFINED ENV{HOME})
    message(FATAL_ERROR "HOME environment variable is not set")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs/CMakeLists.txt")
    add_subdirectory(docs)
else()
    message(STATUS "Skipping docs: docs/CMakeLists.txt not found")
endif()

set(QAMRPP_LIB_INSTALL_DIR "$ENV{HOME}/.qamrpp/eqvsglib")
set(EQVSG_BIN_INSTALL_DIR "$ENV{HOME}/.local/bin")
set(EQVSG_INCLUDE_INSTALL_DIR "$ENV{HOME}/.local/include")

# Shared library: libqamrpp-sexpr.so
add_library(qamrpp-sexpr SHARED
    ${QAMRPP_DIR}/QaMRpp-Library.c
    ${CMAKE_CURRENT_SOURCE_DIR}/qamrpp-lib/S-Expression.c
)

set_target_properties(qamrpp-sexpr PROPERTIES
    OUTPUT_NAME "qamrpp-sexpr"
    POSITION_INDEPENDENT_CODE ON
)

# Shared library: libqamrpp-leqvsg.so
add_library(qamrpp-leqvsg SHARED
    ${QAMRPP_DIR}/QaMRpp-Library.c
    ${CMAKE_CURRENT_SOURCE_DIR}/qamrpp-lib/leqvsg.c
)

set_target_properties(qamrpp-leqvsg PROPERTIES
    OUTPUT_NAME "qamrpp-leqvsg"
    POSITION_INDEPENDENT_CODE ON
)

# Executable: eqvsg
add_executable(eqvsg
    ${CMAKE_CURRENT_SOURCE_DIR}/EQVSG.cpp
)

target_include_directories(eqvsg PRIVATE ${QAMRPP_DIR})

# Install rules
install(TARGETS qamrpp-sexpr qamrpp-leqvsg
    LIBRARY DESTINATION "${QAMRPP_LIB_INSTALL_DIR}"
)

install(TARGETS eqvsg
    RUNTIME DESTINATION "${EQVSG_BIN_INSTALL_DIR}"
)

file(GLOB PROJECT_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
if(PROJECT_HEADERS)
    install(FILES ${PROJECT_HEADERS}
        DESTINATION "${EQVSG_INCLUDE_INSTALL_DIR}"
    )
endif()
