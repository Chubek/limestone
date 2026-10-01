# graph.q — static directed graph with adjacency lists.
# Nodes are 0 .. NODE_CAP-1. No heap allocation.

@module  graph
@version 1.0

@param NODE_CAP 64
@param EDGE_CAP 256
@param PREFIX graph

@require {${NODE_CAP} > 0} "NODE_CAP must be positive"
@require {${EDGE_CAP} > 0} "EDGE_CAP must be positive"

@guard

@raw {
    #define ${PREFIX}_N (${NODE_CAP})
    #define ${PREFIX}_E (${EDGE_CAP})
}

@struct {
    typedef struct {
        int head[${PREFIX}_N];
        int to[${PREFIX}_E];
        int nxt[${PREFIX}_E];
        int ecount;
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *g) {
        int i;
        for (i = 0; i < ${PREFIX}_N; i++) {
            g->head[i] = -1;
        }
        g->ecount = 0;
    }
}

@fn add_edge {
    /*
     * Add edge u -> v. Returns 1 on success, 0 on bad node id,
     * -1 when the edge pool is full.
     */
    static inline int
    ${PREFIX}_add_edge(${PREFIX}_t *g, int u, int v) {
        int e;
        if (u < 0 || u >= ${PREFIX}_N || v < 0 || v >= ${PREFIX}_N) {
            return 0;
        }
        if (g->ecount >= ${PREFIX}_E) {
            return -1;
        }
        e = g->ecount++;
        g->to[e]   = v;
        g->nxt[e]  = g->head[u];
        g->head[u] = e;
        return 1;
    }
}

@fn has_edge {
    static inline int
    ${PREFIX}_has_edge(const ${PREFIX}_t *g, int u, int v) {
        int e;
        if (u < 0 || u >= ${PREFIX}_N || v < 0 || v >= ${PREFIX}_N) {
            return 0;
        }
        for (e = g->head[u]; e >= 0; e = g->nxt[e]) {
            if (g->to[e] == v) {
                return 1;
            }
        }
        return 0;
    }
}

@fn out_degree {
    static inline int
    ${PREFIX}_out_degree(const ${PREFIX}_t *g, int u) {
        int e;
        int n = 0;
        if (u < 0 || u >= ${PREFIX}_N) {
            return -1;
        }
        for (e = g->head[u]; e >= 0; e = g->nxt[e]) {
            n++;
        }
        return n;
    }
}

@fn edge_count {
    static inline int
    ${PREFIX}_edge_count(const ${PREFIX}_t *g) {
        return g->ecount;
    }
}

@fn clear {
    static inline void
    ${PREFIX}_clear(${PREFIX}_t *g) {
        ${PREFIX}_init(g);
    }
}
