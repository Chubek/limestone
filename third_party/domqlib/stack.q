# stack.q — generic stack, header-only

@module  stack
@version 1.0

@param T            # required — element type, e.g. int, float, MyStruct*
@param PREFIX stack # optional — naming prefix; defaults to @module name
@param CAP    64    # optional — default capacity constant

@include <stdlib.h>
@include <assert.h>

@guard   # emit include-guard using module name + param fingerprint

@struct {
    typedef struct {
        ${T} *data;
        int  top;
        int  cap;
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *s, int cap) {
        s->data = (${T} *)malloc(sizeof(${T}) * (size_t)cap);
        assert(s->data);
        s->cap  = cap;
        s->top  = 0;
    }
}

@fn push {
    static inline void
    ${PREFIX}_push(${PREFIX}_t *s, ${T} val) {
        assert(s->top < s->cap);
        s->data[s->top++] = val;
    }
}

@fn pop {
    static inline ${T}
    ${PREFIX}_pop(${PREFIX}_t *s) {
        assert(s->top > 0);
        return s->data[--s->top];
    }
}

@fn peek {
    static inline ${T}
    ${PREFIX}_peek(const ${PREFIX}_t *s) {
        assert(s->top > 0);
        return s->data[s->top - 1];
    }
}

@fn empty {
    static inline int
    ${PREFIX}_empty(const ${PREFIX}_t *s) {
        return s->top == 0;
    }
}

@fn free {
    static inline void
    ${PREFIX}_free(${PREFIX}_t *s) {
        free(s->data);
        s->data = 0;
        s->top  = 0;
        s->cap  = 0;
    }
}
