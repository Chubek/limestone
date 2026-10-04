# Simdette

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![Header-Only](https://img.shields.io/badge/Type-Header--Only-green.svg)](https://github.com/topics/header-only)

A **header-only C++20 abstraction library for SIMD intrinsics**. Write vectorized code using familiar C++ operators without needing to know intrinsics.

## Features

- **Zero-intrinsic knowledge required**: Write `a + b` instead of `_mm_add_ps(a, b)`
- **Cross-platform**: Supports x86 (SSE, AVX, AVX2, AVX-512), ARM (NEON, SVE), WebAssembly (SIMD128)
- **Header-only**: No compilation or installation needed
- **C++20**: Uses concepts and constraints for type safety
- **Type-safe**: Compile-time architecture selection
- **Linear algebra layer**: `vec*` / `mat*` with free-function operations

## Quick Start

```cpp
#include "simdette/simdette.hpp"

using f32x4 = simdette::batch<float, simdette::default_arch>;

f32x4 a{1.0f, 2.0f, 3.0f, 4.0f};
f32x4 b{5.0f, 6.0f, 7.0f, 8.0f};

f32x4 sum = a + b;          // Element-wise addition
f32x4 prod = a * b;         // Element-wise multiplication
f32x4 mask = a < b;         // Comparison (returns mask)
```

## Installation

Simdette is header-only. Simply add the `include/` directory to your include path:

```bash
g++ -std=c++20 -msse2 -O2 -I./include main.cpp -o main
```

Or with CMake:

```cmake
add_executable(main main.cpp)
target_include_directories(main PRIVATE ${CMAKE_SOURCE_DIR}/include)
```

## Supported Architectures

| Architecture | Vector Width | Description |
|--------------|--------------|-------------|
| **SSE** | 128-bit | Basic SIMD (all x86/x86_64) |
| **AVX** | 256-bit | Advanced SIMD |
| **AVX2** | 256-bit | AVX + integer operations |
| **AVX-512** | 512-bit | Wide vector processing |
| **NEON** | 128-bit | ARM SIMD |
| **SVE** | Variable | ARM Scalable Vector Extension |
| **SIMD128** | 128-bit | WebAssembly SIMD |
| **Scalar** | 1 | Pure C++ fallback |

## Documentation

- **[User Manual](doc/manual/)**: Complete guide to using Simdette
- **[Doxygen Docs](doc/doxygen/html/)**: API reference (run `make doc`)
- **[Examples](examples/)**: Working code examples
- **[Tests](tests/)**: Test suite

## Building

### Using CMake

```bash
mkdir build && cd build
cmake .. -DSIMDETTE_BUILD_TESTS=ON
make
ctest
```

### Using Makefile

```bash
make           # Build examples and tests
make examples  # Build examples only
make tests     # Build tests only
make clean     # Clean build artifacts
```

### Using Meson

```bash
meson setup build
meson compile -C build
meson test -C build
```

## Examples

```bash
# Build examples
make examples

# Run examples
./example_basics
./example_math
./example_dot_product
./example_mandelbrot
./example_linalg_vectors
./example_linalg_matrices
```

## Testing

```bash
# Build and run tests
make tests
./test_comprehensive

# Using CTest
ctest --test-dir build
```

## Performance

Simdette automatically detects and uses the best available architecture at compile time. For optimal performance:

```bash
# Use native CPU optimization
g++ -std=c++20 -march=native -O3 your_code.cpp
```

## Documentation

Generate documentation:

```bash
make doc           # Generate HTML docs
make man           # Generate manpages
make install-doc   # Install documentation
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please read the [CONTRIBUTING](CONTRIBUTING.md) file for guidelines.

## Support

- **Issues**: [GitHub Issues](https://github.com/your-repo/simdette/issues)
- **Documentation**: [User Manual](doc/manual/)
- **API Reference**: [Doxygen Docs](doc/doxygen/html/)

## Acknowledgments

Simdette leverages:
- LLVM/Clang for compiler detection
- Modern C++20 features (concepts, constraints)
- SIMD intrinsics from x86 and ARM architectures
