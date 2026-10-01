# hash.q — open-addressing hash map with linear probing.
# Keys and values are stored by value (no ownership taken).
# HASH is a function name: unsigned long HASH(KEY k).
# EQ is a function name: int EQ(KEY a, KEY b), nonzero when equal.

@module  hash
@version 1.0

@param KEY
@param T
@param CAP 64
@param HASH
@param EQ
@param PREFIX hash

@require {${CAP} > 0} "CAP must be positive"
@require {[string length ${HASH}] > 0} "HASH must name a hash function"
@require {[string length ${EQ}] > 0} "EQ must name an equality function"

@include <string.h>
@include <assert.h>

@guard

@raw {
    #define ${PREFIX}_CAP (${CAP})
}

@struct {
    typedef struct {
        ${KEY}       keys[${PREFIX}_CAP];
        ${T}         vals[${PREFIX}_CAP];
        unsigned char used[${PREFIX}_CAP];
        int          count;
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *h) {
        memset(h->used, 0, sizeof(h->used));
        h->count = 0;
    }
}

@fn put {
    /*
     * Insert or update. Returns 1 when a new entry was added,
     * 0 when an existing entry was updated, -1 when the table is full.
     */
    static inline int
    ${PREFIX}_put(${PREFIX}_t *h, ${KEY} k, ${T} v) {
        unsigned long slot = ${HASH}(k) % (unsigned long)${PREFIX}_CAP;
        unsigned long i;
        for (i = 0; i < (unsigned long)${PREFIX}_CAP; i++) {
            unsigned long at = (slot + i) % (unsigned long)${PREFIX}_CAP;
            if (!h->used[at]) {
                h->keys[at] = k;
                h->vals[at] = v;
                h->used[at] = 1;
                h->count++;
                return 1;
            }
            if (${EQ}(h->keys[at], k)) {
                h->vals[at] = v;
                return 0;
            }
        }
        return -1;
    }
}

@fn get {
    static inline ${T} *
    ${PREFIX}_get(${PREFIX}_t *h, ${KEY} k) {
        unsigned long slot = ${HASH}(k) % (unsigned long)${PREFIX}_CAP;
        unsigned long i;
        for (i = 0; i < (unsigned long)${PREFIX}_CAP; i++) {
            unsigned long at = (slot + i) % (unsigned long)${PREFIX}_CAP;
            if (!h->used[at]) {
                return 0;
            }
            if (${EQ}(h->keys[at], k)) {
                return &h->vals[at];
            }
        }
        return 0;
    }
}

@fn has {
    static inline int
    ${PREFIX}_has(${PREFIX}_t *h, ${KEY} k) {
        return ${PREFIX}_get(h, k) != 0;
    }
}

@fn remove {
    /* Remove an entry, rehashing the probe cluster. Returns 1 if removed. */
    static inline int
    ${PREFIX}_remove(${PREFIX}_t *h, ${KEY} k) {
        unsigned long slot = ${HASH}(k) % (unsigned long)${PREFIX}_CAP;
        unsigned long i;
        unsigned long j;
        for (i = 0; i < (unsigned long)${PREFIX}_CAP; i++) {
            unsigned long at = (slot + i) % (unsigned long)${PREFIX}_CAP;
            if (!h->used[at]) {
                return 0;
            }
            if (${EQ}(h->keys[at], k)) {
                h->used[at] = 0;
                h->count--;
                j = (at + 1) % (unsigned long)${PREFIX}_CAP;
                while (h->used[j]) {
                    ${KEY} rk = h->keys[j];
                    ${T}   rv = h->vals[j];
                    h->used[j] = 0;
                    h->count--;
                    assert(${PREFIX}_put(h, rk, rv) == 1);
                    j = (j + 1) % (unsigned long)${PREFIX}_CAP;
                }
                return 1;
            }
        }
        return 0;
    }
}

@fn count {
    static inline int
    ${PREFIX}_count(const ${PREFIX}_t *h) {
        return h->count;
    }
}

@fn clear {
    static inline void
    ${PREFIX}_clear(${PREFIX}_t *h) {
        memset(h->used, 0, sizeof(h->used));
        h->count = 0;
    }
}
