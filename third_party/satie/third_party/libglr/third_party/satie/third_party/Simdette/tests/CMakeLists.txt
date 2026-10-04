# ============================================================================
# Simdette Test CMake Configuration
# ============================================================================

cmake_minimum_required(VERSION 3.14)

# Add test executables
add_executable(test_addition test_addition.cpp)
add_executable(test_math test_math.cpp)
add_executable(test_memory_load test_memory_load.cpp)
add_executable(test_linalg_vectors test_linalg_vectors.cpp)
add_executable(test_linalg_matrices test_linalg_matrices.cpp)
add_executable(test_linalg_fuzz test_linalg_fuzz.cpp)

# Include directories
target_include_directories(test_addition PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(test_math PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(test_memory_load PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(test_linalg_vectors PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR})
target_include_directories(test_linalg_matrices PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR})
target_include_directories(test_linalg_fuzz PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR})

# Enable C++20
target_compile_features(test_addition PRIVATE cxx_std_20)
target_compile_features(test_math PRIVATE cxx_std_20)
target_compile_features(test_memory_load PRIVATE cxx_std_20)
target_compile_features(test_linalg_vectors PRIVATE cxx_std_20)
target_compile_features(test_linalg_matrices PRIVATE cxx_std_20)
target_compile_features(test_linalg_fuzz PRIVATE cxx_std_20)

# Optimization for release builds
target_compile_options(test_addition PRIVATE $<$<CONFIG:Release>:-O3>)
target_compile_options(test_math PRIVATE $<$<CONFIG:Release>:-O3>)
target_compile_options(test_memory_load PRIVATE $<$<CONFIG:Release>:-O3>)
target_compile_options(test_linalg_vectors PRIVATE $<$<CONFIG:Release>:-O3>)
target_compile_options(test_linalg_matrices PRIVATE $<$<CONFIG:Release>:-O3>)
target_compile_options(test_linalg_fuzz PRIVATE $<$<CONFIG:Release>:-O3>)

# Register tests with CTest
include(CTest)

add_test(NAME AdditionTests COMMAND test_addition)
add_test(NAME MathTests COMMAND test_math)
add_test(NAME MemoryLoadTests COMMAND test_memory_load)
add_test(NAME LinalgVectorTests COMMAND test_linalg_vectors)
add_test(NAME LinalgMatrixTests COMMAND test_linalg_matrices)
add_test(NAME LinalgFuzzTests COMMAND test_linalg_fuzz)
