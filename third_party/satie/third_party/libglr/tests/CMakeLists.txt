cmake_minimum_required(VERSION 3.16)

enable_testing()

add_library(glr_test_config INTERFACE)
target_include_directories(glr_test_config INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(glr_test_config INTERFACE -Wall -Wextra -Wpedantic)

if(ENABLE_TEST_SANITIZERS)
    target_compile_options(glr_test_config INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(glr_test_config INTERFACE -fsanitize=address,undefined)
endif()

function(glr_add_test target)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES LIBRARIES LABELS)
    cmake_parse_arguments(GLRTEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${target} ${GLRTEST_SOURCES})
    target_link_libraries(${target} PRIVATE libglr glr_test_config ${GLRTEST_LIBRARIES})

    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        TIMEOUT 30
        LABELS "${GLRTEST_LABELS}")
endfunction()

glr_add_test(test_grammar SOURCES test_grammar.c LABELS "unit;core")
glr_add_test(test_stack SOURCES test_stack.c LABELS "unit;core")
glr_add_test(test_fork SOURCES test_fork.c LABELS "unit;core")
glr_add_test(test_forest SOURCES test_forest.c LABELS "unit;core")
glr_add_test(test_reduction SOURCES test_reduction.c LABELS "unit;core")
glr_add_test(test_parser SOURCES test_parser.c LABELS "unit;core")
glr_add_test(test_reader SOURCES test_reader.c LABELS "unit;core;lexer")
glr_add_test(test_rewrite SOURCES test_rewrite.c LABELS "unit;rewrite")
glr_add_test(test_stratified SOURCES test_stratified.c LABELS "unit;core;stratified")
glr_add_test(test_stratified_sppf SOURCES test_stratified_sppf.c LABELS "unit;core;stratified;forest")
glr_add_test(test_live_parsing SOURCES test_live_parsing.c LABELS "unit;core;live")
glr_add_test(test_query SOURCES test_query.c LABELS "unit;core;query")
glr_add_test(test_scannerless SOURCES test_scannerless.c LABELS "unit;core;scannerless")
glr_add_test(test_semantic_action SOURCES test_semantic_action.c LABELS "unit;core;semantic")
glr_add_test(test_thread SOURCES test_thread.c LABELS "unit;core;thread")

# Stratified suite needs the same feature macros as the library so the
# cache-backed cases compile in when the incremental cache is enabled.
if(ENABLE_CACHE AND HAVE_LIBMDBX)
    target_compile_definitions(test_stratified PRIVATE HAVE_LMDB=1 HAVE_LIBMDBX=1)
    if(TARGET mdbx-static)
        target_link_libraries(test_stratified PRIVATE mdbx-static)
    elseif(TARGET mdbx)
        target_link_libraries(test_stratified PRIVATE mdbx)
    endif()
    if(OpenSSL_FOUND)
        target_link_libraries(test_stratified PRIVATE OpenSSL::Crypto)
    endif()
endif()
glr_add_test(test_ambiguity
    SOURCES test_ambiguity.c
    LABELS "unit;integration;legacy")

if(TARGET glrdisambstd)
    target_link_libraries(test_ambiguity PRIVATE glrdisambstd)
    glr_add_test(test_disambiguate
        SOURCES test_disambiguate.c
        LIBRARIES glrdisambstd
        LABELS "unit;disambiguation;legacy")
endif()

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS
        test_grammar
        test_stack
        test_fork
        test_forest
        test_reduction
        test_parser
        test_reader
        test_rewrite
        test_stratified
        test_stratified_sppf
        test_live_parsing
        test_query
        test_scannerless
        test_semantic_action
        test_thread
        test_ambiguity
    USES_TERMINAL)

if(TARGET test_disambiguate)
    add_dependencies(check test_disambiguate)
endif()

add_custom_target(check-verbose
    COMMAND ${CMAKE_CTEST_COMMAND} -V
    DEPENDS check
    USES_TERMINAL)

# Catch2 tests for incremental parsing
if(BUILD_TESTS)
    add_subdirectory(catch2)
endif()

# AFL fuzzing targets
add_subdirectory(fuzz)
