# pydict.q — Python dict construction and inspection.

@module  pydict
@version 1.0

@param PREFIX pydict

@include <Python.h>

@guard

@fn new {
    static inline PyObject *
    ${PREFIX}_new(void) {
        return PyDict_New();
    }
}

@fn size {
    static inline Py_ssize_t
    ${PREFIX}_size(PyObject *o) {
        return PyDict_Size(o);
    }
}

@fn check {
    static inline int
    ${PREFIX}_check(PyObject *o) {
        return PyDict_Check(o);
    }
}

@fn get {
    /* Borrowed reference to d[key], or NULL if absent. This legacy
     * lookup suppresses lookup exceptions; prefer get_ref for checked use. */
    static inline PyObject *
    ${PREFIX}_get(PyObject *o, PyObject *key) {
        return PyDict_GetItem(o, key);
    }
}

@fn get_ref {
    /* New reference to d[key] written to *result (3.13+). Returns 1 when
     * present (result = new ref), 0 when missing (result = NULL), -1 on
     * error. */
    static inline int
    ${PREFIX}_get_ref(PyObject *o, PyObject *key, PyObject **result) {
        return PyDict_GetItemRef(o, key, result);
    }
}

@fn set {
    /* Increment v's refcount and insert d[key] = v. 0 on success. */
    static inline int
    ${PREFIX}_set(PyObject *o, PyObject *key, PyObject *v) {
        return PyDict_SetItem(o, key, v);
    }
}

@fn set_string {
    static inline int
    ${PREFIX}_set_string(PyObject *o, const char *key, PyObject *v) {
        return PyDict_SetItemString(o, key, v);
    }
}

@fn del {
    static inline int
    ${PREFIX}_del(PyObject *o, PyObject *key) {
        return PyDict_DelItem(o, key);
    }
}

@fn del_string {
    static inline int
    ${PREFIX}_del_string(PyObject *o, const char *key) {
        return PyDict_DelItemString(o, key);
    }
}

@fn contains {
    static inline int
    ${PREFIX}_contains(PyObject *o, PyObject *key) {
        return PyDict_Contains(o, key);
    }
}

@fn get_string {
    /* Borrowed reference to d[key] or NULL if missing. */
    static inline PyObject *
    ${PREFIX}_get_string(PyObject *o, const char *key) {
        return PyDict_GetItemString(o, key);
    }
}

@fn get_string_ref {
    /* New reference to d[key] written to *result (3.13+). Returns 1 when
     * present (result = new ref), 0 when missing (result = NULL), -1 on
     * error. */
    static inline int
    ${PREFIX}_get_string_ref(PyObject *o, const char *key, PyObject **result) {
        return PyDict_GetItemStringRef(o, key, result);
    }
}

@fn clear {
    static inline void
    ${PREFIX}_clear(PyObject *o) {
        PyDict_Clear(o);
    }
}
