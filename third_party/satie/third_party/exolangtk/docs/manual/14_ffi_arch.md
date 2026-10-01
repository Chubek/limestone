# FFItk: Architecture and Loader {#manual_14_ffi_arch}

Modules: [ffi_platform.h](../include/FFItk/ffi_platform.h),
[ffi_types.h](../include/FFItk/ffi_types.h),
[ffi_loader.h](../include/FFItk/ffi_loader.h),
[ffi_library.h](../include/FFItk/ffi_library.h) | Stability: experimental

## Overview

FFItk is the foreign function interface layer of ExolangTk. It provides the
machinery for loading shared libraries, describing call interfaces, building
call frames, invoking foreign functions, and creating trampolines and
closures. This chapter covers the platform shims, type system, and library
loader.

## Platform Shims

`ffi_platform.h` defines `FFI_DEF` (the FFItk function qualifier macro) and
re-exports target detection macros from InteropTk::platform as `FFI_OS_*` and
`FFI_ARCH_*` aliases. This allows FFItk headers to include only one
portability header.

## Common Types

`ffi_types.h` defines the shared types used across FFItk:

~~~c
typedef enum ffi_status {
    FFI_OK       = 0,
    FFI_EINVAL   = -1,
    FFI_ENOMEM   = -2,
    FFI_ENOTFOUND = -4,
    FFI_EFAIL    = -6
} ffi_status;

typedef uintptr_t ffi_arg;
typedef intptr_t ffi_sarg;
typedef union ffi_value {
    uint64_t u64;
    int64_t s64;
    double d;
    long double ld;
    void *ptr;
} ffi_value;
~~~

`ffi_arg` and `ffi_sarg` are word-sized slots for passing arguments.
`ffi_value` is the return-value slot.

## Library Loader

`ffi_loader.h` provides portable shared-library loading:

~~~c
typedef struct ffi_library ffi_library;

ffi_status ffi_library_open(const char *path, ffi_library **out);
ffi_status ffi_library_close(ffi_library *lib);
ffi_status ffi_library_find(const char *name, ffi_library **out);
ffi_status ffi_library_sym(ffi_library *lib, const char *name, void **out);
~~~

- `ffi_library_open()` loads a shared library by path. On POSIX this wraps
  `dlopen()`; on Windows it wraps `LoadLibraryEx()`.
- `ffi_library_close()` unloads the library.
- `ffi_library_find()` searches for a library by name, consulting the
  platform's default search paths.
- `ffi_library_sym()` resolves a symbol and returns its address.

## Library Management

`ffi_library.h` provides a higher-level library handle:

~~~c
typedef struct ffi_lib_handle ffi_lib_handle;

ffi_status ffi_lib_open(const char *path, ffi_lib_handle **out);
void ffi_lib_close(ffi_lib_handle *handle);
ffi_status ffi_lib_sym(ffi_lib_handle *handle, const char *name, void **out);
ffi_status ffi_lib_sym_opt(ffi_lib_handle *handle, const char *name,
                           void **out);
~~~

- `ffi_lib_sym_opt()` is like `ffi_lib_sym()` but returns `FFI_OK` with
  `*out = NULL` when the symbol is not found, rather than `FFI_ENOTFOUND`.

## Usage Example

~~~c
#define FFI_LOADER_IMPLEMENTATION
#define FFI_LIBRARY_IMPLEMENTATION
#include "FFItk/ffi_library.h"

ffi_lib_handle *lib = NULL;
ffi_status st = ffi_lib_open("libm.so.6", &lib);
if (st == FFI_OK) {
    void *cos_fn = NULL;
    ffi_lib_sym(lib, "cos", &cos_fn);
    ffi_lib_close(lib);
}
~~~
