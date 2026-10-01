Examples
========

This section contains complete working examples of using Simdette.

Example 1: Basic Vector Operations
-----------------------------------

.. literalinclude:: ../../../examples/example_basics.cpp
   :language: cpp
   :caption: Basic operations demonstration

Example 2: Mathematical Functions
----------------------------------

.. literalinclude:: ../../../examples/example_math.cpp
   :language: cpp
   :caption: Math functions demonstration

Example 3: Dot Product
----------------------

.. literalinclude:: ../../../examples/example_dot_product.cpp
   :language: cpp
   :caption: Vector dot product with benchmarking

Example 4: Mandelbrot Set Generation
-------------------------------------

.. literalinclude:: ../../../examples/example_mandelbrot.cpp
   :language: cpp
   :caption: Fractal generation demonstration

Example 5: Comprehensive Test Suite
------------------------------------

.. literalinclude:: ../../../tests/test_comprehensive.cpp
   :language: cpp
   :caption: Comprehensive test suite

Building the Examples
---------------------

Using CMake:

.. code-block:: bash

   mkdir build && cd build
   cmake .. -DSIMDETTE_BUILD_EXAMPLES=ON
   make

Using Makefile:

.. code-block:: bash

   make examples

Using Meson:

.. code-block:: bash

   meson setup build
   meson compile -C build

Using Ninja:

.. code-block:: bash

   ninja

Running the Examples
--------------------

.. code-block:: bash

   # Basic operations
   ./example_basics

   # Math functions
   ./example_math

   # Dot product
   ./example_dot_product

   # Mandelbrot
   ./example_mandelbrot

Expected Output (example_basics)
--------------------------------

.. code-block:: text

   === Simdette Basic Operations ===

   1. Batch Creation
   -----------------
   a_broadcast : 3.14, 3.14, 3.14, 3.14
   a_load      : 1.00, 2.00, 3.00, 4.00

   2. Arithmetic Operations
   -------------------------
   a           : 1.00, 2.00, 3.00, 4.00
   b           : 5.00, 6.00, 7.00, 8.00
   a + b       : 6.00, 8.00, 10.00, 12.00
   a - b       : -4.00, -4.00, -4.00, -4.00
   a * b       : 5.00, 12.00, 21.00, 32.00
   a / b       : 0.20, 0.33, 0.43, 0.50
   c += b      : 6.00, 8.00, 10.00, 12.00

   3. Scalar Operations
   ---------------------
   a * 2.0f    : 2.00, 4.00, 6.00, 8.00
   a + 1.0f    : 2.00, 3.00, 4.00, 5.00
   b / 2.0f    : 2.50, 3.00, 3.50, 4.00

   ... (more output)

Example Usage Patterns
----------------------

### Pattern 1: Element-wise Operations

.. code-block:: cpp

   // Apply operation to entire vector
   auto result = a * b + c;

### Pattern 2: Reduction

.. code-block:: cpp

   // Sum all elements
   float total = vector.reduce_add();

### Pattern 3: Conditional Processing

.. code-block:: cpp

   // Use comparison as mask
   auto mask = a > threshold;
   // Process based on mask

### Pattern 4: Memory-bound Operations

.. code-block:: cpp

   // Load, process, store
   auto v = batch_t::load_aligned(data);
   auto result = simdute::sqrt(v);
   result.store_aligned(output);

Troubleshooting
---------------

### Problem: Segmentation fault

**Solution**: Ensure memory is properly aligned:

.. code-block:: cpp

   alignas(32) float data[8];  // For AVX2

### Problem: Incorrect results

**Solution**: Verify against scalar implementation:

.. code-block:: cpp

   // Compare with scalar version
   bool equal = verify_simd_vs_scalar(simd_result, scalar_result);

### Problem: Poor performance

**Solution**: Check architecture detection:

.. code-block:: bash

   g++ -std=c++20 -march=native -O3 your_code.cpp

Next Steps
----------

- Study the example source code
- Run the tests to verify your implementation
- Read the :doc:`best-practices` guide for optimization tips
- Explore the :doc:`core-concepts` documentation for deeper understanding
