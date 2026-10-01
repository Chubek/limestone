# pytuple.q — Python tuple construction and inspection.

@module  pytuple
@version 1.0

@param PREFIX pytuple

@include <Python.h>

@guard

@fn new {
    /* New tuple of length `size` (NULL entries); NULL on failure. */
    static inline PyObject *
    ${PREFIX}_new(Py_ssize_t size) {
        return PyTuple_New(size);
    }
}

@fn size {
    static inline Py_ssize_t
    ${PREFIX}_size(PyObject *o) {
        return PyTuple_Size(o);
    }
}

@fn check {
    static inline int
    ${PREFIX}_check(PyObject *o) {
        return PyTuple_Check(o);
    }
}

@fn get {
    /* Borrowed reference to item at index i. */
    static inline PyObject *
    ${PREFIX}_get(PyObject *o, Py_ssize_t i) {
        return PyTuple_GetItem(o, i);
    }
}

@fn set {
    /* Steal a reference to v into slot i. Returns 0 on success. */
    static inline int
    ${PREFIX}_set(PyObject *o, Py_ssize_t i, PyObject *v) {
        return PyTuple_SetItem(o, i, v);
    }
}

@fn get_slice {
    /* New tuple o[low:high]; NULL on failure. */
    static inline PyObject *
    ${PREFIX}_get_slice(PyObject *o, Py_ssize_t low, Py_ssize_t high) {
        return PyTuple_GetSlice(o, low, high);
    }
}

@raw {
    /* Varargs pack: pytuple_pack(n, o1, o2, ...) -> new tuple. */
    #define ${PREFIX}_pack PyTuple_Pack
}
