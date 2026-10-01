# pyvec.q — a reference-managed vector of PyObject*.
#
# Demonstrates domqlib reuse: the storage comes from domqlib's `vector`
# module (imported under the `pyvec_raw` prefix), and this module layers
# Python reference-counting on top — `append` increments, `clear`/`free`
# decrement, and `steal` transfers ownership without touching the count.

@module  pyvec
@version 1.0

@param PREFIX pyvec

@include <Python.h>
@include <limits.h>

@import vector T=PyObject* PREFIX=pyvec_raw

@guard

@struct {
    typedef struct {
        pyvec_raw_t items;
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *v) {
        pyvec_raw_init(&v->items);
    }
}

@fn len {
    static inline int
    ${PREFIX}_len(const ${PREFIX}_t *v) {
        return pyvec_raw_len(&v->items);
    }
}

@fn get {
    /* Borrowed pointer to the stored PyObject* (no refcount change). */
    static inline PyObject *
    ${PREFIX}_get(${PREFIX}_t *v, int i) {
        if (i < 0 || i >= v->items.len) {
            PyErr_SetString(PyExc_IndexError, "vector index out of range");
            return NULL;
        }
        return v->items.data[i];
    }
}

@fn try_append {
    /* Checked append: leaves the vector and caller reference intact on error. */
    static inline int
    ${PREFIX}_try_append(${PREFIX}_t *v, PyObject *o) {
        if (!o) {
            PyErr_SetString(PyExc_ValueError, "cannot append NULL");
            return -1;
        }
        if (v->items.len == INT_MAX ||
            !pyvec_raw_reserve(&v->items, v->items.len + 1)) {
            PyErr_NoMemory();
            return -1;
        }
        Py_INCREF(o);
        v->items.data[v->items.len++] = o;
        return 0;
    }
}

@fn append {
    /* Legacy void interface: inspect PyErr_Occurred after failure. */
    static inline void
    ${PREFIX}_append(${PREFIX}_t *v, PyObject *o) {
        (void)${PREFIX}_try_append(v, o);
    }
}

@fn try_steal {
    /* Transfer ownership only on success. */
    static inline int
    ${PREFIX}_try_steal(${PREFIX}_t *v, PyObject *o) {
        if (${PREFIX}_try_append(v, o) < 0) return -1;
        Py_DECREF(o);
        return 0;
    }
}

@fn steal {
    static inline void
    ${PREFIX}_steal(${PREFIX}_t *v, PyObject *o) {
        (void)${PREFIX}_try_steal(v, o);
    }
}

@fn pop {
    /* Remove and return the last object (caller gains ownership). */
    static inline PyObject *
    ${PREFIX}_pop(${PREFIX}_t *v) {
        if (!v->items.len) {
            PyErr_SetString(PyExc_IndexError, "pop from empty vector");
            return NULL;
        }
        return v->items.data[--v->items.len];
    }
}

@fn clear {
    /* DECREF every stored object and empty the vector. */
    static inline void
    ${PREFIX}_clear(${PREFIX}_t *v) {
        while (v->items.len) {
            PyObject *o = v->items.data[--v->items.len];
            Py_XDECREF(o);
        }
    }
}

@fn free {
    /* DECREF every stored object and release the backing buffer. */
    static inline void
    ${PREFIX}_free(${PREFIX}_t *v) {
        ${PREFIX}_clear(v);
        pyvec_raw_free(&v->items);
    }
}

@fn tuple {
    /* Build a new tuple from the current contents. New ref or NULL. */
    static inline PyObject *
    ${PREFIX}_tuple(${PREFIX}_t *v) {
        PyObject *t = PyTuple_New(pyvec_raw_len(&v->items));
        int i;
        if (!t) {
            return 0;
        }
        for (i = 0; i < pyvec_raw_len(&v->items); i++) {
            PyObject *o = *pyvec_raw_get(&v->items, i);
            Py_INCREF(o);
            PyTuple_SET_ITEM(t, i, o);
        }
        return t;
    }
}
