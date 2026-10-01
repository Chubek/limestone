# bit.q — fixed-size bitset

@module  bit
@version 1.0

@param N 128
@param PREFIX bit

@require {${N} > 0} "N must be positive"

@include <string.h>
@include <assert.h>

@guard

@raw {
    #define ${PREFIX}_N (${N})
    #define ${PREFIX}_WORDS ((unsigned)(${PREFIX}_N + 31) / 32u)
}

@struct {
    typedef struct {
        unsigned w[${PREFIX}_WORDS];
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *b) {
        memset(b->w, 0, sizeof(b->w));
    }
}

@fn set {
    static inline void
    ${PREFIX}_set(${PREFIX}_t *b, int i) {
        assert(i >= 0 && i < ${PREFIX}_N);
        b->w[i / 32] |= (1u << (i % 32));
    }
}

@fn clr {
    static inline void
    ${PREFIX}_clr(${PREFIX}_t *b, int i) {
        assert(i >= 0 && i < ${PREFIX}_N);
        b->w[i / 32] &= ~(1u << (i % 32));
    }
}

@fn test {
    static inline int
    ${PREFIX}_test(const ${PREFIX}_t *b, int i) {
        assert(i >= 0 && i < ${PREFIX}_N);
        return (b->w[i / 32] & (1u << (i % 32))) != 0;
    }
}

@fn count {
    static inline int
    ${PREFIX}_count(const ${PREFIX}_t *b) {
        unsigned k;
        int total = 0;
        for (k = 0; k < ${PREFIX}_WORDS; k++) {
            unsigned x = b->w[k];
            while (x) {
                x &= x - 1u;
                total++;
            }
        }
        return total;
    }
}

@fn empty {
    static inline int
    ${PREFIX}_empty(const ${PREFIX}_t *b) {
        unsigned k;
        for (k = 0; k < ${PREFIX}_WORDS; k++) {
            if (b->w[k]) {
                return 0;
            }
        }
        return 1;
    }
}
