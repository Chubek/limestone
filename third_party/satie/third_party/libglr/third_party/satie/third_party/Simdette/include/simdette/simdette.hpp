#pragma once

// Root inclusion header for Simdette
#include "setup/config.hpp"
#include "setup/detect_arch.hpp"

#if defined(SIMDETTE_ARCH_AVX512) || defined(SIMDETTE_ARCH_AVX2) || defined(SIMDETTE_ARCH_AVX) || defined(SIMDETTE_ARCH_SSE)
#include "arch/x86/x86.hpp"
#elif defined(SIMDETTE_ARCH_SVE) || defined(SIMDETTE_ARCH_NEON)
#include "arch/arm/arm.hpp"
#elif defined(SIMDETTE_ARCH_WASM_SIMD128)
#include "arch/wasm/wasm.hpp"
#else
#include "arch/scalar/scalar.hpp"
#endif

#include "core/traits.hpp"
#include "core/batch.hpp"
#include "core/memory.hpp"
#include "core/operators.hpp"

#include "math/basic_math.hpp"
#include "math/transcendental.hpp"
#include "math/linalg.hpp"
