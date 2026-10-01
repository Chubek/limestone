Architecture Support
====================

Simdette automatically detects and uses the best available SIMD architecture
at compile time. This chapter explains each supported architecture and how
to control architecture selection.

Automatic Architecture Detection
--------------------------------

By default, Simdette uses ``default_arch``, which is a typedef that resolves
to the best available architecture:

.. code-block:: cpp

   #include "simdette/simdette.hpp"

   // Automatically uses best architecture
   using f32xN = simdette::batch<float, simdette::default_arch>;

Detection Order
~~~~~~~~~~~~~~~

Simdette checks for architectures in this order:

1. **AVX-512** (if ``__AVX512F__`` is defined)
2. **AVX2** (if ``__AVX2__`` is defined)
3. **AVX** (if ``__AVX__`` is defined)
4. **SSE** (if ``__SSE2__`` or x86_64)
5. **SVE** (if ``__ARM_FEATURE_SVE``)
6. **NEON** (if ``__ARM_NEON``)
7. **SIMD128** (if ``__wasm_simd128__``)
8. **Scalar** (fallback)

Architecture Selection via Compiler Flags
-----------------------------------------

**GCC/Clang**:

.. code-block:: bash

   # Use native architecture
   g++ -std=c++20 -march=native -O2 ...

   # Force specific architecture
   g++ -std=c++20 -msse2 -O2 ...
   g++ -std=c++20 -mavx -O2 ...
   g++ -std=c++20 -mavx2 -O2 ...
   g++ -std=c++20 -mavx512f -O2 ...
   g++ -std=c++20 -mneon -O2 ...

**MSVC**:

.. code-block:: batch

   # Visual Studio project settings
   # C/C++ -> Code Generation -> Enable Enhanced Instructions

   # SSE2 (default on x64)
   # Streaming SIMD Extensions 2

   # AVX
   # Streaming SIMD Extensions 3

   # AVX2
   # Advanced Vector Extensions 2

Manual Architecture Selection
-----------------------------

You can manually select an architecture:

.. code-block:: cpp

   #include "simdette/simdette.hpp"

   // Force SSE
   using f32x4 = simdette::batch<float, simdette::sse>;

   // Force AVX
   using f32x8 = simdette::batch<float, simdette::avx>;

   // Force AVX2
   using f32x8 = simdette::batch<float, simdette::avx2>;

   // Force AVX-512
   using f32x16 = simdette::batch<float, simdette::avx512>;

   // Force NEON
   using f32x4 = simdette::batch<float, simdette::neon>;

   // Force SVE
   using f32xN = simdette::batch<float, simdette::sve>;

   // Force SIMD128
   using f32x4 = simdette::batch<float, simdette::simd128>;

   // Force scalar
   using f32x1 = simdette::batch<float, simdette::scalar>;

Architecture-Specific Details
-----------------------------

### SSE (Streaming SIMD Extensions)

- **Vector width**: 128 bits (4 floats, 2 doubles, 4 int32)
- **Header**: ``<xmmintrin.h>``
- **Requirements**: SSE2 (all x86_64, enable with ``-msse2``)
- **Use case**: Legacy support, universal compatibility

.. code-block:: cpp

   using f32x4 = simdette::batch<float, simdette::sse>;
   // Width: 4 elements

### AVX (Advanced Vector Extensions)

- **Vector width**: 256 bits (8 floats, 4 doubles, 8 int32)
- **Header**: ``<immintrin.h>``
- **Requirements**: AVX (``-mavx``, Intel Sandy Bridge+, AMD Bulldozer+)
- **Use case**: Modern x86 performance

.. code-block:: cpp

   using f32x8 = simdette::batch<float, simdette::avx>;
   // Width: 8 elements

### AVX2 (Advanced Vector Extensions 2)

- **Vector width**: 256 bits (8 floats, 4 doubles, 8 int32)
- **Header**: ``<immintrin.h>``
- **Requirements**: AVX2 (``-mavx2``, Intel Haswell+, AMD Excavator+)
- **Features**: Integer SIMD, gather/scatter
- **Use case**: Best for general-purpose x86 SIMD

.. code-block:: cpp

   using f32x8 = simdette::batch<float, simdette::avx2>;
   using i32x8 = simdette::batch<int32_t, simdette::avx2>;

### AVX-512 (Advanced Vector Extensions 512)

- **Vector width**: 512 bits (16 floats, 8 doubles, 16 int32)
- **Header**: ``<immintrin.h>``
- **Requirements**: AVX-512 Foundation (``-mavx512f``, Intel Skylake+, AMD Zen4+)
- **Use case**: High-throughput workloads, data processing
- **Note**: May have lower clock speeds on some CPUs

.. code-block:: cpp

   using f32x16 = simdette::batch<float, simdette::avx512>;
   using f64x8 = simdette::batch<double, simdette::avx512>;

### NEON (ARM SIMD)

- **Vector width**: 128 bits (4 floats, 2 doubles, 4 int32)
- **Header**: ``<arm_neon.h>``
- **Requirements**: ARMv7-A with NEON (``-march=armv7-a+neon``)
- **Use case**: ARM mobile, embedded systems

.. code-block:: cpp

   using f32x4 = simdette::batch<float, simdette::neon>;
   // Width: 4 elements

### SVE (Scalable Vector Extension)

- **Vector width**: **Runtime-determined** (128-2048 bits, multiples of 128)
- **Header**: ``<arm_sve.h>``
- **Requirements**: ARMv8.2+ (``-march=armv8-a+sqrt+sve``)
- **Features**: Variable-length vectors, predicate masking
- **Use case**: ARM server, future-proof ARM code

.. code-block:: cpp

   using f32xN = simdette::batch<float, simdette::sve>;
   // Width: Runtime-determined by hardware

### SIMD128 (WebAssembly)

- **Vector width**: 128 bits (4 floats, 2 doubles, 4 int32)
- **Header**: ``<wasm_simd128.h>``
- **Requirements**: WebAssembly SIMD128 proposal
- **Use case**: WebAssembly, browser-based SIMD

.. code-block:: cpp

   using f32x4 = simdette::batch<float, simdette::simd128>;

### Scalar (Fallback)

- **Vector width**: 1 (single element)
- **Header**: None (pure C++)
- **Requirements**: None
- **Use case**: Portability, debugging, non-SIMD platforms

.. code-block:: cpp

   using f32x1 = simdette::batch<float, simdette::scalar>;

Portability Tips
----------------

**Write portable code**:

.. code-block:: cpp

   #include "simdette/simdette.hpp"

   // Use default_arch for automatic detection
   template <typename T>
   void process(const T* data, std::size_t n) {
       using batch_t = simdette::batch<T, simdette::default_arch>;
       // Code works on any architecture
   }

**Multi-architecture builds**:

.. code-block:: bash

   # Build for multiple architectures
   g++ -std=c++20 -march=native -O3 ...  # Native
   g++ -std=c++20 -msse2 -O3 ...         # SSE2 fallback
   g++ -std=c++20 -mno-avx -O3 ...       # AVX-disabled

Runtime Dispatch
----------------

For runtime architecture selection, create multiple implementations:

.. code-block:: cpp

   void dot_product_sse(const float* a, const float* b, float* c, int n);
   void dot_product_avx2(const float* a, const float* b, float* c, int n);

   void dot_product_dispatch(const float* a, const float* b, float* c, int n) {
       #if defined(__AVX2__)
           dot_product_avx2(a, b, c, n);
       #else
           dot_product_sse(a, b, c, n);
       #endif
   }

Performance Notes
-----------------

- **AVX-512** provides highest throughput but may have lower clocks
- **AVX2** offers best balance for most workloads
- **SSE** is universally available but slower
- **NEON** is essential for ARM mobile
- **Scalar** ensures code runs everywhere (fallback)

Testing Architecture
--------------------

Check which architecture is being used:

.. code-block:: cpp

   #include "simdette/simdette.hpp"
   #include <iostream>

   int main() {
       std::cout << "Float batch width: "
                 << simdette::detail::batch_width_v<float, simdette::default_arch>
                 << "\n";
       return 0;
   }
