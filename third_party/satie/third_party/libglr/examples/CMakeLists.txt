cmake_minimum_required(VERSION 3.16)

# Calculator example
add_executable(calc calc.c)
target_link_libraries(calc PRIVATE libglr)

# Ambiguous grammar example
add_executable(ambiguous ambiguous.c)
target_link_libraries(ambiguous PRIVATE libglr)

# Incremental editing example
add_executable(live live.c)
target_link_libraries(live PRIVATE libglr)

# S-expression query and AST building example
add_executable(query query.c)
target_link_libraries(query PRIVATE libglr)

# Install examples
install(TARGETS calc ambiguous live query
    RUNTIME DESTINATION bin
)
