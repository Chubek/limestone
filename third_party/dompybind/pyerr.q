# pyerr.q — exception state management.
#
# Named wrappers over the PyErr_* family. The fetch/restore pair, the
# matches test and format-style construction are the workhorses here.

@module  pyerr
@version 1.0

@param PREFIX pyerr

@include <Python.h>
@include <stdarg.h>

@guard

@fn occurred {
    /* Borrowed reference to the current exception, or NULL if none. */
    static inline PyObject *
    ${PREFIX}_occurred(void) {
        return PyErr_Occurred();
    }
}

@fn clear {
    static inline void
    ${PREFIX}_clear(void) {
        PyErr_Clear();
    }
}

@fn set_none {
    static inline void
    ${PREFIX}_set_none(PyObject *exc) {
        PyErr_SetNone(exc);
    }
}

@fn set_string {
    static inline void
    ${PREFIX}_set_string(PyObject *exc, const char *msg) {
        PyErr_SetString(exc, msg);
    }
}

@fn set_object {
    static inline void
    ${PREFIX}_set_object(PyObject *exc, PyObject *value) {
        PyErr_SetObject(exc, value);
    }
}

@fn format {
    /* PyErr_Format(exc, fmt, ...): raises the exception in the current
     * thread state. Returns NULL (the value is retrievable via
     * pyerr_fetch / pyerr_occurred). */
    static inline PyObject *
    ${PREFIX}_format(PyObject *exc, const char *fmt, ...) {
        va_list ap;
        PyObject *r;
        va_start(ap, fmt);
        r = PyErr_FormatV(exc, fmt, ap);
        va_end(ap);
        return r;
    }
}

@fn fetch {
    /* Fetch the current exception triple into (ptype, pvalue, ptb).
     * pvalue/ptb may be NULL. */
    static inline void
    ${PREFIX}_fetch(PyObject **ptype, PyObject **pvalue, PyObject **ptb) {
        PyErr_Fetch(ptype, pvalue, ptb);
    }
}

@fn restore {
    /* Restore a previously fetched exception triple. */
    static inline void
    ${PREFIX}_restore(PyObject *type, PyObject *value, PyObject *tb) {
        PyErr_Restore(type, value, tb);
    }
}

@fn normalize {
    static inline void
    ${PREFIX}_normalize(PyObject **ptype, PyObject **pvalue, PyObject **ptb) {
        PyErr_NormalizeException(ptype, pvalue, ptb);
    }
}

@fn matches {
    /* True if the pending exception is an instance of `exc`. */
    static inline int
    ${PREFIX}_matches(PyObject *exc) {
        return PyErr_ExceptionMatches(exc);
    }
}

@fn given_exception_matches {
    static inline int
    ${PREFIX}_given_exception_matches(PyObject *raised, PyObject *exc) {
        return PyErr_GivenExceptionMatches(raised, exc);
    }
}

@fn print {
    static inline void
    ${PREFIX}_print(void) {
        PyErr_PrintEx(1);
    }
}

@fn write_unraisable {
    static inline void
    ${PREFIX}_write_unraisable(PyObject *o) {
        PyErr_WriteUnraisable(o);
    }
}

@fn set_from_errno {
    /* New reference to the OSError matching the current errno. */
    static inline PyObject *
    ${PREFIX}_set_from_errno(PyObject *exc) {
        return PyErr_SetFromErrno(exc);
    }
}
