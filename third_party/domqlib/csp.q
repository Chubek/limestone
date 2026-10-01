# csp.q — bounded channel (single-threaded ring buffer with
# CSP-style send/recv names). No heap allocation.

@module  csp
@version 1.0

@param T
@param CAP 16
@param PREFIX csp

@require {${CAP} > 0} "CAP must be positive"

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
    ${PREFIX}_init(${PREFIX}_t *c) {
        c->head  = 0;
        c->count = 0;
    }
}

@fn send {
    /* Enqueue. Returns 1 on success, 0 when the channel is full. */
    static inline int
    ${PREFIX}_send(${PREFIX}_t *c, ${T} x) {
        if (c->count >= ${PREFIX}_CAP) {
            return 0;
        }
        c->buf[(c->head + c->count) % ${PREFIX}_CAP] = x;
        c->count++;
        return 1;
    }
}

@fn recv {
    /* Dequeue into *out. Returns 1 on success, 0 when empty. */
    static inline int
    ${PREFIX}_recv(${PREFIX}_t *c, ${T} *out) {
        if (c->count <= 0) {
            return 0;
        }
        *out = c->buf[c->head];
        c->head = (c->head + 1) % ${PREFIX}_CAP;
        c->count--;
        return 1;
    }
}

@fn avail {
    static inline int
    ${PREFIX}_avail(const ${PREFIX}_t *c) {
        return c->count;
    }
}

@fn space {
    static inline int
    ${PREFIX}_space(const ${PREFIX}_t *c) {
        return ${PREFIX}_CAP - c->count;
    }
}

@fn clear {
    static inline void
    ${PREFIX}_clear(${PREFIX}_t *c) {
        c->head  = 0;
        c->count = 0;
    }
}
