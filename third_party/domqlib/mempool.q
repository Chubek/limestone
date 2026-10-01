# mempool.q — fixed-size arena allocator

@module  mempool
@version 1.0

@param T
@param CAP  256
@param PREFIX mempool

@require {${CAP} > 0} "CAP must be positive"

@include <string.h>
@include <assert.h>

@guard

@raw {
    /* pool capacity compiled-in as a constant */
    #define ${PREFIX}_CAP ((int)(${CAP}))
}

@struct {
    typedef struct {
        ${T}  slots[${PREFIX}_CAP];
        int   used;
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *p) {
        memset(p->slots, 0, sizeof(p->slots));
        p->used = 0;
    }
}

@fn alloc {
    static inline ${T} *
    ${PREFIX}_alloc(${PREFIX}_t *p) {
        assert(p->used < ${PREFIX}_CAP);
        return &p->slots[p->used++];
    }
}

@fn reset {
    static inline void
    ${PREFIX}_reset(${PREFIX}_t *p) {
        p->used = 0;
    }
}

@fn full {
    static inline int
    ${PREFIX}_full(const ${PREFIX}_t *p) {
        return p->used >= ${PREFIX}_CAP;
    }
}
