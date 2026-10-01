# xmm128.q — 4x float32 SIMD vector (SSE2 baseline, always present
# on x86-64, so no special compiler flags are required)

@module  xmm128
@version 1.0

@param PREFIX xmm128

@include <immintrin.h>

@guard

@struct {
    typedef __m128 ${PREFIX}_t;
}

@fn load {
    static inline ${PREFIX}_t
    ${PREFIX}_load(const float *p) {
        return _mm_loadu_ps(p);
    }
}

@fn store {
    static inline void
    ${PREFIX}_store(${PREFIX}_t v, float *p) {
        _mm_storeu_ps(p, v);
    }
}

@fn zero {
    static inline ${PREFIX}_t
    ${PREFIX}_zero(void) {
        return _mm_setzero_ps();
    }
}

@fn splat {
    static inline ${PREFIX}_t
    ${PREFIX}_splat(float x) {
        return _mm_set1_ps(x);
    }
}

@fn add {
    static inline ${PREFIX}_t
    ${PREFIX}_add(${PREFIX}_t a, ${PREFIX}_t b) {
        return _mm_add_ps(a, b);
    }
}

@fn sub {
    static inline ${PREFIX}_t
    ${PREFIX}_sub(${PREFIX}_t a, ${PREFIX}_t b) {
        return _mm_sub_ps(a, b);
    }
}

@fn mul {
    static inline ${PREFIX}_t
    ${PREFIX}_mul(${PREFIX}_t a, ${PREFIX}_t b) {
        return _mm_mul_ps(a, b);
    }
}

@fn div {
    static inline ${PREFIX}_t
    ${PREFIX}_div(${PREFIX}_t a, ${PREFIX}_t b) {
        return _mm_div_ps(a, b);
    }
}
