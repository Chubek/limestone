add_library(glrdisambstd STATIC
    associativity.c
    dynamic_programming.c
    precedence.c
    predicate.c
    probability.c
    semantic.c
)

target_link_libraries(glrdisambstd PUBLIC libglr)

target_include_directories(glrdisambstd
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(glrdisambstd PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)

install(TARGETS glrdisambstd
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
