cmake_minimum_required(VERSION 3.14)

add_executable(bench_dot_product bench_dot_product.cpp)
add_executable(bench_mandelbrot bench_mandelbrot.cpp)
add_executable(bench_linalg_matrices bench_linalg_matrices.cpp)

target_include_directories(bench_dot_product PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(bench_mandelbrot PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(bench_linalg_matrices PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_compile_features(bench_dot_product PRIVATE cxx_std_20)
target_compile_features(bench_mandelbrot PRIVATE cxx_std_20)
target_compile_features(bench_linalg_matrices PRIVATE cxx_std_20)

target_compile_options(bench_dot_product PRIVATE $<$<CONFIG:Release>:-O3>)
target_compile_options(bench_mandelbrot PRIVATE $<$<CONFIG:Release>:-O3>)
target_compile_options(bench_linalg_matrices PRIVATE $<$<CONFIG:Release>:-O3>)
