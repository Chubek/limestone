# avl.q — AVL tree map (self-balancing binary search tree).
# CMP is a function name: int CMP(KEY a, KEY b). Keys/values by value.

@module  avl
@version 1.0

@param KEY
@param T
@param CMP
@param PREFIX avl

@require {[string length ${CMP}] > 0} "CMP must name a compare function"

@include <stdlib.h>
@include <assert.h>

@guard

@struct {
    typedef struct ${PREFIX}_node {
        ${KEY}               key;
        ${T}                 val;
        struct ${PREFIX}_node *l;
        struct ${PREFIX}_node *r;
        int                  h;
    } ${PREFIX}_node;

    typedef struct {
        ${PREFIX}_node *root;
        int            count;
    } ${PREFIX}_t;
}

@fn height {
    static inline int
    ${PREFIX}_height(const ${PREFIX}_node *n) {
        return n ? n->h : 0;
    }
}

@fn fix {
    static inline void
    ${PREFIX}_fix(${PREFIX}_node *n) {
        int hl = ${PREFIX}_height(n->l);
        int hr = ${PREFIX}_height(n->r);
        n->h = (hl > hr ? hl : hr) + 1;
    }
}

@fn rotl {
    static inline ${PREFIX}_node *
    ${PREFIX}_rotl(${PREFIX}_node *n) {
        ${PREFIX}_node *r = n->r;
        n->r = r->l;
        r->l = n;
        ${PREFIX}_fix(n);
        ${PREFIX}_fix(r);
        return r;
    }
}

@fn rotr {
    static inline ${PREFIX}_node *
    ${PREFIX}_rotr(${PREFIX}_node *n) {
        ${PREFIX}_node *l = n->l;
        n->l = l->r;
        l->r = n;
        ${PREFIX}_fix(n);
        ${PREFIX}_fix(l);
        return l;
    }
}

@fn balance {
    static inline ${PREFIX}_node *
    ${PREFIX}_balance(${PREFIX}_node *n) {
        ${PREFIX}_fix(n);
        if (${PREFIX}_height(n->r) - ${PREFIX}_height(n->l) == 2) {
            if (${PREFIX}_height(n->r->l) > ${PREFIX}_height(n->r->r)) {
                n->r = ${PREFIX}_rotr(n->r);
            }
            return ${PREFIX}_rotl(n);
        }
        if (${PREFIX}_height(n->l) - ${PREFIX}_height(n->r) == 2) {
            if (${PREFIX}_height(n->l->r) > ${PREFIX}_height(n->l->l)) {
                n->l = ${PREFIX}_rotl(n->l);
            }
            return ${PREFIX}_rotr(n);
        }
        return n;
    }
}

@fn ins {
    static ${PREFIX}_node *
    ${PREFIX}_ins(${PREFIX}_node *n, ${KEY} k, ${T} v, int *added) {
        if (!n) {
            ${PREFIX}_node *m = (${PREFIX}_node *)malloc(sizeof *m);
            assert(m);
            m->key = k;
            m->val = v;
            m->l   = 0;
            m->r   = 0;
            m->h   = 1;
            *added = 1;
            return m;
        }
        {
            int c = ${CMP}(k, n->key);
            if (c < 0) {
                n->l = ${PREFIX}_ins(n->l, k, v, added);
            } else if (c > 0) {
                n->r = ${PREFIX}_ins(n->r, k, v, added);
            } else {
                n->val = v;
                *added = 0;
                return n;
            }
        }
        return ${PREFIX}_balance(n);
    }
}

@fn destroy {
    static void
    ${PREFIX}_destroy(${PREFIX}_node *n) {
        if (n) {
            ${PREFIX}_destroy(n->l);
            ${PREFIX}_destroy(n->r);
            free(n);
        }
    }
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *t) {
        t->root  = 0;
        t->count = 0;
    }
}

@fn put {
    /* Insert or update. Returns 1 when new, 0 when updated. */
    static inline int
    ${PREFIX}_put(${PREFIX}_t *t, ${KEY} k, ${T} v) {
        int added = 0;
        t->root = ${PREFIX}_ins(t->root, k, v, &added);
        t->count += added;
        return added;
    }
}

@fn get {
    static inline ${T} *
    ${PREFIX}_get(${PREFIX}_t *t, ${KEY} k) {
        ${PREFIX}_node *n = t->root;
        while (n) {
            int c = ${CMP}(k, n->key);
            if (c < 0) {
                n = n->l;
            } else if (c > 0) {
                n = n->r;
            } else {
                return &n->val;
            }
        }
        return 0;
    }
}

@fn has {
    static inline int
    ${PREFIX}_has(${PREFIX}_t *t, ${KEY} k) {
        return ${PREFIX}_get(t, k) != 0;
    }
}

@fn count {
    static inline int
    ${PREFIX}_count(const ${PREFIX}_t *t) {
        return t->count;
    }
}

@fn free {
    static inline void
    ${PREFIX}_free(${PREFIX}_t *t) {
        ${PREFIX}_destroy(t->root);
        t->root  = 0;
        t->count = 0;
    }
}
