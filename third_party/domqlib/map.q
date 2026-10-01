# map.q — sorted dynamic-array map with binary search.
# CMP is a function name: int CMP(KEY a, KEY b), negative when a < b,
# zero when equal, positive when a > b. Keys/values stored by value.

@module  map
@version 1.0

@param KEY
@param T
@param CMP
@param PREFIX map

@require {[string length ${CMP}] > 0} "CMP must name a compare function"

@include <stdlib.h>
@include <string.h>
@include <assert.h>

@guard

@struct {
    typedef struct {
        ${KEY} *keys;
        ${T}   *vals;
        int    len;
        int    cap;
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *m) {
        m->keys = 0;
        m->vals = 0;
        m->len  = 0;
        m->cap  = 0;
    }
}

@fn grow {
    static inline int
    ${PREFIX}_grow(${PREFIX}_t *m) {
        int ncap = m->cap ? m->cap * 2 : 8;
        ${KEY} *nk = (${KEY} *)realloc(m->keys, sizeof(${KEY}) * (size_t)ncap);
        ${T}   *nv;
        if (!nk) {
            return 0;
        }
        nv = (${T} *)realloc(m->vals, sizeof(${T}) * (size_t)ncap);
        if (!nv) {
            free(nk);
            return 0;
        }
        m->keys = nk;
        m->vals = nv;
        m->cap  = ncap;
        return 1;
    }
}

@fn put {
    /*
     * Insert or update. Returns 1 when a new entry was added,
     * 0 when an existing entry was updated, -1 on OOM.
     */
    static inline int
    ${PREFIX}_put(${PREFIX}_t *m, ${KEY} k, ${T} v) {
        int lo = 0;
        int hi = m->len;
        while (lo < hi) {
            int mid = lo + (hi - lo) / 2;
            int c = ${CMP}(k, m->keys[mid]);
            if (c < 0) {
                hi = mid;
            } else if (c > 0) {
                lo = mid + 1;
            } else {
                m->vals[mid] = v;
                return 0;
            }
        }
        if (m->len == m->cap && !${PREFIX}_grow(m)) {
            return -1;
        }
        memmove(&m->keys[lo + 1], &m->keys[lo], sizeof(${KEY}) * (size_t)(m->len - lo));
        memmove(&m->vals[lo + 1], &m->vals[lo], sizeof(${T}) * (size_t)(m->len - lo));
        m->keys[lo] = k;
        m->vals[lo] = v;
        m->len++;
        return 1;
    }
}

@fn get {
    static inline ${T} *
    ${PREFIX}_get(${PREFIX}_t *m, ${KEY} k) {
        int lo = 0;
        int hi = m->len;
        while (lo < hi) {
            int mid = lo + (hi - lo) / 2;
            int c = ${CMP}(k, m->keys[mid]);
            if (c < 0) {
                hi = mid;
            } else if (c > 0) {
                lo = mid + 1;
            } else {
                return &m->vals[mid];
            }
        }
        return 0;
    }
}

@fn has {
    static inline int
    ${PREFIX}_has(${PREFIX}_t *m, ${KEY} k) {
        return ${PREFIX}_get(m, k) != 0;
    }
}

@fn remove {
    static inline int
    ${PREFIX}_remove(${PREFIX}_t *m, ${KEY} k) {
        int lo = 0;
        int hi = m->len;
        while (lo < hi) {
            int mid = lo + (hi - lo) / 2;
            int c = ${CMP}(k, m->keys[mid]);
            if (c < 0) {
                hi = mid;
            } else if (c > 0) {
                lo = mid + 1;
            } else {
                memmove(&m->keys[mid], &m->keys[mid + 1],
                        sizeof(${KEY}) * (size_t)(m->len - mid - 1));
                memmove(&m->vals[mid], &m->vals[mid + 1],
                        sizeof(${T}) * (size_t)(m->len - mid - 1));
                m->len--;
                return 1;
            }
        }
        return 0;
    }
}

@fn len {
    static inline int
    ${PREFIX}_len(const ${PREFIX}_t *m) {
        return m->len;
    }
}

@fn clear {
    static inline void
    ${PREFIX}_clear(${PREFIX}_t *m) {
        m->len = 0;
    }
}

@fn free {
    static inline void
    ${PREFIX}_free(${PREFIX}_t *m) {
        free(m->keys);
        free(m->vals);
        m->keys = 0;
        m->vals = 0;
        m->len  = 0;
        m->cap  = 0;
    }
}
