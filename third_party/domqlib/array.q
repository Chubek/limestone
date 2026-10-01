# array.q — fixed-size array wrapper with bounds checking

@module  array
@version 1.0

@param T
@param N 16
@param PREFIX array

@require {${N} > 0} "N must be positive"

@include <assert.h>

@guard

@raw {
    #define ${PREFIX}_LEN (${N})
}

@struct {
    typedef struct {
        ${T} items[${PREFIX}_LEN];
    } ${PREFIX}_t;
}

@fn get {
    static inline ${T} *
    ${PREFIX}_get(${PREFIX}_t *a, int i) {
        assert(i >= 0 && i < ${PREFIX}_LEN);
        return &a->items[i];
    }
}

@fn set {
    static inline void
    ${PREFIX}_set(${PREFIX}_t *a, int i, ${T} x) {
        assert(i >= 0 && i < ${PREFIX}_LEN);
        a->items[i] = x;
    }
}

@fn fill {
    static inline void
    ${PREFIX}_fill(${PREFIX}_t *a, ${T} x) {
        int i;
        for (i = 0; i < ${PREFIX}_LEN; i++) {
            a->items[i] = x;
        }
    }
}

@fn len {
    static inline int
    ${PREFIX}_len(const ${PREFIX}_t *a) {
        (void)a;
        return ${PREFIX}_LEN;
    }
}
