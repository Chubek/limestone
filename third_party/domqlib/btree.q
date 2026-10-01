# btree.q — B-tree map (CLRS-style, minimum degree DEG).
# CMP is a function name: int CMP(KEY a, KEY b). Keys/values by value.

@module  btree
@version 1.0

@param KEY
@param T
@param CMP
@param DEG 3
@param PREFIX btree

@require {[string length ${CMP}] > 0} "CMP must name a compare function"
@require {${DEG} >= 2} "DEG must be at least 2"

@include <stdlib.h>
@include <string.h>
@include <assert.h>

@guard

@raw {
    #define ${PREFIX}_T    (${DEG})
    #define ${PREFIX}_MINK (${PREFIX}_T - 1)
    #define ${PREFIX}_MAXK ((2 * ${PREFIX}_T) - 1)
    #define ${PREFIX}_MAXC (2 * ${PREFIX}_T)
}

@struct {
    typedef struct ${PREFIX}_node {
        int                  n;
        int                  leaf;
        ${KEY}               k[${PREFIX}_MAXK];
        ${T}                 v[${PREFIX}_MAXK];
        struct ${PREFIX}_node *c[${PREFIX}_MAXC];
    } ${PREFIX}_node;

    typedef struct {
        ${PREFIX}_node *root;
        int            count;
    } ${PREFIX}_t;
}

@fn new_node {
    static ${PREFIX}_node *
    ${PREFIX}_new_node(int leaf) {
        ${PREFIX}_node *n = (${PREFIX}_node *)malloc(sizeof *n);
        assert(n);
        n->n    = 0;
        n->leaf = leaf;
        return n;
    }
}

@fn destroy {
    static void
    ${PREFIX}_destroy(${PREFIX}_node *n) {
        if (n) {
            int i;
            if (!n->leaf) {
                for (i = 0; i <= n->n; i++) {
                    ${PREFIX}_destroy(n->c[i]);
                }
            }
            free(n);
        }
    }
}

@fn search {
    static ${T} *
    ${PREFIX}_search(${PREFIX}_node *n, ${KEY} k) {
        while (n) {
            int i = 0;
            while (i < n->n && ${CMP}(k, n->k[i]) > 0) {
                i++;
            }
            if (i < n->n && ${CMP}(k, n->k[i]) == 0) {
                return &n->v[i];
            }
            if (n->leaf) {
                return 0;
            }
            n = n->c[i];
        }
        return 0;
    }
}

@fn assign {
    /* Overwrite the value for an existing key. Returns 1 if found. */
    static int
    ${PREFIX}_assign(${PREFIX}_node *n, ${KEY} k, ${T} v) {
        while (n) {
            int i = 0;
            while (i < n->n && ${CMP}(k, n->k[i]) > 0) {
                i++;
            }
            if (i < n->n && ${CMP}(k, n->k[i]) == 0) {
                n->v[i] = v;
                return 1;
            }
            if (n->leaf) {
                return 0;
            }
            n = n->c[i];
        }
        return 0;
    }
}

@fn split {
    static void
    ${PREFIX}_split(${PREFIX}_node *x, int i) {
        ${PREFIX}_node *y = x->c[i];
        ${PREFIX}_node *z = ${PREFIX}_new_node(y->leaf);
        int j;
        assert(y->n == ${PREFIX}_MAXK);
        z->n = ${PREFIX}_MINK;
        for (j = 0; j < ${PREFIX}_MINK; j++) {
            z->k[j] = y->k[j + ${PREFIX}_T];
            z->v[j] = y->v[j + ${PREFIX}_T];
        }
        if (!y->leaf) {
            for (j = 0; j < ${PREFIX}_T; j++) {
                z->c[j] = y->c[j + ${PREFIX}_T];
            }
        }
        y->n = ${PREFIX}_MINK;
        memmove(&x->c[i + 2], &x->c[i + 1],
                sizeof(x->c[0]) * (size_t)(x->n - i));
        x->c[i + 1] = z;
        memmove(&x->k[i + 1], &x->k[i], sizeof(x->k[0]) * (size_t)(x->n - i));
        memmove(&x->v[i + 1], &x->v[i], sizeof(x->v[0]) * (size_t)(x->n - i));
        x->k[i] = y->k[${PREFIX}_MINK];
        x->v[i] = y->v[${PREFIX}_MINK];
        x->n++;
    }
}

@fn insert_nonfull {
    static void
    ${PREFIX}_insert_nonfull(${PREFIX}_node *x, ${KEY} k, ${T} v) {
        int i = x->n - 1;
        if (x->leaf) {
            while (i >= 0 && ${CMP}(k, x->k[i]) < 0) {
                x->k[i + 1] = x->k[i];
                x->v[i + 1] = x->v[i];
                i--;
            }
            x->k[i + 1] = k;
            x->v[i + 1] = v;
            x->n++;
        } else {
            while (i >= 0 && ${CMP}(k, x->k[i]) < 0) {
                i--;
            }
            i++;
            if (x->c[i]->n == ${PREFIX}_MAXK) {
                ${PREFIX}_split(x, i);
                if (${CMP}(k, x->k[i]) > 0) {
                    i++;
                }
            }
            ${PREFIX}_insert_nonfull(x->c[i], k, v);
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
        if (t->root && ${PREFIX}_assign(t->root, k, v)) {
            return 0;
        }
        if (!t->root) {
            t->root = ${PREFIX}_new_node(1);
        }
        if (t->root->n == ${PREFIX}_MAXK) {
            ${PREFIX}_node *s = ${PREFIX}_new_node(0);
            s->c[0] = t->root;
            t->root = s;
            ${PREFIX}_split(s, 0);
            ${PREFIX}_insert_nonfull(s, k, v);
        } else {
            ${PREFIX}_insert_nonfull(t->root, k, v);
        }
        t->count++;
        return 1;
    }
}

@fn get {
    static inline ${T} *
    ${PREFIX}_get(${PREFIX}_t *t, ${KEY} k) {
        if (!t->root) {
            return 0;
        }
        return ${PREFIX}_search(t->root, k);
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
