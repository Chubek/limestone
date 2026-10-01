# pyinit.q — Python interpreter lifecycle and GIL management.
#
# Thin, namespace-safe wrappers over the interpreter startup/shutdown and
# GIL functions of Python's C API. All wrappers are `static inline` and
# header-only, following the Q language conventions.

@module  pyinit
@version 1.0

@param PREFIX pyinit

@include <Python.h>

@guard

@fn initialize {
    static inline void
    ${PREFIX}_initialize(void) {
        Py_Initialize();
    }
}

@fn finalize {
    /* Return 0 on success, -1 on failure (Py_FinalizeEx semantics). */
    static inline int
    ${PREFIX}_finalize(void) {
        return Py_FinalizeEx();
    }
}

@fn is_initialized {
    static inline int
    ${PREFIX}_is_initialized(void) {
        return Py_IsInitialized();
    }
}

@fn gil_ensure {
    static inline PyGILState_STATE
    ${PREFIX}_gil_ensure(void) {
        return PyGILState_Ensure();
    }
}

@fn gil_release {
    static inline void
    ${PREFIX}_gil_release(PyGILState_STATE s) {
        PyGILState_Release(s);
    }
}

@fn append_inittab {
    /* Register a built-in module so `import name` resolves to initfunc.
     * Must be called before Py_Initialize. Returns 0 on success, -1 on
     * error (name already registered). */
    static inline int
    ${PREFIX}_append_inittab(const char *name, PyObject *(*initfunc)(void)) {
        return PyImport_AppendInittab(name, initfunc);
    }
}
