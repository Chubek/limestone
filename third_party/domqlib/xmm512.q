# xmm512.q — 16x float32 SIMD vector (AVX512F; compile with -mavx512f)

@module  xmm512
@version 1.0

@param PREFIX xmm512

@include <immintrin.h>

@guard

@struct {
    typedef __m512 ${PREFIX}_t;
}

@fn load {
    static inline ${PREFIX}_t
    ${PREFIX}_load(const float *p) {
        return _mm512_loadu_ps(p);
    }
}

@fn store {
    static inline void
    ${PREFIX}_store(${PREFIX}_t v, float *p) {
        _mm512_storeu_ps(p, v);
    }
}

@fn zero {
    static inline ${PREFIX}_t
    ${PREFIX}_zero(void) {
        return _mm512_setzero_ps();
    }
}

@fn splat {
    static inline ${PREFIX}_t
    ${PREFIX}_splat(float x) {
        return _mm512_set1_ps(x);
    }
}

@fn add {
    static inline ${PREFIX}_t
    ${PREFIX}_add(${PREFIX}_t a, ${PREFIX}_t b) {
        return _mm512_add_ps(a, b);
    }
}

@fn sub {
    static inline ${PREFIX}_t
    ${PREFIX}_sub(${PREFIX}_t a, ${PREFIX}_t b) {
        return _mm512_sub_ps(a, b);
    }
}

@fn mul {
    static inline ${PREFIX}_t
    ${PREFIX}_mul(${PREFIX}_t a, ${PREFIX}_t b) {
        return _mm512_mul_ps(a, b);
    }
}

@fn div {
    static inline ${PREFIX}_t
    ${PREFIX}_div(${PREFIX}_t a, ${PREFIX}_t b) {
        return _mm512_div_ps(a, b);
    }
}
