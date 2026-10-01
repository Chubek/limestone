# deque.q — double-ended queue on a circular buffer

@module  deque
@version 1.0

@param T
@param CAP 64
@param PREFIX deque

@require {${CAP} > 0} "CAP must be positive"

@include <assert.h>

@guard

@raw {
    #define ${PREFIX}_CAP (${CAP})
}

@struct {
    typedef struct {
        ${T} buf[${PREFIX}_CAP];
        int  head;
        int  count;
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *d) {
        d->head  = 0;
        d->count = 0;
    }
}

@fn size {
    static inline int
    ${PREFIX}_size(const ${PREFIX}_t *d) {
        return d->count;
    }
}

@fn empty {
    static inline int
    ${PREFIX}_empty(const ${PREFIX}_t *d) {
        return d->count == 0;
    }
}

@fn full {
    static inline int
    ${PREFIX}_full(const ${PREFIX}_t *d) {
        return d->count >= ${PREFIX}_CAP;
    }
}

@fn push_back {
    static inline void
    ${PREFIX}_push_back(${PREFIX}_t *d, ${T} x) {
        assert(!${PREFIX}_full(d));
        d->buf[(d->head + d->count) % ${PREFIX}_CAP] = x;
        d->count++;
    }
}

@fn push_front {
    static inline void
    ${PREFIX}_push_front(${PREFIX}_t *d, ${T} x) {
        assert(!${PREFIX}_full(d));
        d->head = (d->head + ${PREFIX}_CAP - 1) % ${PREFIX}_CAP;
        d->buf[d->head] = x;
        d->count++;
    }
}

@fn pop_back {
    static inline ${T}
    ${PREFIX}_pop_back(${PREFIX}_t *d) {
        assert(!${PREFIX}_empty(d));
        d->count--;
        return d->buf[(d->head + d->count) % ${PREFIX}_CAP];
    }
}

@fn pop_front {
    static inline ${T}
    ${PREFIX}_pop_front(${PREFIX}_t *d) {
        ${T} x;
        assert(!${PREFIX}_empty(d));
        x = d->buf[d->head];
        d->head = (d->head + 1) % ${PREFIX}_CAP;
        d->count--;
        return x;
    }
}

@fn front {
    static inline ${T} *
    ${PREFIX}_front(${PREFIX}_t *d) {
        assert(!${PREFIX}_empty(d));
        return &d->buf[d->head];
    }
}

@fn back {
    static inline ${T} *
    ${PREFIX}_back(${PREFIX}_t *d) {
        assert(!${PREFIX}_empty(d));
        return &d->buf[(d->head + d->count - 1) % ${PREFIX}_CAP];
    }
}

@fn clear {
    static inline void
    ${PREFIX}_clear(${PREFIX}_t *d) {
        d->head  = 0;
        d->count = 0;
    }
}
