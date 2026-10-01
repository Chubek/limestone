# pybind.q — aggregate header for the dompybind Python C API wrappers.
#
# Importing this module pulls in every dompybind submodule, so a single
# generated header exposes the whole wrapper. Compile with a search path
# that includes both domlibs/dompybind and domlibs/domqlib (the latter is
# needed by pyvec/pystr, which reuse domqlib's vector/string modules).
#
# Each submodule is imported with an explicit PREFIX so it keeps its own
# namespace (an @import otherwise inherits this module's `pybind` prefix,
# which would collide `size`/`get`/`set`/`new` across containers).

@module  pybind
@version 1.0

@include <Python.h>

@guard

@import pyinit   PREFIX=pyinit
@import pyobj    PREFIX=pyobj
@import pyconv   PREFIX=pyconv
@import pystr    PREFIX=pystr
@import pylist   PREFIX=pylist
@import pytuple  PREFIX=pytuple
@import pydict   PREFIX=pydict
@import pycall   PREFIX=pycall
@import pyerr    PREFIX=pyerr
@import pyargs   PREFIX=pyargs
@import pymodule PREFIX=pymodule
@import pyvec    PREFIX=pyvec
