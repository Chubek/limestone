# pystr.q — string (unicode) and bytes helpers.
#
# Wrappers over PyUnicode/PyBytes construction and inspection. Also
# leverages domqlib's `string` module (imported under the `pysb` prefix)
# to provide a growable C string builder that can accumulate reprs and
# UTF-8 payloads before handing them off to Python.

@module  pystr
@version 1.0

@param PREFIX pystr

@include <Python.h>
@include <limits.h>
@include <stdarg.h>

@import string PREFIX=pysb

@guard

@fn from_cstr {
    /* New reference to a str decoded from a NUL-terminated UTF-8 string. */
    static inline PyObject *
    ${PREFIX}_from_cstr(const char *s) {
        return PyUnicode_FromString(s);
    }
}

@fn from_cstrn {
    /* New reference to a str from `n` UTF-8 bytes (not NUL-terminated). */
    static inline PyObject *
    ${PREFIX}_from_cstrn(const char *s, Py_ssize_t n) {
        return PyUnicode_FromStringAndSize(s, n);
    }
}

@fn from_object {
    /* New reference to str(o); NULL on failure. */
    static inline PyObject *
    ${PREFIX}_from_object(PyObject *o) {
        return PyUnicode_FromObject(o);
    }
}

@fn as_utf8 {
    /* Borrowed pointer to the UTF-8 encoding (cached). NULL on error. */
    static inline const char *
    ${PREFIX}_as_utf8(PyObject *o) {
        return PyUnicode_AsUTF8(o);
    }
}

@fn as_utf8_and_size {
    /* As above, also returning the length in *size. */
    static inline const char *
    ${PREFIX}_as_utf8_and_size(PyObject *o, Py_ssize_t *size) {
        return PyUnicode_AsUTF8AndSize(o, size);
    }
}

@fn size {
    /* Number of code points. */
    static inline Py_ssize_t
    ${PREFIX}_size(PyObject *o) {
        return PyUnicode_GetLength(o);
    }
}

@fn check {
    static inline int
    ${PREFIX}_check(PyObject *o) {
        return PyUnicode_Check(o);
    }
}

@fn concat {
    /* New reference to left + right; NULL on failure. */
    static inline PyObject *
    ${PREFIX}_concat(PyObject *left, PyObject *right) {
        return PyUnicode_Concat(left, right);
    }
}

@fn from_format {
    /* printf-style formatting into a new str. */
    static inline PyObject *
    ${PREFIX}_from_format(const char *fmt, ...) {
        va_list ap;
        PyObject *r;
        va_start(ap, fmt);
        r = PyUnicode_FromFormatV(fmt, ap);
        va_end(ap);
        return r;
    }
}

# -- bytes ---------------------------------------------------------

@fn bytes_from_cstrn {
    static inline PyObject *
    ${PREFIX}_bytes_from_cstrn(const char *s, Py_ssize_t n) {
        return PyBytes_FromStringAndSize(s, n);
    }
}

@fn bytes_from_cstr {
    static inline PyObject *
    ${PREFIX}_bytes_from_cstr(const char *s) {
        return PyBytes_FromString(s);
    }
}

@fn bytes_as_cstr {
    /* Borrowed NUL-terminated pointer to the bytes' data. */
    static inline char *
    ${PREFIX}_bytes_as_cstr(PyObject *o) {
        return PyBytes_AsString(o);
    }
}

@fn bytes_size {
    static inline Py_ssize_t
    ${PREFIX}_bytes_size(PyObject *o) {
        return PyBytes_Size(o);
    }
}

@fn bytes_check {
    static inline int
    ${PREFIX}_bytes_check(PyObject *o) {
        return PyBytes_Check(o);
    }
}

# -- domqlib string-builder integration ----------------------------

@fn sb_init {
    /* Initialise a domqlib string builder (pysb_t) for C-side text. */
    static inline void
    ${PREFIX}_sb_init(pysb_t *sb) {
        pysb_init(sb);
    }
}

@fn sb_try_append_n {
    /* Checked UTF-8 byte append, including embedded NULs. */
    static inline int
    ${PREFIX}_sb_try_append_n(pysb_t *sb, const char *s, Py_ssize_t n) {
        if (!s || n < 0) {
            PyErr_SetString(PyExc_ValueError, "invalid string buffer");
            return 0;
        }
        if (n >= INT_MAX - sb->len || !pysb_reserve(sb, sb->len + (int)n)) {
            PyErr_NoMemory();
            return 0;
        }
        memcpy(sb->data + sb->len, s, (size_t)n);
        sb->len += (int)n;
        return 1;
    }
}

@fn sb_append_cstr {
    static inline void
    ${PREFIX}_sb_append_cstr(pysb_t *sb, const char *s) {
        (void)${PREFIX}_sb_try_append_n(sb, s, (Py_ssize_t)strlen(s));
    }
}

@fn sb_append_repr {
    /* Append repr(o) as UTF-8 to the builder. Returns 0 on failure
     * (exception set), 1 on success. */
    static inline int
    ${PREFIX}_sb_append_repr(pysb_t *sb, PyObject *o) {
        PyObject *r = PyObject_Repr(o);
        Py_ssize_t n;
        const char *s;
        int result;
        if (!r) return 0;
        s = PyUnicode_AsUTF8AndSize(r, &n);
        result = s ? ${PREFIX}_sb_try_append_n(sb, s, n) : 0;
        Py_DECREF(r);
        return result;
    }
}

@fn sb_append_utf8 {
    /* Append str(o)'s UTF-8 to the builder. Returns 0 on failure, 1 on
     * success. */
    static inline int
    ${PREFIX}_sb_append_utf8(pysb_t *sb, PyObject *o) {
        Py_ssize_t n;
        const char *s = PyUnicode_AsUTF8AndSize(o, &n);
        return s ? ${PREFIX}_sb_try_append_n(sb, s, n) : 0;
    }
}

@fn sb_cstr {
    /* NUL-terminate and return the accumulated C string. */
    static inline const char *
    ${PREFIX}_sb_cstr(pysb_t *sb) {
        if (!pysb_reserve(sb, sb->len)) { PyErr_NoMemory(); return NULL; }
        sb->data[sb->len] = 0;
        return sb->data;
    }
}

@fn sb_free {
    static inline void
    ${PREFIX}_sb_free(pysb_t *sb) {
        pysb_free(sb);
    }
}
