# dompybind — maintenance notes

`dompybind` is a Q-language library (see `../domqlib/AGENTS.md` for the Q
language and its compiler). It wraps Python's C API as name-spaced,
header-only `static inline` functions. This file documents the design rules
specific to dompybind so future edits stay consistent.

## Layout

```
dompybind/
  *.q          # one module per C API concern
  pybind.q     # aggregate: @imports every other module
  README.md    # user-facing docs
  AGENTS.md    # this file
```

The C modules are templates compiled on demand with `q.QCompiler`.
`include/dompybind/` provides the C++ ownership and binding layer.
`CMakeLists.txt` generates private Q primitives and publishes the interface
 target `DomPyBind::DomPyBind`; `cmake/DomPyBind.cmake` supports embedding and
 extension-module consumers. Preserve all existing C wrapper signatures when
 adding checked alternatives.

## Module rules

1. **One concern per module.** Modules mirror the C API's own headers
   (`pyinit`, `pyobj`, `pyconv`, `pystr`, `pylist`, `pytuple`, `pydict`,
   `pycall`, `pyerr`, `pyargs`, `pymodule`, `pyvec`).
2. **`@param PREFIX <module>` always.** The prefix defaults to the module
   name, so `pyobj` emits `pyobj_*`, etc. Users may override it.
3. **`@include <Python.h>` first**, then any `<stdarg.h>`/`<stddef.h>`, then
   domqlib `@import`s, then `@guard`.
4. **Prefer `static inline` functions.** Only fall back to `@raw` macros
   when the underlying C API is variadic with no `va_list` entry point
   (`PyTuple_Pack`, `PyObject_CallMethod`, `PyObject_CallFunction`,
   `PyArg_UnpackTuple`), or for token-pasting helpers (`pymodule_define`).
5. **Use the `*V` variants for variadic wrappers** so they stay true
   functions: `Py_VaBuildValue`, `PyArg_VaParse`,
   `PyArg_VaParseTupleAndKeywords`, `PyErr_FormatV`, `PyUnicode_FromFormatV`.

## The aggregate (`pybind.q`)

`pybind.q` `@import`s each submodule **with an explicit `PREFIX=`**:

```q
@import pylist PREFIX=pylist
```

This is required because `@import` inherits the parent's bindings by
default; without the explicit prefix every submodule would be renamed
`pybind_*` and their shared `size`/`get`/`set`/`new`/`clear` functions would
collide. Each import must name the submodule's own prefix.

When adding a module, add a matching `@import` line here **and** add the
module to `tests/dompybind-test-suite/test_pybind_compile.py`'s
`MODULE_PARAMS`.

## Leveraging domqlib

- `pyvec.q` — `@import vector T=PyObject* PREFIX=pyvec_raw`, then layers
  INCREF/DECREF on top of the imported vector's push/get/pop/clear/free.
- `pystr.q` — `@import string PREFIX=pysb`, then exposes `pysb_t` helpers
  that accumulate UTF-8/repr text.

If a new module needs a general-purpose container or string helper that
belongs in domqlib's standard library rather than in a Python-specific
module, add it to `domlibs/domqlib/*.q` first and `@import` it here.

## API target

Code targets CPython 3.13+ (developed against 3.14). Use the current
function forms:

- `PyModule_AddObjectRef` (not the deprecated `PyModule_AddObject`).
- `PyDict_GetItemRef` / `PyDict_GetItemStringRef` / `PyList_GetItemRef`
  (three/two-argument out-parameter forms, returning `int`/`PyObject*`).
- `PyObject_GetOptionalAttrString(obj, name, PyObject **result)` (returns
  `int`).
- `PyObject_CallNoArgs`; avoid `PyObject_CallOneArg` (not in the stable
  headers) and deprecated `Py_SetProgramName`.

`-Wall -Wextra` must stay warning-free — the test harness compiles with it.
