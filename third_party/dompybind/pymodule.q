# pymodule.q — extension module construction and registration.
#
# Helpers around PyModule_Create, attribute/constant registration and a
# macro that emits the conventional `PyInit_<name>` entry point for a
# single-translation-unit extension module.

@module  pymodule
@version 1.0

@param PREFIX pymodule

@include <Python.h>

@guard

@fn create {
    /* Create a module object from `def` (a static PyModuleDef). New
     * reference; NULL on failure. */
    static inline PyObject *
    ${PREFIX}_create(PyModuleDef *def) {
        return PyModule_Create(def);
    }
}

@fn get_name {
    /* Borrowed reference to the module's name. */
    static inline const char *
    ${PREFIX}_get_name(PyObject *m) {
        return PyModule_GetName(m);
    }
}

@fn get_dict {
    /* Borrowed reference to the module's __dict__. */
    static inline PyObject *
    ${PREFIX}_get_dict(PyObject *m) {
        return PyModule_GetDict(m);
    }
}

@fn get_state {
    /* Opaque per-module state pointer (m_size > 0), else NULL. */
    static inline void *
    ${PREFIX}_get_state(PyObject *m) {
        return PyModule_GetState(m);
    }
}

@fn add_object_ref {
    /* Add `value` to the module under `name` (increments value). */
    static inline int
    ${PREFIX}_add_object_ref(PyObject *m, const char *name, PyObject *value) {
        return PyModule_AddObjectRef(m, name, value);
    }
}

@fn add_object {
    /* Add `value` to the module under `name` (steals a reference). */
    static inline int
    ${PREFIX}_add_object(PyObject *m, const char *name, PyObject *value) {
        int result = PyModule_AddObjectRef(m, name, value);
        if (result == 0) Py_DECREF(value);
        return result;
    }
}

@fn add_int_constant {
    static inline int
    ${PREFIX}_add_int_constant(PyObject *m, const char *name, long value) {
        return PyModule_AddIntConstant(m, name, value);
    }
}

@fn add_string_constant {
    static inline int
    ${PREFIX}_add_string_constant(PyObject *m, const char *name,
                                  const char *value) {
        return PyModule_AddStringConstant(m, name, value);
    }
}

@raw {
    /* Emit the conventional module initialisation function:
     *
     *   pymodule_define(spam, "spam module doc", spam_methods)
     *
     * expands to `PyInit_spam` creating a static module with the given
     * docstring and method table. `methods` is a `PyMethodDef` array
     * (NULL-terminated). Use in exactly one translation unit. */
    #define ${PREFIX}_define(name, doc, methods)                              \
        PyMODINIT_FUNC PyInit_##name(void) {                                  \
            static struct PyModuleDef _q_##name##_moduledef = {               \
                PyModuleDef_HEAD_INIT,                                        \
                #name,                                                        \
                doc,                                                          \
                -1,                                                           \
                methods,                                                      \
                NULL, /* m_slots */                                           \
                NULL, /* m_traverse */                                        \
                NULL, /* m_clear */                                           \
                NULL, /* m_free */                                            \
            };                                                                \
            return PyModule_Create(&_q_##name##_moduledef);                   \
        }
}

