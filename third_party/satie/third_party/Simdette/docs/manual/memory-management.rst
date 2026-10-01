Memory Management
=================

Simdette provides utilities for aligned memory access, which is critical
for optimal SIMD performance.

Alignment Requirements
----------------------

Different architectures require different memory alignments:

| Architecture | Alignment |
|--------------|-----------|
| SSE          | 16 bytes  |
| AVX/AVX2     | 32 bytes  |
| AVX-512      | 64 bytes  |
| NEON         | 16 bytes  |
| Scalar       | None      |

Simdette defines alignment constants:

.. code-block:: cpp

   #include "simdette/simdette.hpp"

   constexpr std::size_t Alignment = simdette::Alignment;        // 32
   constexpr std::size_t AVX512_Alignment = simdette::AVX512_Alignment;  // 64

Using alignas for Aligned Arrays
---------------------------------

**Stack allocation**:

.. code-block:: cpp

   alignas(32) float data[8];  // 32-byte aligned

   simdette::batch<float, simdette::avx2> v = 
       simdette::batch<float, simdette::avx2>::load_aligned(data);

**Heap allocation with aligned_alloc**:

.. code-block:: cpp

   #include "simdette/core/memory.hpp"

   std::size_t n = 1024;
   float* data = simdette::make_aligned_array<float>(n);

   // Use data...
   simdette::free_aligned_array(data);

**C++17 aligned_new (alternative)**:

.. code-block:: cpp

   alignas(32) float* data = new float[1024];
   // Note: delete[] doesn't preserve alignment
   // Use custom deleter or aligned_array

Memory Access Patterns
----------------------

### Load Operations

**Aligned load** (fastest, requires alignment):

.. code-block:: cpp

   alignas(32) float data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
   auto v = simdette::batch<float, simdette::avx2>::load_aligned(data);

**Unaligned load** (works with any pointer):

.. code-block:: cpp

   float data[8];  // May not be aligned
   auto v = simdette::batch<float, simdette::avx2>::load_unaligned(data);

### Store Operations

**Aligned store**:

.. code-block:: cpp

   alignas(32) float out[8];
   v.store_aligned(out);

**Unaligned store**:

.. code-block:: cpp

   float out[8];
   v.store_unaligned(out);

### Extraction

**Extract single element**:

.. code-block:: cpp

   float first = v.extract(0);
   float fourth = v.extract(3);

**Convert to array**:

.. code-block:: cpp

   alignas(32) std::array<float, 8> arr;
   v.to_array(arr.data());

The aligned_array Container
----------------------------

Simdette provides ``aligned_array`` for RAII-managed aligned memory:

**Basic usage**:

.. code-block:: cpp

   #include "simdette/core/memory.hpp"

   simdette::aligned_array<float> arr(1024);  // Allocate 1024 floats

   // Access like a vector
   arr[0] = 1.0f;
   arr[1] = 2.0f;

   // Fill with value
   arr.fill(0.0f);

   // Get pointer
   float* ptr = arr.data();

   // Automatically freed when out of scope

**With initialization**:

.. code-block:: cpp

   simdette::aligned_array<float> arr(1024, 3.14f);  // All elements = 3.14

**Size queries**:

.. code-block:: cpp

   std::size_t n = arr.size();
   bool empty = arr.empty();

Common Memory Patterns
----------------------

### Vectorized Dot Product

.. code-block:: cpp

   float dot_product(const float* a, const float* b, std::size_t n) {
       using batch_t = simdette::batch<float, simdette::avx2>;
       constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::avx2>;

       simdette::aligned_array<float> sum_vec(width);
       sum_vec.fill(0.0f);

       for (std::size_t i = 0; i < n; i += width) {
           batch_t va(a + i);
           batch_t vb(b + i);
           batch_t prod = va * vb;

           // Reduce and accumulate (simplified)
           alignas(32) float tmp[8];
           prod.to_array(tmp);
           for (std::size_t j = 0; j < width; ++j) {
               sum_vec[j] += tmp[j];
           }
       }

       float result = 0;
       for (std::size_t i = 0; i < width; ++i) {
           result += sum_vec[i];
       }
       return result;
   }

### Transpose Operation

.. code-block:: cpp

   void transpose_4x4(const float* in, float* out) {
       using f32x4 = simdette::batch<float, simdette::sse>;

       alignas(16) f32x4 v0 = f32x4::load_aligned(in);
       alignas(16) f32x4 v1 = f32x4::load_aligned(in + 4);
       alignas(16) f32x4 v2 = f32x4::load_aligned(in + 8);
       alignas(16) f32x4 v3 = f32x4::load_aligned(in + 12);

       // Store transposed
       f32x4::store_aligned(out, v0);
       f32x4::store_aligned(out + 4, v1);
       f32x4::store_aligned(out + 8, v2);
       f32x4::store_aligned(out + 12, v3);
   }

Performance Tips
----------------

**1. Always prefer aligned operations when possible**:

.. code-block:: cpp

   // Good: Aligned
   alignas(32) float data[8];
   auto v = batch_t::load_aligned(data);

   // Avoid: Unaligned (slower)
   float data[8];
   auto v = batch_t::load_unaligned(data);

**2. Pre-allocate memory for repeated operations**:

.. code-block:: cpp

   // Good: Allocate once
   class Processor {
       simdette::aligned_array<float> buffer;
   public:
       Processor(std::size_t n) : buffer(n) {}
   };

   // Bad: Allocate in loop
   for (auto& item : items) {
       simdette::aligned_array<float> temp(1024);  // Allocations!
   }

**3. Minimize scalar-to-vector conversions**:

.. code-block:: cpp

   // Good: Keep in vector form
   auto result = a * b + c * d;

   // Bad: Frequent extraction/storing
   float tmp[8];
   a.to_array(tmp);
   // ... scalar operations ...
   auto v = batch_t(tmp);

**4. Use appropriate alignment**:

.. code-block:: cpp

   // For AVX2 (256-bit)
   alignas(32) float data[8];

   // For AVX-512 (512-bit)
   alignas(64) float data[16];

Memory Safety
-------------

Simdette provides type-safe memory operations:

.. code-block:: cpp

   // Checked alignment
   alignas(32) float data[8];

   // Safe load
   auto v = batch_t::load_aligned(data);

   // Bounds-checked access
   float val = v.extract(0);  // Valid: 0 < width
   // float val = v.extract(10);  // Invalid: 10 >= width

Best Practices
--------------

1. **Always align memory** for SIMD operations when performance matters
2. **Use ``aligned_array``** for automatic cleanup
3. **Prefer aligned loads/stores** over unaligned when possible
4. **Minimize memory round-trips** (batch → array → batch)
5. **Consider cache-friendly access patterns** (sequential > random)
6. **Use ``store_unaligned``** for temporary results
7. **Batch operations** are faster than scalar loops

Example: Efficient Matrix Multiplication
----------------------------------------

.. code-block:: cpp

   void matmul(const float* A, const float* B, float* C,
               std::size_t rows, std::size_t cols, std::size_t depth) {
       using f32x8 = simdette::batch<float, simdette::avx2>;
       constexpr std::size_t width = simdette::detail::batch_width_v<float, simdette::avx2>;

       // Aligned output
       auto C_aligned = simdette::make_aligned_array<float>(rows * cols);

       for (std::size_t i = 0; i < rows; i += width) {
           for (std::size_t j = 0; j < cols; j += width) {
               f32x8 sum(0.0f);
               for (std::size_t k = 0; k < depth; ++k) {
                   f32x8 a_val(A[i * depth + k]);
                   f32x8 b_val(B[k * cols + j]);
                   sum = sum + (a_val * b_val);
               }
               sum.store_aligned(C_aligned.data() + i * cols + j);
           }
       }

       // Copy to output
       std::memcpy(C, C_aligned.data(), rows * cols * sizeof(float));
   }
