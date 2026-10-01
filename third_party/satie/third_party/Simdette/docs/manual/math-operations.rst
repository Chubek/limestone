Math Operations
===============

Simdette provides both arithmetic operators and specialized math functions
for vectorized computation.

Arithmetic Operators
--------------------

All batch types support standard arithmetic operators:

.. code-block:: cpp

   batch<float, simdette::avx2> a{1, 2, 3, 4, 5, 6, 7, 8};
   batch<float, simdette::avx2> b{10, 20, 30, 40, 50, 60, 70, 80};

   // Element-wise operations
   auto sum = a + b;       // [11, 22, 33, 44, 55, 66, 77, 88]
   auto diff = a - b;      // [-9, -18, -27, ...]
   auto prod = a * b;      // [10, 40, 90, 160, 250, 360, 490, 640]
   auto quot = a / b;      // [0.1, 0.1, 0.1, ...]

**Compound assignment**:

.. code-block:: cpp

   a += b;    // a = a + b
   a -= b;    // a = a - b
   a *= b;    // a = a * b
   a /= b;    // a = a / b

**Scalar operations**:

.. code-block:: cpp

   auto scaled = a * 2.0f;    // [2, 4, 6, 8, 10, 12, 14, 16]
   auto offset = a + 1.0f;    // [2, 3, 4, 5, 6, 7, 8, 9]

Comparison Operators
--------------------

Comparisons return a **mask** (all bits set for true, 0 for false):

.. code-block:: cpp

   batch<float, simdette::avx2> a{1, 5, 3, 7, 2, 6, 4, 8};

   auto lt = a < batch<float, simdette::avx2>{4};  // [1, 0, 1, 0, 1, 0, 1, 0]
   auto gt = a > batch<float, simdette::avx2>{4};  // [0, 1, 0, 1, 0, 1, 0, 1]
   auto eq = a == batch<float, simdette::avx2>{4}; // [0, 0, 0, 0, 0, 0, 1, 0]

**All available comparisons**:

- ``operator<``  : Less than
- ``operator<=`` : Less than or equal
- ``operator>``  : Greater than
- ``operator>=`` : Greater than or equal
- ``operator==`` : Equal
- ``operator!=`` : Not equal

Bitwise Operators (Integer Types)
---------------------------------

.. code-block:: cpp

   batch<int32_t, simdette::avx2> a{0xF0F0F0F0, 0x0F0F0F0F, ...};
   batch<int32_t, simdette::avx2> b{0x0F0F0F0F, 0xF0F0F0F0, ...};

   auto andv = a & b;      // Bitwise AND
   auto orv = a | b;       // Bitwise OR
   auto xorv = a ^ b;      // Bitwise XOR
   auto notv = ~a;         // Bitwise NOT

**Compound assignment**:

.. code-block:: cpp

   a &= b;
   a |= b;
   a ^= b;

Logical Operators (Boolean/Integer)
-----------------------------------

.. code-block:: cpp

   batch<int32_t, simdette::avx2> a{1, 0, 1, 0, 1, 0, 1, 0};

   auto andv = a && batch<int32_t, simdette::avx2>{1};  // Logical AND
   auto orv = a || batch<int32_t, simdette::avx2>{0};   // Logical OR
   auto notv = !a;                                      // Logical NOT

Unary Operators
---------------

.. code-block:: cpp

   batch<float, simdette::avx2> a{1, 2, 3, 4, 5, 6, 7, 8};

   auto neg = -a;   // [-1, -2, -3, -4, -5, -6, -7, -8]
   auto pos = +a;   // [1, 2, 3, 4, 5, 6, 7, 8] (no-op)

Specialized Math Functions
--------------------------

Include the math header:

.. code-block:: cpp

   #include "simdette/math/basic_math.hpp"

### Elementary Functions

**Absolute value**:

.. code-block:: cpp

   batch<float, simdette::avx2> a{-3, -2, -1, 0, 1, 2, 3, 4};
   auto abs = simdette::abs(a);  // [3, 2, 1, 0, 1, 2, 3, 4]

**Square root**:

.. code-block:: cpp

   batch<float, simdette::avx2> a{1, 4, 9, 16, 25, 36, 49, 64};
   auto sqrt = simdette::sqrt(a);  // [1, 2, 3, 4, 5, 6, 7, 8]

**Reciprocal square root** (fast approximation):

.. code-block:: cpp

   batch<float, simdette::avx2> a{1, 4, 9, 16, 25, 36, 49, 64};
   auto rsqrt = simdette::rsqrt(a);  // [1, 0.5, 0.333, 0.25, ...]

**Floor** (round down):

.. code-block:: cpp

   batch<float, simdette::avx2> a{1.7, 2.3, 3.9, 4.1, ...};
   auto floored = simdette::floor(a);  // [1, 2, 3, 4, ...]

**Ceil** (round up):

.. code-block:: cpp

   auto ceiled = simdette::ceil(a);  // [2, 3, 4, 5, ...]

**Round** (nearest integer):

.. code-block:: cpp

   auto rounded = simdette::round(a);  // [2, 2, 4, 4, ...]

**Truncate** (round toward zero):

.. code-block:: cpp

   auto truncated = simdette::trunc(a);

**Minimum/Maximum**:

.. code-block:: cpp

   batch<float, simdette::avx2> a{1, 5, 3, 7, 2, 6, 4, 8};
   batch<float, simdette::avx2> b{4, 2, 6, 1, 5, 3, 8, 0};

   auto minv = simdette::min(a, b);   // [1, 2, 3, 1, 2, 3, 4, 0]
   auto maxv = simdette::max(a, b);   // [4, 5, 6, 7, 5, 6, 8, 8]

**Clamp** ( constrain to range):

.. code-block:: cpp

   batch<float, simdette::avx2> a{-1, 0, 5, 10, 15, 20};
   auto lo = batch<float, simdette::avx2>{0};
   auto hi = batch<float, simdette::avx2>{10};

   auto clamped = simdette::clamp(a, lo, hi);  // [0, 0, 5, 10, 10, 10]

**Is NaN / Is Inf**:

.. code-block:: cpp

   batch<float, simdette::avx2> a{1, 2, std::numeric_limits<float>::quiet_NaN(), 4};

   auto isnan = simdette::isnan(a);  // [0, 0, 1, 0]
   auto isinf = simdette::isinf(a);  // [0, 0, 0, 0]

Function Overloads
------------------

All math functions are overloaded for:

1. **Scalar types** (single values)
2. **Batch types** (vectorized)

.. code-block:: cpp

   // Scalar
   float x = 4.0f;
   float y = simdette::sqrt(x);  // 2.0f

   // Batch
   simdette::batch<float, simdette::avx2> v{4, 9, 16, 25, 36, 49, 64, 81};
   auto result = simdette::sqrt(v);  // Vectorized

Transcendental Functions
------------------------

The ``transcendental.hpp`` header provides advanced functions:

.. code-block:: cpp

   #include "simdette/math/transcendental.hpp"

   batch<float, simdette::avx2> a{1, 2, 3, 4, 5, 6, 7, 8};

   auto sinv = simdette::sin(a);
   auto cosv = simdette::cos(a);
   auto tanv = simdette::tan(a);
   auto expv = simdette::exp(a);
   auto logv = simdette::log(a);
   auto powv = simdette::pow(a, batch<float, simdette::avx2>{2});

Note: Transcendental functions are implemented via table lookup or
polynomial approximation for performance.

Expression Templates
--------------------

Simdette supports expression templates for efficient composition:

.. code-block:: cpp

   batch<float, simdette::avx2> a{1, 2, 3, 4, 5, 6, 7, 8};
   batch<float, simdette::avx2> b{10, 20, 30, 40, 50, 60, 70, 80};

   // Complex expression (optimized)
   auto result = (a * a) + (2 * a * b) + (b * b);  // (a + b)^2

   // No temporary batches created
   // All operations fused where possible

Performance Notes
-----------------

- **Element-wise operations** (``+``, ``-``, ``*``, ``/``) map directly to intrinsics
- **Transcendental functions** use polynomial approximations
- **Reduction operations** (``reduce_add``, etc.) may be slower
- **Scalar conversions** (``extract``, ``to_array``) require memory access

Best Practice
-------------

For performance-critical code, prefer intrinsic-level operations
over function calls where possible:

.. code-block:: cpp

   // Good: Direct operations
   auto result = a * a + b * b;

   // Less efficient: Function calls
   auto result = simdette::add(simdette::mul(a, a), simdette::mul(b, b));
