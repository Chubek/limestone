# arena.q — bump-pointer arena allocator on a static buffer.
# Allocations are only reclaimed together by reset. No header includes
# beyond stddef are needed.

@module  arena
@version 1.0

@param CAP 4096
@param PREFIX arena

@require {${CAP} > 0} "CAP must be positive"

@include <stddef.h>

@guard

@raw {
    #define ${PREFIX}_CAP (${CAP})
}

@struct {
    typedef struct {
        char          buf[${PREFIX}_CAP];
        unsigned long off;
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *a) {
        a->off = 0;
    }
}

@fn alloc {
    /*
     * Allocate n bytes aligned up to `align` (power of two; 0 means 1).
     * Returns NULL when the arena is exhausted.
     */
    static inline void *
    ${PREFIX}_alloc(${PREFIX}_t *a, unsigned long n, unsigned long align) {
        unsigned long al = align ? align : 1;
        unsigned long o = (a->off + al - 1) & ~(al - 1);
        void *p;
        if (o + n > (unsigned long)${PREFIX}_CAP) {
            return 0;
        }
        p = (void *)&a->buf[o];
        a->off = o + n;
        return p;
    }
}

@fn used {
    static inline unsigned long
    ${PREFIX}_used(const ${PREFIX}_t *a) {
        return a->off;
    }
}

@fn capacity {
    static inline unsigned long
    ${PREFIX}_capacity(const ${PREFIX}_t *a) {
        (void)a;
        return (unsigned long)${PREFIX}_CAP;
    }
}

@fn reset {
    static inline void
    ${PREFIX}_reset(${PREFIX}_t *a) {
        a->off = 0;
    }
}
