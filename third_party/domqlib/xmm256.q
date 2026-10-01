# xmm256.q — 8x float32 SIMD vector (AVX; compile with -mavx)

@module  xmm256
@version 1.0

@param PREFIX xmm256

@include <immintrin.h>

@guard

@struct {
    typedef __m256 ${PREFIX}_t;
}

@fn load {
    static inline ${PREFIX}_t
    ${PREFIX}_load(const float *p) {
        return _mm256_loadu_ps(p);
    }
}

@fn store {
    static inline void
    ${PREFIX}_store(${PREFIX}_t v, float *p) {
        _mm256_storeu_ps(p, v);
    }
}

@fn zero {
    static inline ${PREFIX}_t
    ${PREFIX}_zero(void) {
        return _mm256_setzero_ps();
    }
}

@fn splat {
    static inline ${PREFIX}_t
    ${PREFIX}_splat(float x) {
        return _mm256_set1_ps(x);
    }
}

@fn add {
    static inline ${PREFIX}_t
    ${PREFIX}_add(${PREFIX}_t a, ${PREFIX}_t b) {
        return _mm256_add_ps(a, b);
    }
}

@fn sub {
    static inline ${PREFIX}_t
    ${PREFIX}_sub(${PREFIX}_t a, ${PREFIX}_t b) {
        return _mm256_sub_ps(a, b);
    }
}

@fn mul {
    static inline ${PREFIX}_t
    ${PREFIX}_mul(${PREFIX}_t a, ${PREFIX}_t b) {
        return _mm256_mul_ps(a, b);
    }
}

@fn div {
    static inline ${PREFIX}_t
    ${PREFIX}_div(${PREFIX}_t a, ${PREFIX}_t b) {
        return _mm256_div_ps(a, b);
    }
}
