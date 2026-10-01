# pyconv.q — scalar conversion between C primitives and Python objects.
#
# Wrappers over PyLong/PyFloat/PyBool and the number protocol. Each
# `from_*` returns a new reference (NULL on failure); each `as_*`
# returns a C scalar and relies on the caller to clear any exception.

@module  pyconv
@version 1.0

@param PREFIX pyconv

@include <Python.h>

@guard

@fn long_from_long {
    static inline PyObject *
    ${PREFIX}_long_from_long(long v) {
        return PyLong_FromLong(v);
    }
}

@fn long_from_longlong {
    static inline PyObject *
    ${PREFIX}_long_from_longlong(long long v) {
        return PyLong_FromLongLong(v);
    }
}

@fn long_from_ulonglong {
    static inline PyObject *
    ${PREFIX}_long_from_ulonglong(unsigned long long v) {
        return PyLong_FromUnsignedLongLong(v);
    }
}

@fn long_from_ssize {
    static inline PyObject *
    ${PREFIX}_long_from_ssize(Py_ssize_t v) {
        return PyLong_FromSsize_t(v);
    }
}

@fn long_as_long {
    static inline long
    ${PREFIX}_long_as_long(PyObject *o) {
        return PyLong_AsLong(o);
    }
}

@fn long_as_longlong {
    static inline long long
    ${PREFIX}_long_as_longlong(PyObject *o) {
        return PyLong_AsLongLong(o);
    }
}

@fn long_as_ssize {
    static inline Py_ssize_t
    ${PREFIX}_long_as_ssize(PyObject *o) {
        return PyLong_AsSsize_t(o);
    }
}

@fn long_as_ulonglong {
    /* Returns (unsigned long long)-1 and raises OverflowError on
     * overflow or a negative input. */
    static inline unsigned long long
    ${PREFIX}_long_as_ulonglong(PyObject *o) {
        return PyLong_AsUnsignedLongLong(o);
    }
}

@fn float_from_double {
    static inline PyObject *
    ${PREFIX}_float_from_double(double v) {
        return PyFloat_FromDouble(v);
    }
}

@fn float_as_double {
    static inline double
    ${PREFIX}_float_as_double(PyObject *o) {
        return PyFloat_AsDouble(o);
    }
}

@fn bool_from_long {
    static inline PyObject *
    ${PREFIX}_bool_from_long(long v) {
        return PyBool_FromLong(v);
    }
}

@fn bool_check {
    static inline int
    ${PREFIX}_bool_check(PyObject *o) {
        return PyBool_Check(o);
    }
}

@fn long_check {
    static inline int
    ${PREFIX}_long_check(PyObject *o) {
        return PyLong_Check(o);
    }
}

@fn float_check {
    static inline int
    ${PREFIX}_float_check(PyObject *o) {
        return PyFloat_Check(o);
    }
}

@fn number_index {
    /* Return a Python int (new reference) from any object supporting
     * __index__; NULL on failure. */
    static inline PyObject *
    ${PREFIX}_number_index(PyObject *o) {
        return PyNumber_Index(o);
    }
}
