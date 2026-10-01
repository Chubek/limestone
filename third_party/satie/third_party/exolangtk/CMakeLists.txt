cmake_minimum_required(VERSION 3.16)

project(ExolangTk VERSION 0.1.0 LANGUAGES C)

option(EXOLANGTK_BUILD_TESTS "Build ExolangTk smoke tests" OFF)
option(EXOLANGTK_BUILD_DOCS "Build Doxygen documentation" OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(ExolangTk INTERFACE)
add_library(ExolangTk::ExolangTk ALIAS ExolangTk)
target_compile_features(ExolangTk INTERFACE c_std_99)
target_include_directories(ExolangTk INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

add_subdirectory(include)

if(EXOLANGTK_BUILD_DOCS)
    add_subdirectory(docs)
endif()

if(EXOLANGTK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS ExolangTk ExolangTk_InteropTk ExolangTk_FFItk
    ExolangTk_DebugTk ExolangTk_ExtensionTk EXPORT ExolangTkTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")
install(EXPORT ExolangTkTargets
    FILE ExolangTkTargets.cmake
    NAMESPACE ExolangTk::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ExolangTk)

configure_package_config_file(cmake/ExolangTkConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ExolangTkConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ExolangTk)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ExolangTkConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ExolangTkConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ExolangTkConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ExolangTk)
