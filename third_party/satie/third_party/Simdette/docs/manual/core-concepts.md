# Core Concepts

## The Batch Abstraction

At the heart of Simdette is the **\`\`batch\`\`** template, which
represents a vector of values processed in parallel.

### Type Signature

``` cpp
template <typename T, typename Arch = simdette::default_arch>
struct batch;
```

### Parameters

-   **T**: The element type (`float`, `double`, `int32_t`, etc.)
-   **Arch**: The architecture backend (`sse`, `avx2`, `neon`, etc.)

### Common Batch Types

``` cpp
// Auto-detect best architecture
using f32x4 = simdette::batch<float, simdette::default_arch>;
using f64x2 = simdette::batch<double, simdette::default_arch>;
using i32x8 = simdette::batch<int32_t, simdette::default_arch>;

// Explicit architecture
using f32x4_sse = simdette::batch<float, simdette::sse>;
using f32x8_avx2 = simdette::batch<float, simdette::avx2>;
using f32x16_avx512 = simdette::batch<float, simdette::avx512>;
```

### Batch Width

Each architecture has a fixed **vector width** (number of elements):

| Architecture \| float \| double \| int32_t \|

\-\-\-\-\-\-\-\-\-\-\-\-\-\-\--\| \| SSE \| 4 \| 2 \| 4 \| \| AVX \| 8
\| 4 \| 8 \| \| AVX2 \| 8 \| 4 \| 8 \| \| AVX-512 \| 16 \| 8 \| 16 \| \|
NEON \| 4 \| 2 \| 4 \| \| SVE \| Variable \| Variable \| Variable \| \|
Scalar \| 1 \| 1 \| 1 \|

Get the width at compile time:

``` cpp
constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::avx2>;
// width == 8
```

### Constructors

**Default constructor** (uninitialized):

``` cpp
batch<float, simdette::avx2> b;  // Uninitialized
```

**Broadcast scalar**:

``` cpp
batch<float, simdette::avx2> b(3.14f);  // [3.14, 3.14, 3.14, ...]
```

**Load from array**:

``` cpp
float data[8] = {1.0f, 2.0f, ...};
batch<float, simdette::avx2> b(data);  // Load withunaligned load
```

### Memory Access

**Aligned load** (requires 32-byte alignment):

``` cpp
alignas(32) float data[8];
auto b = batch<float, simdette::avx2>::load_aligned(data);
```

**Unaligned load** (no alignment requirement):

``` cpp
float data[8];  // May not be aligned
auto b = batch<float, simdette::avx2>::load_unaligned(data);
```

**Store to memory**:

``` cpp
alignas(32) float out[8];
b.store_aligned(out);
```

**Store unaligned**:

``` cpp
float out[8];
b.store_unaligned(out);
```

**Extract single element**:

``` cpp
float value = b.extract(0);  // Get first element
```

**Convert to array**:

``` cpp
alignas(32) std::array<float, 8> arr;
b.to_array(arr.data());
```

## Basic Operations

**Arithmetic operators**:

``` cpp
batch<float, simdette::avx2> a{1, 2, 3, 4, 5, 6, 7, 8};
batch<float, simdette::avx2> b{10, 20, 30, 40, 50, 60, 70, 80};

auto sum = a + b;      // [11, 22, 33, 44, 55, 66, 77, 88]
auto diff = a - b;     // [-9, -18, -27, ...]
auto prod = a * b;     // [10, 40, 90, ...]
auto quot = a / b;     // [0.1, 0.1, 0.1, ...]
```

**Comparison operators** (return mask):

``` cpp
auto lt = a < b;       // Less-than mask
auto gt = a > b;       // Greater-than mask
auto eq = a == b;      // Equality mask
```

**Bitwise operators** (integer types):

``` cpp
batch<int32_t, simdette::avx2> a{0xF0F0F0F0, ...};
batch<int32_t, simdette::avx2> b{0x0F0F0F0F, ...};

auto andv = a & b;     // Bitwise AND
auto orv = a | b;      // Bitwise OR
auto xorv = a ^ b;     // Bitwise XOR
auto notv = ~a;        // Bitwise NOT
```

**Scalar operations**:

``` cpp
auto scaled = a * 2.0f;      // Multiply by scalar
auto added = a + 1.0f;       // Add scalar
```

## Reduction Operations

Reduce operations collapse a batch into a single scalar:

``` cpp
batch<float, simdette::avx2> v{1, 2, 3, 4, 5, 6, 7, 8};

float sum = v.reduce_add();   // 36.0
float min = v.reduce_min();   // 1.0
float max = v.reduce_max();   // 8.0
```

## Predicate Operations

``` cpp
batch<float, simdette::avx2> v{1, 0, 3, 0, 5, 0, 7, 0};

bool all_nonzero = v.all();   // false (has zeros)
bool any_nonzero = v.any();   // true (has non-zeros)
```

## Compound Assignment

``` cpp
batch<float, simdette::avx2> a{1, 2, 3, 4, 5, 6, 7, 8};

a += batch<float, simdette::avx2>{10};  // a = a + 10
a *= 2.0f;                              // a = a * 2
a -= 5.0f;                              // a = a - 5
```

## Type Traits

Simdette provides various type traits for compile-time checks:

``` cpp
using namespace simdette;

static_assert(is_simd_compatible_v<float>);      // true
static_assert(is_simd_compatible_v<int32_t>);    // true
static_assert(!is_simd_compatible_v<double*>);   // false

static_assert(is_floating_point_v<float>);       // true
static_assert(!is_floating_point_v<int32_t>);    // false

static_assert(is_integral_v<int32_t>);           // true
static_assert(is_signed_integral_v<int32_t>);    // true
static_assert(is_unsigned_integral_v<uint32_t>); // true
```

## Alignment

Simdette defines alignment constants:

``` cpp
constexpr std::size_t Alignment = simdette::Alignment;        // 32
constexpr std::size_t AVX512_Alignment = simdette::AVX512_Alignment;  // 64
```

Use `alignas` for aligned arrays:

``` cpp
alignas(32) float data[8];  // 32-byte aligned for AVX2
```

## Memory Management

Simdette provides `aligned_array` for RAII-managed aligned memory:

``` cpp
using namespace simdette;

aligned_array<float> arr(8);           // Allocate 8 floats
arr[0] = 1.0f;
arr.fill(0.0f);                        // Set all to 0
float* ptr = arr.data();
// Automatically freed when out of scope
```
