# pylist.q — Python list construction and inspection.

@module  pylist
@version 1.0

@param PREFIX pylist

@include <Python.h>

@guard

@fn new {
    /* New empty list of length `size` (NULL entries); NULL on failure. */
    static inline PyObject *
    ${PREFIX}_new(Py_ssize_t size) {
        return PyList_New(size);
    }
}

@fn size {
    static inline Py_ssize_t
    ${PREFIX}_size(PyObject *o) {
        return PyList_Size(o);
    }
}

@fn check {
    static inline int
    ${PREFIX}_check(PyObject *o) {
        return PyList_Check(o);
    }
}

@fn get {
    /* Borrowed reference to item at index i. */
    static inline PyObject *
    ${PREFIX}_get(PyObject *o, Py_ssize_t i) {
        return PyList_GetItem(o, i);
    }
}

@fn get_ref {
    /* New reference to item at index i (3.13+); NULL on error. */
    static inline PyObject *
    ${PREFIX}_get_ref(PyObject *o, Py_ssize_t i) {
        return PyList_GetItemRef(o, i);
    }
}

@fn set {
    /* Steal a reference to v into slot i. Returns 0 on success. */
    static inline int
    ${PREFIX}_set(PyObject *o, Py_ssize_t i, PyObject *v) {
        return PyList_SetItem(o, i, v);
    }
}

@fn append {
    /* Increment v's refcount and append. Returns 0 on success. */
    static inline int
    ${PREFIX}_append(PyObject *o, PyObject *v) {
        return PyList_Append(o, v);
    }
}

@fn insert {
    static inline int
    ${PREFIX}_insert(PyObject *o, Py_ssize_t i, PyObject *v) {
        return PyList_Insert(o, i, v);
    }
}

@fn sort {
    static inline int
    ${PREFIX}_sort(PyObject *o) {
        return PyList_Sort(o);
    }
}
