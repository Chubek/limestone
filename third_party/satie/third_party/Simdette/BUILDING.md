# Building Simdette

This guide covers all build systems supported by Simdette.

## Prerequisites

- **C++20 Compiler**: GCC 10+, Clang 12+, MSVC 2022+
- **CMake**: Version 3.14+ (for CMake builds)
- **Meson**: Version 0.56+ (optional)
- **Automake/Autoconf**: (optional, for autotools)
- **Doxygen**: For documentation generation

## Quick Start

### CMake (Recommended)

```bash
# Configure
mkdir build && cd build
cmake .. -DSIMDETTE_BUILD_TESTS=ON -DSIMDETTE_BUILD_EXAMPLES=ON

# Build
cmake --build . --parallel

# Test
ctest --output-on-failure

# Install (optional)
cmake --install . --prefix /usr/local
```

### Makefile

```bash
# Build everything
make

# Build specific targets
make examples
make tests
make bench

# Run tests
./test_comprehensive

# Clean
make clean
```

### Meson

```bash
# Setup
meson setup build

# Build
meson compile -C build

# Test
meson test -C build

# Install
sudo meson install -C build
```

### Ninja

```bash
# Build with Ninja
ninja

# Run tests
./test_comprehensive
```

### Autotools

```bash
# Generate configure script
./autogen.sh

# Configure
./configure --enable-tests --enable-examples

# Build
make

# Test
make check

# Install
sudo make install
```

## Build Options

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `SIMDETTE_BUILD_TESTS` | OFF | Build test suite |
| `SIMDETTE_BUILD_EXAMPLES` | OFF | Build examples |
| `SIMDETTE_BUILD_BENCHMARKS` | OFF | Build benchmarks |
| `SIMDETTE_BUILD_DOCS` | ON | Build documentation |
| `SIMDETTE_INSTALL` | ON | Install library |

Example:

```bash
cmake .. -DSIMDETTE_BUILD_TESTS=ON -DSIMDETTE_BUILD_EXAMPLES=ON
```

### Meson Options

| Option | Default | Description |
|--------|---------|-------------|
| `build_tests` | false | Build test suite |
| `install` | false | Install library |

Example:

```bash
meson setup build -Dbuild_tests=true
```

## Architecture Flags

Simdette automatically detects available SIMD extensions, but you can force specific ones:

### GCC/Clang

```bash
# SSE2 (baseline)
g++ -std=c++20 -msse2 ...

# AVX
g++ -std=c++20 -mavx ...

# AVX2 (recommended)
g++ -std=c++20 -mavx2 ...

# AVX-512
g++ -std=c++20 -mavx512f ...

# Native (best for current CPU)
g++ -std=c++20 -march=native ...
```

### MSVC

Set in Visual Studio project properties:
- C/C++ → Code Generation → Enable Enhanced Instructions

## Build Types

### Debug Build

```bash
# CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .

# Makefile
make CXXFLAGS="-g -O0"
```

### Release Build

```bash
# CMake
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Makefile
make CXXFLAGS="-O3 -DNDEBUG"
```

### RelWithDebInfo

```bash
# CMake
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .
```

## Testing

### Running Tests

```bash
# CTest
ctest --test-dir build --output-on-failure

# Directly
./test_comprehensive
./test_addition
./test_math
```

### Test Coverage

```bash
# GCC with coverage
g++ -std=c++20 --coverage -fprofile-arcs -ftest-coverage tests/test_comprehensive.cpp -o test_coverage
./test_coverage
gcov tests/test_comprehensive.cpp.gcno
```

## Documentation

### Generate HTML Documentation

```bash
# Using CMake
cmake --build . --target doc

# Using Makefile
make doc
```

### Generate Manpages

```bash
# Using script
./scripts/gen_manpages.sh

# Using Makefile
make man
```

## Installation

### CMake Installation

```bash
cmake --install build --prefix /usr/local
```

### Meson Installation

```bash
meson install -C build
```

### Manual Installation

```bash
# Headers
sudo cp -r include/* /usr/local/include/

# Documentation
sudo cp -r docs/manual /usr/local/share/docs/simdette/
```

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Build

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install dependencies
      run: sudo apt-get install -y cmake g++
    
    - name: Configure
      run: cmake -S . -B build -DSIMDETTE_BUILD_TESTS=ON
    
    - name: Build
      run: cmake --build build
    
    - name: Test
      run: ctest --test-dir build --output-on-failure
```

## Troubleshooting

### Build Fails with "C++20 required"

Ensure your compiler supports C++20:

```bash
g++ --version  # Should be GCC 10+ or Clang 12+
```

### SIMD Instructions Not Detected

Force architecture flag:

```bash
export CXXFLAGS="-march=native"
```

### Tests Fail

Verify architecture detection:

```bash
# Check which architecture is being used
g++ -std=c++20 -dM -E -march=native < /dev/null | grep -E "AVX|SSE"
```

### Performance Issues

Ensure optimizations are enabled:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

## Additional Resources

- [CMake Documentation](https://cmake.org/documentation/)
- [Meson Documentation](https://mesonbuild.com/)
- [Automake Documentation](https://www.gnu.org/software/automake/)
- [C++20 Features](https://en.cppreference.com/w/cpp/20)
