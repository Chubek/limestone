# Simdette Usage

Simdette provides a high-level, operator-overloaded frontend for CPU SIMD intrinsics.

## Core types

```cpp
#include "simdette/simdette.hpp"

using f32x4 = simdette::batch<float, simdette::default_arch>;
```

## Basic operations

```cpp
f32x4 a{1.0f, 2.0f, 3.0f, 4.0f};
f32x4 b{5.0f, 6.0f, 7.0f, 8.0f};

f32x4 sum = a + b;          // element-wise addition
f32x4 prod = a * b;         // element-wise multiplication
f32x4 mask = a < b;         // comparison (mask)
```

## Memory access

```cpp
float data[8];
f32x4 v = f32x4::load_aligned(data);  // or load_unaligned
v.to_array(data);                     // store back
```

## Math functions

```cpp
#include "simdette/math/basic_math.hpp"
f32x4 r = simdette::sqrt(x);
f32x4 a = simdette::abs(x);
```

## Architecture selection

Choose via compiler flags (`-march=native`, `-mavx2`, `-mneon`), or define manually:

```bash
g++ -DSIMDETTE_ARCH_AVX2 ...
```
