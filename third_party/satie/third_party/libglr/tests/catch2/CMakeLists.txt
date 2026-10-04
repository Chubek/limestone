cmake_minimum_required(VERSION 3.16)
enable_language(CXX)

find_package(Catch2 3 QUIET)

if(NOT Catch2_FOUND)
    find_package(Catch2 2 QUIET)
endif()

if(NOT Catch2_FOUND)
    message(WARNING "Catch2 not found. Install with: sudo pacman -S catch2")
    message(STATUS "Skipping Catch2 tests")
    return()
endif()

function(add_catch2_test target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE libglr Catch2::Catch2)
    target_compile_features(${target} PRIVATE cxx_std_11)
    
if(ENABLE_CACHE)
    target_compile_definitions(${target} PRIVATE HAVE_LMDB=1 HAVE_LIBMDBX=1)
    if(TARGET mdbx-static)
        target_link_libraries(${target} PRIVATE mdbx-static)
    elseif(TARGET mdbx)
        target_link_libraries(${target} PRIVATE mdbx)
    endif()
    if(OPENSSL_FOUND)
        target_link_libraries(${target} PRIVATE OpenSSL::Crypto)
    endif()
    endif()
    
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 60)
endfunction()

if(ENABLE_CACHE)
    add_catch2_test(catch2_test_cache test_cache.cpp)
    add_catch2_test(catch2_test_diff test_diff.cpp)
    add_catch2_test(catch2_test_serialization test_serialization.cpp)
    add_catch2_test(catch2_test_incremental test_incremental.cpp)
    
    # Comprehensive test suite (26 tests)
    add_catch2_test(catch2_test_comprehensive test_comprehensive.cpp)
    
    add_custom_target(check-catch2
        COMMAND ${CMAKE_CTEST_COMMAND} -R "catch2_" --output-on-failure
        DEPENDS
            catch2_test_cache
            catch2_test_diff
            catch2_test_serialization
            catch2_test_incremental
            catch2_test_comprehensive
        USES_TERMINAL
    )
else()
    message(STATUS "Cache disabled, skipping Catch2 incremental tests")
endif()
