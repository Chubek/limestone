cmake_minimum_required(VERSION 3.15)
project(GLRpp VERSION 1.0.0 LANGUAGES CXX)

# Header-only library
add_library(GLRpp INTERFACE)
target_include_directories(GLRpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(GLRpp INTERFACE cxx_std_20)

include(CTest)
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    find_package(Catch2 2 QUIET)
endif()
if(BUILD_TESTING AND Catch2_FOUND AND TARGET libglr)
    add_executable(glrpp_test_glrpp tests/unittest/test_glrpp.cpp)
    if(TARGET Catch2::Catch2WithMain)
        target_link_libraries(glrpp_test_glrpp PRIVATE GLRpp libglr Catch2::Catch2WithMain)
    else()
        target_link_libraries(glrpp_test_glrpp PRIVATE GLRpp libglr Catch2::Catch2)
    endif()
    target_compile_features(glrpp_test_glrpp PRIVATE cxx_std_20)
    add_test(NAME glrpp_test_glrpp COMMAND glrpp_test_glrpp)
endif()

# Find libglr (optional, for validation)
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBGLR libglr)
endif()

# Install
install(FILES GLRpp.hpp Polyfills.hpp DSLUtils.hpp
              rewrite/GrammarIR.hpp
              rewrite/RewritePass.hpp
              rewrite/RewritePipeline.hpp
              rewrite/native/NativePasses.hpp
              rewrite/equinox/EquinoxPasses.hpp
              rewrite/syntax/SyntaxDSL.hpp
        DESTINATION include/GLRpp)

# pkg-config file
configure_file(cmake/glrpp.pc.in glrpp.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/glrpp.pc
        DESTINATION lib/pkgconfig)

# Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(Doxyfile.in Doxyfile @ONLY)
    add_custom_target(glrpp-docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
    )
endif()
