# Installing Simdette

Simdette is a **header-only** C++20 library. No installation is required.

## Requirements

- C++20-capable compiler (GCC 10+, Clang 12+, MSVC 2022+)
- CMake 3.20+ (optional, for examples/tests)

## Usage

Add the `include/` directory to your include path:

```bash
g++ -std=c++20 -msse2 -O2 -I./include main.cpp -o main
```

Or with CMake:

```cmake
add_executable(main main.cpp)
target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
```

## Supported Architectures

- x86: SSE, AVX, AVX2, AVX-512
- ARM: NEON, SVE
- WebAssembly: SIMD128
- Scalar fallback (default)

Select via compiler flags (e.g., `-mavx2`, `-msse2`, `-mneon`).
