# pyargs.q — argument parsing helpers for extension functions.
#
# Wrappers over PyArg_ParseTuple and PyArg_ParseTupleAndKeywords that
# forward the trailing varargs through the API's va_list entry points,
# plus a couple of non-variadic size/sequence helpers.

@module  pyargs
@version 1.0

@param PREFIX pyargs

@include <Python.h>
@include <stdarg.h>

@guard

@fn parse_tuple {
    /* Parse `args` (a tuple) with a format string. Returns 1 on success,
     * 0 on failure (TypeError raised). */
    static inline int
    ${PREFIX}_parse_tuple(PyObject *args, const char *fmt, ...) {
        va_list ap;
        int r;
        va_start(ap, fmt);
        r = PyArg_VaParse(args, fmt, ap);
        va_end(ap);
        return r;
    }
}

@fn parse_tuple_kw {
    /* Parse positional and keyword arguments. `keywords` is a
     * NULL-terminated array of accepted keyword names. */
    static inline int
    ${PREFIX}_parse_tuple_kw(PyObject *args, PyObject *kw, const char *fmt,
                             char *keywords[], ...) {
        va_list ap;
        int r;
        va_start(ap, keywords);
        r = PyArg_VaParseTupleAndKeywords(args, kw, fmt, keywords, ap);
        va_end(ap);
        return r;
    }
}

@fn size {
    /* Number of items in a tuple/list/sequence argument. -1 on error. */
    static inline Py_ssize_t
    ${PREFIX}_size(PyObject *args) {
        return PyTuple_Size(args);
    }
}

@fn tuple_check {
    static inline int
    ${PREFIX}_tuple_check(PyObject *o) {
        return PyTuple_Check(o);
    }
}

@raw {
    /* Varargs alias: pyargs_unpack_tuple(args, name, min, max, ...). */
    #define ${PREFIX}_unpack_tuple PyArg_UnpackTuple
}
