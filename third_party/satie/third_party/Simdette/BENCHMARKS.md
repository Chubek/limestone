# Simdette Benchmarks

This document describes the benchmark programs included with Simdette.

## Benchmark Programs

### bench_dot_product.cpp

Compares scalar vs SIMD dot product performance.

**Features:**
- Scalable vector sizes
- Multiple iterations for timing
- Speedup calculation

**Build:**
```bash
make bench_dot_product
```

**Run:**
```bash
./bench_dot_product
```

### bench_mandelbrot.cpp

Mandelbrot set generation benchmark.

**Features:**
- Pixel-wise parallelism
- Different resolutions
- Generation time measurement

**Build:**
```bash
make bench_mandelbrot
```

**Run:**
```bash
./bench_mandelbrot
```

### bench_linalg_matrices.cpp

Measures matrix composition plus matrix-vector application throughput.

**Features:**
- Chained `mat4 * mat4` composition
- Per-step `mat4 * vec4` application
- Single checksum to prevent dead-code elimination

**Build:**
```bash
cmake -S . -B build -DSIMDETTE_BUILD_BENCHMARKS=ON
cmake --build build --target bench_linalg_matrices
```

**Run:**
```bash
./build/benchmarks/bench_linalg_matrices
```

## Running Benchmarks

### Using Makefile

```bash
# Build all benchmarks
make bench

# Run specific benchmark
./bench_dot_product
./bench_mandelbrot
```

### Using CMake

```bash
cmake -S . -B build -DSIMDETTE_BUILD_BENCHMARKS=ON
cmake --build build
```

## Benchmark Results

Typical speedup factors:

| Operation | Vector Size | Speedup |
|-----------|-------------|---------|
| Dot Product | 1K elements | 3-5x |
| Dot Product | 16K elements | 4-8x |
| Mandelbrot | 80x60 | 2-4x |
| Mandelbrot | 640x480 | 5-10x |

## Benchmark Methodology

1. **Warmup**: Run once to warm up CPU cache
2. **Timing**: Measure 10,000 iterations
3. **Averaging**: Report average time per iteration
4. **Comparison**: Compare against scalar reference

## System Requirements

- C++20-capable compiler
- CMake 3.14+
- Modern x86_64 or ARM64 CPU
- At least 2GB RAM

## Profiling

For deeper analysis:

```bash
# Profile with gprof
g++ -std=c++20 -pg -O3 bench_dot_product.cpp -o bench_dot_product
./bench_dot_product
gprof bench_dot_product > profile.txt

# Profile with perf (Linux)
perf record ./bench_dot_product
perf report
```

## Adding New Benchmarks

1. Create file in `benchmarks/` directory
2. Include `simdette/simdette.hpp`
3. Implement comparison with scalar version
4. Add to `benchmarks/CMakeLists.txt`

Example structure:

```cpp
#include "simdette/simdette.hpp"
#include <iostream>
#include <chrono>

// Scalar reference
double scalar_function(...) { ... }

// SIMD implementation
double simd_function(...) { ... }

int main() {
    // Setup data
    // Benchmark both implementations
    // Report results
}
```

## Performance Tips

1. **Use `-march=native`** for optimal performance
2. **Profile before optimizing**
3. **Test on target hardware**
4. **Consider memory bandwidth** as bottleneck
