cmake_minimum_required(VERSION 3.20)

project(MEMTKX VERSION 1.0 LANGUAGES CXX)

include(GNUInstallDirs)

option(MEMTKX_BUILD_TESTS "Build the MEMTKX test suite" ON)
option(MEMTKX_BUILD_BENCHMARKS "Build the MEMTKX benchmarks" OFF)
option(MEMTKX_ENABLE_WARNINGS "Enable strict compiler warnings" ON)

add_library(MEMTKX INTERFACE)
add_library(MEMTKX::MEMTKX ALIAS MEMTKX)

target_include_directories(MEMTKX
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(MEMTKX INTERFACE cxx_std_20)

if(MEMTKX_ENABLE_WARNINGS)
    target_compile_options(MEMTKX INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    )
endif()

find_package(Threads REQUIRED)
target_link_libraries(MEMTKX INTERFACE Threads::Threads)

if(MEMTKX_BUILD_TESTS)
    enable_testing()

    set(MEMTKX_TESTS
        test_units
        test_allocator
        test_barrier
        test_immix
        test_large_object
        test_scheduler_edges
        test_dispatcher
        test_scheduler
        test_plans
    )

    foreach(test_name IN LISTS MEMTKX_TESTS)
        add_executable(${test_name} tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE MEMTKX::MEMTKX)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()

if(MEMTKX_BUILD_BENCHMARKS)
    set(MEMTKX_BENCHMARKS
        bench_alloc
        bench_barrier
    )

    foreach(bench_name IN LISTS MEMTKX_BENCHMARKS)
        add_executable(${bench_name} benchmarks/${bench_name}.cpp)
        target_link_libraries(${bench_name} PRIVATE MEMTKX::MEMTKX)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${bench_name} PRIVATE -O3)
        endif()
    endforeach()
endif()

install(DIRECTORY include/MEMTKX
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp")

install(TARGETS MEMTKX EXPORT MEMTKXTargets)
install(EXPORT MEMTKXTargets
        FILE MEMTKXTargets.cmake
        NAMESPACE MEMTKX::
        DESTINATION lib/cmake/MEMTKX)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/MEMTKXConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/MEMTKXConfig.cmake"
    INSTALL_DESTINATION lib/cmake/MEMTKX
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/MEMTKXConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/MEMTKXConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/MEMTKXConfigVersion.cmake"
    DESTINATION lib/cmake/MEMTKX
)
