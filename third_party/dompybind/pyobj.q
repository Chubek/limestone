# pyobj.q — reference counting and general object inspection.
#
# Provides safe, named helpers for the most common PyObject* operations:
# reference management, type checks, truth tests, repr/str, rich
# comparison and the singleton constructors.

@module  pyobj
@version 1.0

@param PREFIX pyobj

@include <Python.h>

@guard

@fn incref {
    static inline void
    ${PREFIX}_incref(PyObject *o) {
        Py_INCREF(o);
    }
}

@fn decref {
    static inline void
    ${PREFIX}_decref(PyObject *o) {
        Py_DECREF(o);
    }
}

@fn xincref {
    static inline void
    ${PREFIX}_xincref(PyObject *o) {
        Py_XINCREF(o);
    }
}

@fn xdecref {
    static inline void
    ${PREFIX}_xdecref(PyObject *o) {
        Py_XDECREF(o);
    }
}

@fn newref {
    /* Return a new strong reference to o (o must be non-NULL). */
    static inline PyObject *
    ${PREFIX}_newref(PyObject *o) {
        return Py_NewRef(o);
    }
}

@fn xnewref {
    /* Like newref, but accepts NULL and returns NULL. */
    static inline PyObject *
    ${PREFIX}_xnewref(PyObject *o) {
        return Py_XNewRef(o);
    }
}

@fn type {
    static inline PyTypeObject *
    ${PREFIX}_type(PyObject *o) {
        return Py_TYPE(o);
    }
}

@fn type_check {
    /* True if o is an instance of `type` or a subtype. */
    static inline int
    ${PREFIX}_type_check(PyObject *o, PyTypeObject *type) {
        return PyObject_TypeCheck(o, type);
    }
}

@fn is_true {
    /* True if o is truthy; -1 on error. */
    static inline int
    ${PREFIX}_is_true(PyObject *o) {
        return PyObject_IsTrue(o);
    }
}

@fn not {
    /* True if o is falsy; -1 on error. */
    static inline int
    ${PREFIX}_not(PyObject *o) {
        return PyObject_Not(o);
    }
}

@fn repr {
    /* Return a new reference; NULL on failure. */
    static inline PyObject *
    ${PREFIX}_repr(PyObject *o) {
        return PyObject_Repr(o);
    }
}

@fn str {
    /* Return a new reference; NULL on failure. */
    static inline PyObject *
    ${PREFIX}_str(PyObject *o) {
        return PyObject_Str(o);
    }
}

@fn bytes {
    /* Return a new reference; NULL on failure. */
    static inline PyObject *
    ${PREFIX}_bytes(PyObject *o) {
        return PyObject_Bytes(o);
    }
}

@fn rich_compare_bool {
    /* Return the comparison result as a truth value; -1 on error. */
    static inline int
    ${PREFIX}_rich_compare_bool(PyObject *a, PyObject *b, int op) {
        return PyObject_RichCompareBool(a, b, op);
    }
}

@fn hash {
    /* Return the hash value; -1 on failure. */
    static inline Py_hash_t
    ${PREFIX}_hash(PyObject *o) {
        return PyObject_Hash(o);
    }
}

@fn get_item {
    /* Borrowed-key getitem. Return a new reference; NULL on failure. */
    static inline PyObject *
    ${PREFIX}_get_item(PyObject *o, PyObject *key) {
        return PyObject_GetItem(o, key);
    }
}

@fn set_item {
    static inline int
    ${PREFIX}_set_item(PyObject *o, PyObject *key, PyObject *value) {
        return PyObject_SetItem(o, key, value);
    }
}

@fn del_item {
    static inline int
    ${PREFIX}_del_item(PyObject *o, PyObject *key) {
        return PyObject_DelItem(o, key);
    }
}

@fn get_iter {
    /* Return a new reference to an iterator; NULL on failure. */
    static inline PyObject *
    ${PREFIX}_get_iter(PyObject *o) {
        return PyObject_GetIter(o);
    }
}

@fn iter_next {
    /* Return a new reference to the next item, or NULL when exhausted
     * (with no exception set) or on error (exception set). */
    static inline PyObject *
    ${PREFIX}_iter_next(PyObject *it) {
        return PyIter_Next(it);
    }
}

@fn none {
    /* Return a new reference to None. */
    static inline PyObject *
    ${PREFIX}_none(void) {
        return Py_NewRef(Py_None);
    }
}

@fn is_none {
    static inline int
    ${PREFIX}_is_none(PyObject *o) {
        return o == Py_None;
    }
}

@fn true {
    /* Return a new reference to True. */
    static inline PyObject *
    ${PREFIX}_true(void) {
        return Py_NewRef(Py_True);
    }
}

@fn false {
    /* Return a new reference to False. */
    static inline PyObject *
    ${PREFIX}_false(void) {
        return Py_NewRef(Py_False);
    }
}

@fn bool {
    /* Return a new reference to True or False for a C int. */
    static inline PyObject *
    ${PREFIX}_bool(int v) {
        return PyBool_FromLong(v);
    }
}
