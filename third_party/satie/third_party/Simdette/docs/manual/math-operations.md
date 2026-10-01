# Math Operations

Simdette provides both arithmetic operators and specialized math
functions for vectorized computation.

## Arithmetic Operators

All batch types support standard arithmetic operators:

``` cpp
batch<float, simdette::avx2> a{1, 2, 3, 4, 5, 6, 7, 8};
batch<float, simdette::avx2> b{10, 20, 30, 40, 50, 60, 70, 80};

// Element-wise operations
auto sum = a + b;       // [11, 22, 33, 44, 55, 66, 77, 88]
auto diff = a - b;      // [-9, -18, -27, ...]
auto prod = a * b;      // [10, 40, 90, 160, 250, 360, 490, 640]
auto quot = a / b;      // [0.1, 0.1, 0.1, ...]
```

**Compound assignment**:

``` cpp
a += b;    // a = a + b
a -= b;    // a = a - b
a *= b;    // a = a * b
a /= b;    // a = a / b
```

**Scalar operations**:

``` cpp
auto scaled = a * 2.0f;    // [2, 4, 6, 8, 10, 12, 14, 16]
auto offset = a + 1.0f;    // [2, 3, 4, 5, 6, 7, 8, 9]
```

## Comparison Operators

Comparisons return a **mask** (all bits set for true, 0 for false):

``` cpp
batch<float, simdette::avx2> a{1, 5, 3, 7, 2, 6, 4, 8};

auto lt = a < batch<float, simdette::avx2>{4};  // [1, 0, 1, 0, 1, 0, 1, 0]
auto gt = a > batch<float, simdette::avx2>{4};  // [0, 1, 0, 1, 0, 1, 0, 1]
auto eq = a == batch<float, simdette::avx2>{4}; // [0, 0, 0, 0, 0, 0, 1, 0]
```

**All available comparisons**:

-   `operator<` : Less than
-   `operator<=` : Less than or equal
-   `operator>` : Greater than
-   `operator>=` : Greater than or equal
-   `operator==` : Equal
-   `operator!=` : Not equal

## Bitwise Operators (Integer Types)

``` cpp
batch<int32_t, simdette::avx2> a{0xF0F0F0F0, 0x0F0F0F0F, ...};
batch<int32_t, simdette::avx2> b{0x0F0F0F0F, 0xF0F0F0F0, ...};

auto andv = a & b;      // Bitwise AND
auto orv = a | b;       // Bitwise OR
auto xorv = a ^ b;      // Bitwise XOR
auto notv = ~a;         // Bitwise NOT
```

**Compound assignment**:

``` cpp
a &= b;
a |= b;
a ^= b;
```

## Logical Operators (Boolean/Integer)

``` cpp
batch<int32_t, simdette::avx2> a{1, 0, 1, 0, 1, 0, 1, 0};

auto andv = a && batch<int32_t, simdette::avx2>{1};  // Logical AND
auto orv = a || batch<int32_t, simdette::avx2>{0};   // Logical OR
auto notv = !a;                                      // Logical NOT
```

## Unary Operators

``` cpp
batch<float, simdette::avx2> a{1, 2, 3, 4, 5, 6, 7, 8};

auto neg = -a;   // [-1, -2, -3, -4, -5, -6, -7, -8]
auto pos = +a;   // [1, 2, 3, 4, 5, 6, 7, 8] (no-op)
```

## Specialized Math Functions

Include the math header:

``` cpp
#include "simdette/math/basic_math.hpp"
```

\### Elementary Functions

**Absolute value**:

``` cpp
batch<float, simdette::avx2> a{-3, -2, -1, 0, 1, 2, 3, 4};
auto abs = simdette::abs(a);  // [3, 2, 1, 0, 1, 2, 3, 4]
```

**Square root**:

``` cpp
batch<float, simdette::avx2> a{1, 4, 9, 16, 25, 36, 49, 64};
auto sqrt = simdette::sqrt(a);  // [1, 2, 3, 4, 5, 6, 7, 8]
```

**Reciprocal square root** (fast approximation):

``` cpp
batch<float, simdette::avx2> a{1, 4, 9, 16, 25, 36, 49, 64};
auto rsqrt = simdette::rsqrt(a);  // [1, 0.5, 0.333, 0.25, ...]
```

**Floor** (round down):

``` cpp
batch<float, simdette::avx2> a{1.7, 2.3, 3.9, 4.1, ...};
auto floored = simdette::floor(a);  // [1, 2, 3, 4, ...]
```

**Ceil** (round up):

``` cpp
auto ceiled = simdette::ceil(a);  // [2, 3, 4, 5, ...]
```

**Round** (nearest integer):

``` cpp
auto rounded = simdette::round(a);  // [2, 2, 4, 4, ...]
```

**Truncate** (round toward zero):

``` cpp
auto truncated = simdette::trunc(a);
```

**Minimum/Maximum**:

``` cpp
batch<float, simdette::avx2> a{1, 5, 3, 7, 2, 6, 4, 8};
batch<float, simdette::avx2> b{4, 2, 6, 1, 5, 3, 8, 0};

auto minv = simdette::min(a, b);   // [1, 2, 3, 1, 2, 3, 4, 0]
auto maxv = simdette::max(a, b);   // [4, 5, 6, 7, 5, 6, 8, 8]
```

**Clamp** ( constrain to range):

``` cpp
batch<float, simdette::avx2> a{-1, 0, 5, 10, 15, 20};
auto lo = batch<float, simdette::avx2>{0};
auto hi = batch<float, simdette::avx2>{10};

auto clamped = simdette::clamp(a, lo, hi);  // [0, 0, 5, 10, 10, 10]
```

**Is NaN / Is Inf**:

``` cpp
batch<float, simdette::avx2> a{1, 2, std::numeric_limits<float>::quiet_NaN(), 4};

auto isnan = simdette::isnan(a);  // [0, 0, 1, 0]
auto isinf = simdette::isinf(a);  // [0, 0, 0, 0]
```

## Function Overloads

All math functions are overloaded for:

1.  **Scalar types** (single values)
2.  **Batch types** (vectorized)

``` cpp
// Scalar
float x = 4.0f;
float y = simdette::sqrt(x);  // 2.0f

// Batch
simdette::batch<float, simdette::avx2> v{4, 9, 16, 25, 36, 49, 64, 81};
auto result = simdette::sqrt(v);  // Vectorized
```

## Linear Algebra

`simdette/math/linalg.hpp` adds a linear algebra layer with row-major
matrix storage and column-vector multiplication (`m * v`).

``` cpp
#include "simdette/math/linalg.hpp"

simdette::fvec3 a{1.0f, 2.0f, 3.0f};
simdette::fvec3 b{4.0f, 5.0f, 6.0f};

auto d = simdette::dot(a, b);
auto c = simdette::cross(a, b);
auto n = simdette::normalize(a);

simdette::fmat3 m = simdette::fmat3::identity();
auto out = m * a;
```

Available vector and matrix templates:

- `vec2<T>`, `vec3<T>`, `vec4<T>`
- `mat2<T>`, `mat3<T>`, `mat4<T>`

Core free functions:

- vectors: `dot`, `cross`, `length`, `length_squared`, `normalize`, `normalize_or`
- matrices: `transpose`, `determinant`, `inverse`
- composition helpers: `homogeneous_translation`, `homogeneous_scale`, `homogeneous_rotation`, `basis_from_columns`

## Transcendental Functions

The `transcendental.hpp` header provides advanced functions:

``` cpp
#include "simdette/math/transcendental.hpp"

batch<float, simdette::avx2> a{1, 2, 3, 4, 5, 6, 7, 8};

auto sinv = simdette::sin(a);
auto cosv = simdette::cos(a);
auto tanv = simdette::tan(a);
auto expv = simdette::exp(a);
auto logv = simdette::log(a);
auto powv = simdette::pow(a, batch<float, simdette::avx2>{2});
```

Note: Transcendental functions are implemented via table lookup or
polynomial approximation for performance.

## Expression Templates

Simdette supports expression templates for efficient composition:

``` cpp
batch<float, simdette::avx2> a{1, 2, 3, 4, 5, 6, 7, 8};
batch<float, simdette::avx2> b{10, 20, 30, 40, 50, 60, 70, 80};

// Complex expression (optimized)
auto result = (a * a) + (2 * a * b) + (b * b);  // (a + b)^2

// No temporary batches created
// All operations fused where possible
```

## Performance Notes

-   **Element-wise operations** (`+`, `-`, `*`, `/`) map directly to
    intrinsics
-   **Transcendental functions** use polynomial approximations
-   **Reduction operations** (`reduce_add`, etc.) may be slower
-   **Scalar conversions** (`extract`, `to_array`) require memory access

## Best Practice

For performance-critical code, prefer intrinsic-level operations over
function calls where possible:

``` cpp
// Good: Direct operations
auto result = a * a + b * b;

// Less efficient: Function calls
auto result = simdette::add(simdette::mul(a, a), simdette::mul(b, b));
```
