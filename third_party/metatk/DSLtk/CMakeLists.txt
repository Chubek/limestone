project(DSLtk VERSION 1.0 LANGUAGES CXX)

option(DSLtk_BUILD_GENERATOR "Build the parser generator" ON)

add_library(DSLtk INTERFACE)

target_include_directories(DSLtk
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(DSLtk INTERFACE cxx_std_20)

install(
    FILES DSLtk.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    TARGETS DSLtk
    EXPORT DSLtkTargets
)

install(
    EXPORT DSLtkTargets
    FILE DSLtkTargets.cmake
    NAMESPACE DSLtk::
    DESTINATION lib/cmake/DSLtk
)

if (DSLtk_BUILD_GENERATOR)
    find_program(DSLtk_D_COMPILER NAMES ldc2 dmd gdc
                 DOC "D compiler used to build the parser generator")

    if (NOT DSLtk_D_COMPILER)
        message(WARNING "DSLtk: no D compiler found (ldc2/dmd/gdc); "
                        "the parser generator will not be built")
    else()
        set(DSLtk_GENERATOR_SRC
            ${CMAKE_CURRENT_SOURCE_DIR}/generator/DSLtk-ParserGenerator.d)
        set(DSLtk_GENERATOR_BIN
            ${CMAKE_CURRENT_BINARY_DIR}/dsltk-generate${CMAKE_EXECUTABLE_SUFFIX})

        # dmd/ldc2 take -of=<file>; gdc uses the usual -o <file>.
        if (DSLtk_D_COMPILER MATCHES "gdc")
            set(_dsltk_d_flags -O2 -o ${DSLtk_GENERATOR_BIN})
        else()
            set(_dsltk_d_flags -O -of=${DSLtk_GENERATOR_BIN})
        endif()

        add_custom_command(
            OUTPUT ${DSLtk_GENERATOR_BIN}
            COMMAND ${DSLtk_D_COMPILER} ${_dsltk_d_flags} ${DSLtk_GENERATOR_SRC}
            DEPENDS ${DSLtk_GENERATOR_SRC}
            COMMENT "Building DSLtk parser generator"
            VERBATIM
        )

        add_custom_target(DSLtk_generator ALL DEPENDS ${DSLtk_GENERATOR_BIN})

        add_executable(DSLtk::generator IMPORTED GLOBAL)
        set_target_properties(DSLtk::generator PROPERTIES
            IMPORTED_LOCATION ${DSLtk_GENERATOR_BIN})

        install(PROGRAMS ${DSLtk_GENERATOR_BIN}
                DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

include(CMakePackageConfigHelpers)

configure_package_config_file(
    cmake/DSLtkConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/DSLtkConfig.cmake"
    INSTALL_DESTINATION lib/cmake/DSLtk
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/DSLtk.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/DSLtk.pc
    @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/DSLtk.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/DSLtkConfig.cmake"
    DESTINATION lib/cmake/DSLtk
)

