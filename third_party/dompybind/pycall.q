# pycall.q — calling Python callables and attribute access.

@module  pycall
@version 1.0

@param PREFIX pycall

@include <Python.h>

@guard

@fn callable {
    static inline int
    ${PREFIX}_callable(PyObject *o) {
        return PyCallable_Check(o);
    }
}

@fn call_object {
    /* callable(args): args is a tuple (may be NULL). New ref or NULL. */
    static inline PyObject *
    ${PREFIX}_call_object(PyObject *callable, PyObject *args) {
        return PyObject_CallObject(callable, args);
    }
}

@fn call {
    /* callable(args, kwargs): either may be NULL. New ref or NULL. */
    static inline PyObject *
    ${PREFIX}_call(PyObject *callable, PyObject *args, PyObject *kwargs) {
        return PyObject_Call(callable, args, kwargs);
    }
}

@fn call_no_args {
    /* callable(). New ref or NULL. */
    static inline PyObject *
    ${PREFIX}_call_no_args(PyObject *callable) {
        return PyObject_CallNoArgs(callable);
    }
}

@fn get_attr {
    /* New reference to o.name_obj; NULL on failure. */
    static inline PyObject *
    ${PREFIX}_get_attr(PyObject *o, PyObject *name_obj) {
        return PyObject_GetAttr(o, name_obj);
    }
}

@fn get_attr_string {
    /* New reference to o.name; NULL on failure. */
    static inline PyObject *
    ${PREFIX}_get_attr_string(PyObject *o, const char *name) {
        return PyObject_GetAttrString(o, name);
    }
}

@fn set_attr_string {
    static inline int
    ${PREFIX}_set_attr_string(PyObject *o, const char *name, PyObject *v) {
        return PyObject_SetAttrString(o, name, v);
    }
}

@fn has_attr_string {
    static inline int
    ${PREFIX}_has_attr_string(PyObject *o, const char *name) {
        return PyObject_HasAttrString(o, name);
    }
}

@fn get_optional_attr_string {
    /* Fetch o.name without raising AttributeError. Returns 1 (result =
     * new ref) when present, 0 (result = NULL) when absent, -1 on other
     * error. */
    static inline int
    ${PREFIX}_get_optional_attr_string(PyObject *o, const char *name,
                                       PyObject **result) {
        return PyObject_GetOptionalAttrString(o, name, result);
    }
}

@raw {
    /* Varargs convenience aliases (format-based):
     *   pycall_method(obj, name, "fmt", ...)   -> PyObject_CallMethod
     *   pycall_call_function(func, "fmt", ...) -> PyObject_CallFunction
     */
    #define ${PREFIX}_method PyObject_CallMethod
    #define ${PREFIX}_call_function PyObject_CallFunction
}
