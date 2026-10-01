# Getting Started

## Prerequisites

-   **Compiler**: C++20-capable compiler
    -   GCC 10+
    -   Clang 12+
    -   MSVC 2022+
-   **CMake**: Version 3.14+ (optional, for building examples/tests)

## Installation

Simdette is a **header-only** library. No installation is required.

Simply add the `include/` directory to your include path:

``` bash
g++ -std=c++20 -O2 -I./include main.cpp -o main
```

## Using with CMake

Add Simdette to your `CMakeLists.txt`:

``` cmake
cmake_minimum_required(VERSION 3.14)
project(MySimdetteApp VERSION 1.0 LANGUAGES CXX)

add_executable(main main.cpp)
target_include_directories(main PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(main PRIVATE cxx_std_20)
```

## Complete CMake Example

``` cmake
cmake_minimum_required(VERSION 3.14)
project(SimdetteExample VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add your executable
add_executable(example main.cpp)

# Include Simdette headers
target_include_directories(example PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Enable optimizations
target_compile_options(example PRIVATE $<$<CONFIG:Release>:-O3>)
```

## First Program: Vector Addition

Create a file `main.cpp`:

``` cpp
#include "simdette/simdette.hpp"
#include <iostream>
#include <vector>
#include <cmath>

int main() {
    using f32x4 = simdette::batch<float, simdette::default_arch>;

    // Create two vectors
    f32x4 a{1.0f, 2.0f, 3.0f, 4.0f};
    f32x4 b{5.0f, 6.0f, 7.0f, 8.0f};

    // Vectorized addition
    f32x4 sum = a + b;

    // Extract results
    alignas(16) std::array<float, 4> result;
    sum.to_array(result.data());

    // Print results
    std::cout << "Sum: ";
    for (float v : result) {
        std::cout << v << " ";
    }
    std::cout << "\n";

    return 0;
}
```

Compile and run:

``` bash
g++ -std=c++20 -msse2 -O2 -I./include main.cpp -o main
./main
```

Output:

``` text
Sum: 6 8 10 12 
```

## Architecture Detection

Simdette automatically detects the best architecture at **compile
time**:

``` bash
# Detects AVX2 if available
g++ -std=c++20 -O2 -march=native -I./include main.cpp -o main

# Force specific architecture
g++ -std=c++20 -O2 -msse2 -I./include main.cpp -o main
g++ -std=c++20 -O2 -mavx2 -I./include main.cpp -o main
```

You can also force architecture manually:

``` cpp
#include "simdette/simdette.hpp"

// Force SSE
using f32x4 = simdette::batch<float, simdette::sse>;

// Force AVX2
using f32x8 = simdette::batch<float, simdette::avx2>;
```

## Using with Other Build Systems

**Makefile Example**:

``` make
CXX = g++
CXXFLAGS = -std=c++20 -O2 -msse2
INCLUDE = -I./include

all: example

example: main.cpp
    $(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $<

clean:
    rm -f example
```

**Meson Example**:

``` meson
project('simdette-example', 'cpp', version: '1.0')

cpp = meson.get_compiler('cpp')
cpp.add_args('-std=c++20')

executable('example', 'main.cpp',
    include_directories: include_directories('include'),
)
```

## Building with SIMD Flags

Always enable SIMD extensions for best performance:

``` bash
# Conservative (works everywhere)
g++ -std=c++20 -msse2 -O2 ...

# Modern x86
g++ -std=c++20 -march=native -O2 ...

# Maximum performance (AVX2)
g++ -std=c++20 -march=native -mavx2 -O2 ...

# ARM NEON
g++ -std=c++20 -march=armv8-a+simd -O2 ...
```

## Debug vs Release

For debugging, disable optimizations:

``` bash
g++ -std=c++20 -O0 -g -I./include main.cpp -o main_debug
```

For release builds, enable maximum optimizations:

``` bash
g++ -std=c++20 -O3 -march=native -DNDEBUG -I./include main.cpp -o main_release
```

## Next Steps

-   `core-concepts`{.interpreted-text role="doc"}: Learn about batches
    and the type system
-   `math-operations`{.interpreted-text role="doc"}: Explore available
    math functions
-   `examples`{.interpreted-text role="doc"}: See complete working
    examples
