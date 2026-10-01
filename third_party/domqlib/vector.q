# vector.q — generic dynamic array (malloc-backed)

@module  vector
@version 1.0

@param T
@param PREFIX vector

@include <stdlib.h>
@include <assert.h>
@include <limits.h>
@include <stdint.h>

@guard

@struct {
    typedef struct {
        ${T} *data;
        int  len;
        int  cap;
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *v) {
        v->data = 0;
        v->len  = 0;
        v->cap  = 0;
    }
}

@fn reserve {
    /* Ensure room for at least n elements. Returns 1 on success, 0 on OOM. */
    static inline int
    ${PREFIX}_reserve(${PREFIX}_t *v, int n) {
        if (n < 0 || (size_t)n > SIZE_MAX / sizeof(${T})) return 0;
        if (n > v->cap) {
            int ncap = v->cap ? v->cap : 4;
            ${T} *nd;
            while (ncap < n) {
                if (ncap > INT_MAX / 2) { ncap = n; break; }
                ncap *= 2;
            }
            if ((size_t)ncap > SIZE_MAX / sizeof(${T})) return 0;
            nd = (${T} *)realloc(v->data, sizeof(${T}) * (size_t)ncap);
            if (!nd) {
                return 0;
            }
            v->data = nd;
            v->cap  = ncap;
        }
        return 1;
    }
}

@fn push {
    static inline void
    ${PREFIX}_push(${PREFIX}_t *v, ${T} x) {
        if (v->len == INT_MAX || !${PREFIX}_reserve(v, v->len + 1)) abort();
        v->data[v->len++] = x;
    }
}

@fn pop {
    static inline ${T}
    ${PREFIX}_pop(${PREFIX}_t *v) {
        assert(v->len > 0);
        return v->data[--v->len];
    }
}

@fn get {
    static inline ${T} *
    ${PREFIX}_get(${PREFIX}_t *v, int i) {
        assert(i >= 0 && i < v->len);
        return &v->data[i];
    }
}

@fn len {
    static inline int
    ${PREFIX}_len(const ${PREFIX}_t *v) {
        return v->len;
    }
}

@fn clear {
    static inline void
    ${PREFIX}_clear(${PREFIX}_t *v) {
        v->len = 0;
    }
}

@fn free {
    static inline void
    ${PREFIX}_free(${PREFIX}_t *v) {
        free(v->data);
        v->data = 0;
        v->len  = 0;
        v->cap  = 0;
    }
}
