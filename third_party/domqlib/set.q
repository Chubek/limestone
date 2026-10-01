# set.q — sorted dynamic-array set with binary search.
# CMP is a function name: int CMP(T a, T b), negative when a < b,
# zero when equal, positive when a > b. Elements stored by value.

@module  set
@version 1.0

@param T
@param CMP
@param PREFIX set

@require {[string length ${CMP}] > 0} "CMP must name a compare function"

@include <stdlib.h>
@include <string.h>
@include <assert.h>

@guard

@struct {
    typedef struct {
        ${T} *items;
        int  len;
        int  cap;
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *s) {
        s->items = 0;
        s->len   = 0;
        s->cap   = 0;
    }
}

@fn grow {
    static inline int
    ${PREFIX}_grow(${PREFIX}_t *s) {
        int ncap = s->cap ? s->cap * 2 : 8;
        ${T} *ni = (${T} *)realloc(s->items, sizeof(${T}) * (size_t)ncap);
        if (!ni) {
            return 0;
        }
        s->items = ni;
        s->cap   = ncap;
        return 1;
    }
}

@fn add {
    /*
     * Insert. Returns 1 when the element was added, 0 when it was
     * already present, -1 on OOM.
     */
    static inline int
    ${PREFIX}_add(${PREFIX}_t *s, ${T} x) {
        int lo = 0;
        int hi = s->len;
        while (lo < hi) {
            int mid = lo + (hi - lo) / 2;
            int c = ${CMP}(x, s->items[mid]);
            if (c < 0) {
                hi = mid;
            } else if (c > 0) {
                lo = mid + 1;
            } else {
                return 0;
            }
        }
        if (s->len == s->cap && !${PREFIX}_grow(s)) {
            return -1;
        }
        memmove(&s->items[lo + 1], &s->items[lo],
                sizeof(${T}) * (size_t)(s->len - lo));
        s->items[lo] = x;
        s->len++;
        return 1;
    }
}

@fn has {
    static inline int
    ${PREFIX}_has(const ${PREFIX}_t *s, ${T} x) {
        int lo = 0;
        int hi = s->len;
        while (lo < hi) {
            int mid = lo + (hi - lo) / 2;
            int c = ${CMP}(x, s->items[mid]);
            if (c < 0) {
                hi = mid;
            } else if (c > 0) {
                lo = mid + 1;
            } else {
                return 1;
            }
        }
        return 0;
    }
}

@fn remove {
    static inline int
    ${PREFIX}_remove(${PREFIX}_t *s, ${T} x) {
        int lo = 0;
        int hi = s->len;
        while (lo < hi) {
            int mid = lo + (hi - lo) / 2;
            int c = ${CMP}(x, s->items[mid]);
            if (c < 0) {
                hi = mid;
            } else if (c > 0) {
                lo = mid + 1;
            } else {
                memmove(&s->items[mid], &s->items[mid + 1],
                        sizeof(${T}) * (size_t)(s->len - mid - 1));
                s->len--;
                return 1;
            }
        }
        return 0;
    }
}

@fn len {
    static inline int
    ${PREFIX}_len(const ${PREFIX}_t *s) {
        return s->len;
    }
}

@fn clear {
    static inline void
    ${PREFIX}_clear(${PREFIX}_t *s) {
        s->len = 0;
    }
}

@fn free {
    static inline void
    ${PREFIX}_free(${PREFIX}_t *s) {
        free(s->items);
        s->items = 0;
        s->len   = 0;
        s->cap   = 0;
    }
}
