# dompybind

Header-only wrappers over Python's C API, written in the
[Q language](../domqlib) and compiled with domqlib's `QCompiler`.

`dompybind` turns the sprawling, macro-heavy `Python.h` surface into a set
of small, name-spaced, `static inline` functions — one `.q` module per
concern — so embedding and extending CPython from C reads like calling any
other library.

## Why Q / domqlib?

Every module is a `.q` template: parameters are bound at compile time,
include guards are derived from the bindings, and blocks are substituted
through domqlib's Tcl engine. The C wrappers are generated headers: compile
the modules you need to `.h` files and `#include` them. CMake generates the
required headers automatically for C++ consumers.

It also reuses domqlib's standard library:

- `pyvec.q` wraps domqlib's `vector` (`T=PyObject*`) with Python
  reference-count management — `append` increments, `clear`/`free`
  decrement, `steal` transfers ownership.
- `pystr.q` imports domqlib's `string` builder to accumulate C-side text
  (for example `repr` output) before handing it back to Python.

## Modules

| Module    | Wraps |
|-----------|-------|
| `pyinit`  | Interpreter lifecycle + GIL (`Py_Initialize`, `Py_FinalizeEx`, `PyGILState_*`, `PyImport_AppendInittab`) |
| `pyobj`   | Reference counting, type checks, truth/repr/str, rich compare, singletons |
| `pyconv`  | `long`/`float`/`bool` conversion between C scalars and Python objects |
| `pystr`   | `unicode`/`bytes` construction + the domqlib string-builder bridge |
| `pylist`  | `list` construction and inspection |
| `pytuple` | `tuple` construction and inspection |
| `pydict`  | `dict` construction and inspection |
| `pycall`  | Calling callables and attribute access |
| `pyerr`   | Exception state: fetch/restore, format, matches, print |
| `pyargs`  | Argument parsing (`PyArg_VaParse` and friends) |
| `pymodule`| Extension module creation + the `pymodule_define(...)` entry-point macro |
| `pyvec`   | Reference-managed `PyObject*` vector (reuses domqlib `vector`) |
| `pybind`  | Aggregate header that imports all of the above |

## Compiling

Point `QCompiler` at both `dompybind` and `domqlib` so `@import` can resolve
`pyvec`/`pystr`'s dependency on domqlib's `vector`/`string`:

```python
import sys
sys.path.insert(0, "domlibs/domqlib")
from q import QCompiler

compiler = QCompiler(search_path=["domlibs/dompybind", "domlibs/domqlib"])

header = compiler.compile("domlibs/dompybind/pybind.q")      # whole library
header = compiler.compile("domlibs/dompybind/pylist.q")      # just lists
header = compiler.compile("domlibs/dompybind/pylist.q", PREFIX="mylist")
```

Or with the `qc` CLI:

```bash
qc -I domlibs/dompybind -I domlibs/domqlib domlibs/dompybind/pybind.q --stdout > pybind.h
```

Every module defaults its `PREFIX` to its own name, so functions are
`pyobj_incref`, `pylist_append`, `pyerr_format`, … and two instantiations of
the same module with different `PREFIX` values coexist in one translation
unit (distinct include guards).

## Example — embedding

```c
/* compile with: gcc $(python3-config --includes) app.c $(python3-config --embed --ldflags) */
#include "pybind.h"

int main(void) {
    pyinit_initialize();

    PyObject *a = pyconv_long_from_long(40);
    PyObject *b = pyconv_long_from_long(2);

    PyObject *mod = PyImport_ImportModule("builtins");
    PyObject *fn  = pycall_get_attr_string(mod, "pow");
    PyObject *arg = pytuple_pack(2, a, b);
    PyObject *res = pycall_call_object(fn, arg);
    long r = pyconv_long_as_long(res);            /* 1600 */

    Py_DECREF(res); Py_DECREF(arg); Py_DECREF(fn); Py_DECREF(mod);
    Py_DECREF(a); Py_DECREF(b);
    pyinit_finalize();
    return r == 1600 ? 0 : 1;
}
```

## Example — extension module

```c
#include "pybind.h"

static PyObject *add(PyObject *self, PyObject *args) {
    long x, y;
    (void)self;
    if (!pyargs_parse_tuple(args, "ll", &x, &y)) return NULL;
    return pyconv_long_from_long(x + y);
}

static PyMethodDef methods[] = {
    {"add", add, METH_VARARGS, "add(x, y)"},
    {NULL, NULL, 0, NULL}
};

/* emits PyInit_calc */
pymodule_define(calc, "a calculator module", methods)
```

## Reference-managed object vector

```c
pyvec_t v;
pyvec_init(&v);
pyvec_append(&v, obj);   /* INCREF, then store   */
pyvec_steal(&v, obj);    /* store, take ownership */
PyObject *tuple = pyvec_tuple(&v);
pyvec_free(&v);          /* DECREF everything     */
```

## Tests

`tests/dompybind-test-suite/` compiles every module, then builds and runs C
programs against the generated headers (linking the running interpreter).
See `tests/dompybind-test-suite/README.md`.

## C++ embedding and extension modules

`include/dompybind/dompybind.hpp` adds checked object ownership, Python exception
transport, GIL guards, STL conversion, and native function/class publication.
It uses the generated Q primitives and CPython directly. No external binding
package or downloaded binding runtime is required.

```cmake
include("${DOMWEAVE_ROOT}/domlibs/dompybind/cmake/DomPyBind.cmake")
target_link_libraries(my_embedding_host PRIVATE DomPyBind::DomPyBind)
dompybind_add_module(my_extension extension.cpp)
```

Build requirements are CMake 3.18+, C++17, CPython 3.13+ development libraries,
and Python Tcl/Tk support for the local Q compiler. `DomPyBind::DomPyBind` is an
interface target: it generates `dompybind/capi.h`, propagates public includes
and Python linking, and has no separate runtime archive. Standalone installation
also exports a `find_package(DomPyBind CONFIG REQUIRED)` package.

```cpp
#include <dompybind/dompybind.hpp>
namespace py = dompybind;

struct Counter {
    int value;
    explicit Counter(int initial) : value(initial) {}
    int increment(int n) { return value += n; }
};

DOMPYBIND_MODULE(counters, module) {
    py::class_<Counter>(module, "Counter")
        .def(py::init<int>(), py::arg("initial") = 0)
        .def_rw("value", &Counter::value)
        .def("increment", &Counter::increment, py::arg("n") = 1);
}
```

The binding layer implements functions, overloads, constructors, methods,
static methods, readonly/readwrite properties, class values, `IntEnum` types,
keyword/default arguments, and exception translation. STL conversions cover
strings, vectors, maps, unordered maps, optionals and variants. Owned outputs
may use `unique_ptr` or `shared_ptr`. Integer narrowing is checked; strings
and bytes retain explicit lengths. Existing native and Python entry-point
names in consumers are preserved.

`object` and `handle` both retain strong references. Copying increments the
reference count; moving transfers ownership. `borrow()` retains an existing
reference and `steal()` adopts a new reference. `release()`/`release_ptr()` hand
the owned pointer to the caller. `attr()` and item access retain their parent
while an assignment is pending. Python failures are transported by
`python_error`, which retains the original exception and can restore it.
No C++ exception escapes an extension callback.

Native values returned by value are owned copies/moves. Pointer results are
borrowed by default: use `rv_policy::take_ownership` for a newly allocated
pointer or `rv_policy::reference_internal` to retain the receiver while a
returned child is alive. Uninitialized native instances are rejected before
method invocation. Native type registrations belong to the interpreter;
callback storage belongs to Python capsules.

### Interpreter and threading requirements

Embedding hosts own interpreter initialization and finalization. Acquire
`gil_scoped_acquire` before calling Python, and use `gil_scoped_release` around
blocking native work when the current thread holds the GIL. All live Python
objects must be destroyed before finalization; native code must coordinate
shutdown with workers. Object reference cleanup acquires the GIL, but ordinary
attribute, conversion, iteration, and call operations require the caller to
hold it. A Python extension callback already holds it.

The C++ layer targets the main, GIL-enabled interpreter. Concurrent interpreter
startup/shutdown, subinterpreters, free-threaded execution, automatic traversal
of Python references hidden in arbitrary C++ members, and implicit C++ base-class
casts are not supported. Native classes containing Python references must avoid
uncollectable ownership cycles. These constraints are explicit; the layer does
not silently substitute stub results.

### Checked C buffers

The original C wrapper signatures remain available. Prefer
`pyvec_try_append` and `pyvec_try_steal` when handling allocation failure:
they return `0` on success and `-1` with a Python exception on failure. Failed
insertion leaves both ownership and vector contents unchanged. The legacy void
forms set the same exception; callers must check it. Invalid indexes and empty
pops return NULL with `IndexError`.

String-builder appends preserve embedded NUL bytes and report allocation
failures through the Python error indicator. `pystr_sb_try_append_n` returns
`1` on success and `0` on failure. `pydict_get` remains a borrowed-reference
legacy lookup; use `pydict_get_ref` for checked lookup with explicit ownership.
The shared DomQLib buffer implementations now perform allocation in release
builds too, and check capacity overflow before growing.

The test suite compiles both C and C++ callers. It exercises injected allocator
failures, `NDEBUG`, reference counts, owned/borrowed results, overloads, keyword
validation, exception recovery, multithreaded embedding, and interpreter
reinitialization with Python's debug allocator.
