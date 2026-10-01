Quick Reference
===============

Type Reference
--------------

**Floating Point Types:**

.. code-block:: cpp

   using f32xN = simdette::batch<float, simdette::default_arch>;
   using f64xN = simdette::batch<double, simdette::default_arch>;

**Integer Types:**

.. code-block:: cpp

   using i32xN = simdette::batch<int32_t, simdette::default_arch>;
   using u32xN = simdette::batch<uint32_t, simdette::default_arch>;
   using i64xN = simdette::batch<int64_t, simdette::default_arch>;

**Explicit Architectures:**

.. code-block:: cpp

   using f32x4_sse = simdette::batch<float, simdette::sse>;
   using f32x8_avx = simdette::batch<float, simdette::avx>;
   using f32x8_avx2 = simdette::batch<float, simdette::avx2>;
   using f32x16_avx512 = simdette::batch<float, simdette::avx512>;

Basic Operations
----------------

**Create batch:**

.. code-block:: cpp

   batch<float, arch> v;                    // Default (uninitialized)
   batch<float, arch> v(3.14f);             // Broadcast
   batch<float, arch> v(data);              // Load from array
   batch<float, arch> v = batch::load_aligned(data);

**Arithmetic:**

.. code-block:: cpp

   v + u    // Addition
   v - u    // Subtraction
   v * u    // Multiplication
   v / u    // Division

**Comparison:**

.. code-block:: cpp

   v < u    // Less than
   v <= u   // Less or equal
   v > u    // Greater than
   v >= u   // Greater or equal
   v == u   // Equal
   v != u   // Not equal

**Reduction:**

.. code-block:: cpp

   v.reduce_add()   // Sum all elements
   v.reduce_min()   // Minimum element
   v.reduce_max()   // Maximum element

Memory Operations
-----------------

**Load:**

.. code-block:: cpp

   batch<float, arch> v = batch::load_aligned(ptr);
   batch<float, arch> v = batch::load_unaligned(ptr);

**Store:**

.. code-block:: cpp

   v.store_aligned(ptr);
   v.store_unaligned(ptr);

**Extract:**

.. code-block:: cpp

   float value = v.extract(0);
   std::array<float, N> data;
   v.to_array(data.data());

Math Functions
--------------

**Elementary:**

.. code-block:: cpp

   simdette::abs(v)      // Absolute value
   simdette::sqrt(v)     // Square root
   simdette::rsqrt(v)    // Reciprocal square root

**Rounding:**

.. code-block:: cpp

   simdette::floor(v)    // Round down
   simdette::ceil(v)     // Round up
   simdette::round(v)    // Round to nearest
   simdette::trunc(v)    // Truncate

**Min/Max:**

.. code-block:: cpp

   simdette::min(a, b)   // Element-wise minimum
   simdette::max(a, b)   // Element-wise maximum
   simdette::clamp(v, lo, hi)  // Clamp to range

**Special:**

.. code-block:: cpp

   simdette::isnan(v)    // Check for NaN
   simdette::isinf(v)    // Check for infinity

Type Traits
-----------

.. code-block:: cpp

   simdette::is_simd_compatible_v<T>
   simdette::is_floating_point_v<T>
   simdette::is_integral_v<T>
   simdette::bit_width_v<T>
   simdette::simd_alignment_v<T>
   simdette::vector_256_count_v<T>
   simdette::vector_512_count_v<T>

Alignment Constants
-------------------

.. code-block:: cpp

   simdette::Alignment        // 32 bytes (default)
   simdette::AVX512_Alignment // 64 bytes

Best Practices
--------------

1. **Use default_arch** for automatic architecture detection
2. **Align data** for optimal performance
3. **Initialize batches** before use
4. **Verify results** against scalar implementation
5. **Profile performance** with real workloads

Build Commands
--------------

**CMake:**

.. code-block:: bash

   cmake -S . -B build -DSIMDETTE_BUILD_TESTS=ON
   cmake --build build
   ctest --test-dir build

**Make:**

.. code-block:: bash

   make
   make tests
   make clean

**Meson:**

.. code-block:: bash

   meson setup build
   meson compile -C build

Command Line Flags
------------------

**GCC/Clang:**

.. code-block:: bash

   -std=c++20          // C++20 support
   -msse2              // Enable SSE2
   -mavx               // Enable AVX
   -mavx2              // Enable AVX2
   -mavx512f           // Enable AVX-512
   -march=native       // Optimize for current CPU
   -O2                 // Optimization level 2
   -O3                 // Optimization level 3

Header Files
------------

Core headers:

.. code-block:: cpp

   #include "simdette/simdette.hpp"     // Main header
   #include "simdette/core/batch.hpp"   // Batch operations
   #include "simdette/core/traits.hpp"  // Type traits
   #include "simdette/core/memory.hpp"  // Memory utilities
   #include "simdette/core/operators.hpp" // Operators

Math headers:

.. code-block:: cpp

   #include "simdette/math/basic_math.hpp"
   #include "simdette/math/transcendental.hpp"

Architecture headers:

.. code-block:: cpp

   #include "simdette/arch/x86/avx2.hpp"
   #include "simdette/arch/arm/neon.hpp"
   // etc.

Common Pitfalls
---------------

**Bad:** Uninitialized batch

.. code-block:: cpp

   batch<float, arch> v;
   auto result = v * other;  // Undefined behavior!

**Good:** Initialize first

.. code-block:: cpp

   batch<float, arch> v(0.0f);
   auto result = v * other;

**Bad:** Misaligned pointer

.. code-block:: cpp

   float* data = new float[8];
   auto v = batch::load_aligned(data);  // May crash!

**Good:** Proper alignment

.. code-block:: cpp

   alignas(32) float* data = new float[8];
   auto v = batch::load_aligned(data);
