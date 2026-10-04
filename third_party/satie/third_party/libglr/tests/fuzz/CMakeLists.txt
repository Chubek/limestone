cmake_minimum_required(VERSION 3.16)

option(ENABLE_FUZZING "Enable AFL fuzzing targets" OFF)

if(NOT ENABLE_FUZZING)
    message(STATUS "Fuzzing disabled. Use -DENABLE_FUZZING=ON to enable.")
    return()
endif()

find_program(AFL_CC afl-clang-fast)
find_program(AFL_CXX afl-clang-fast++)

if(NOT AFL_CC)
    find_program(AFL_CC afl-gcc)
    find_program(AFL_CXX afl-g++)
endif()

if(NOT AFL_CC)
    message(WARNING "AFL not found. Install afl++ to enable fuzzing.")
    return()
endif()

message(STATUS "AFL found: ${AFL_CC}")

function(add_fuzz_target target)
    add_executable(${target} ${ARGN})
    
    set_target_properties(${target} PROPERTIES
        C_COMPILER ${AFL_CC}
        CXX_COMPILER ${AFL_CXX}
    )
    
    target_compile_options(${target} PRIVATE -g -O2 -fsanitize=address,undefined)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    
    target_link_libraries(${target} PRIVATE libglr)
    
if(ENABLE_CACHE)
        if(TARGET mdbx-static)
            target_link_libraries(${target} PRIVATE mdbx-static)
        elseif(TARGET mdbx)
            target_link_libraries(${target} PRIVATE mdbx)
        endif()
        
        if(OPENSSL_FOUND)
            target_link_libraries(${target} PRIVATE OpenSSL::Crypto)
        endif()
    endif()
endfunction()

# Core fuzzing targets
add_fuzz_target(fuzz_grammar fuzz_grammar.c)
add_fuzz_target(fuzz_stack fuzz_stack.c)
add_fuzz_target(fuzz_forest fuzz_forest.c)
add_fuzz_target(fuzz_reader fuzz_reader.c)
add_fuzz_target(fuzz_parser fuzz_parser.c)
add_fuzz_target(fuzz_live_query fuzz_live_query.c)

# Incremental parsing fuzzing targets (if cache enabled)
if(ENABLE_CACHE)
    add_fuzz_target(fuzz_cache fuzz_cache.c)
    add_fuzz_target(fuzz_serialization fuzz_serialization.c)
    add_fuzz_target(fuzz_diff fuzz_diff.c)
endif()

# Create corpus directories
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_grammar)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_stack)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_forest)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_reader)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_parser)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_cache)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_serialization)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/corpus_diff)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/findings)

# Seed files for core components (using simple text to avoid escape issues)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_grammar/seed1.txt "terminal IDENT")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_stack/seed1.bin "0123456789")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_forest/seed1.bin "hello world")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_reader/seed1.txt "test input\n")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_parser/seed1.txt "1 + 2")

if(ENABLE_CACHE)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_cache/seed1.txt "hello world")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_cache/seed2.txt "a + b * c")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_serialization/seed1.bin "binary data")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/corpus_diff/seed1.txt "old\nnew")
endif()

# Fuzzing targets
add_custom_target(fuzz-grammar
    COMMAND echo "To run: afl-fuzz -i corpus_grammar -o findings ./fuzz_grammar"
    DEPENDS fuzz_grammar
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(fuzz-stack
    COMMAND echo "To run: afl-fuzz -i corpus_stack -o findings ./fuzz_stack"
    DEPENDS fuzz_stack
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(fuzz-forest
    COMMAND echo "To run: afl-fuzz -i corpus_forest -o findings ./fuzz_forest"
    DEPENDS fuzz_forest
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(fuzz-reader
    COMMAND echo "To run: afl-fuzz -i corpus_reader -o findings ./fuzz_reader"
    DEPENDS fuzz_reader
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(fuzz-parser
    COMMAND echo "To run: afl-fuzz -i corpus_parser -o findings ./fuzz_parser"
    DEPENDS fuzz_parser
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

if(ENABLE_CACHE)
    add_custom_target(fuzz-cache
        COMMAND echo "To run: afl-fuzz -i corpus_cache -o findings ./fuzz_cache"
        DEPENDS fuzz_cache
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(fuzz-serialization
        COMMAND echo "To run: afl-fuzz -i corpus_serialization -o findings ./fuzz_serialization"
        DEPENDS fuzz_serialization
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_custom_target(fuzz-diff
        COMMAND echo "To run: afl-fuzz -i corpus_diff -o findings ./fuzz_diff"
        DEPENDS fuzz_diff
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    add_custom_target(fuzz-all
        DEPENDS fuzz_grammar fuzz_stack fuzz_forest fuzz_reader fuzz_parser
                fuzz_cache fuzz_serialization fuzz_diff
    )
else()
    add_custom_target(fuzz-all
        DEPENDS fuzz_grammar fuzz_stack fuzz_forest fuzz_reader fuzz_parser
    )
endif()

add_custom_target(fuzz-live-query
    COMMAND echo "To run: afl-fuzz -i corpus_live_query -o findings ./fuzz_live_query"
    DEPENDS fuzz_live_query
)
