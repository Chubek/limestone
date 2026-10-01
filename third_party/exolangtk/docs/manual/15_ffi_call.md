# FFItk: Calls, Trampolines, and Closures {#manual_15_ffi_call}

Modules: [ffi_cif.h](../include/FFItk/ffi_cif.h),
[ffi_frame.h](../include/FFItk/ffi_frame.h),
[ffi_call.h](../include/FFItk/ffi_call.h),
[ffi_trampoline.h](../include/FFItk/ffi_trampoline.h),
[ffi_closure.h](../include/FFItk/ffi_closure.h) | Stability: experimental

## Overview

This chapter covers the core FFItk modules for describing and invoking foreign
functions. The call interface (CIF) describes a function's signature. The
frame builder prepares argument and return-value storage. The call module
performs the actual invocation. Trampolines and closures enable C functions
to be called from foreign code and vice versa.

## Call Interface (CIF)

`ffi_cif.h` describes a function's ABI-level signature:

~~~c
typedef struct ffi_cif ffi_cif;

ffi_status ffi_cif_init(ffi_cif *cif, const itk_type *ret,
                        const itk_type *const *params, size_t count,
                        itk_callconv cc);
void ffi_cif_fini(ffi_cif *cif);
size_t ffi_cif_arg_count(const ffi_cif *cif);
size_t ffi_cif_stack_size(const ffi_cif *cif);
~~~

- `ffi_cif_init()` builds a CIF from a return type, parameter types, and a
  calling convention.
- `ffi_cif_fini()` releases any resources allocated during initialization.
- `ffi_cif_arg_count()` returns the number of arguments.
- `ffi_cif_stack_size()` returns the stack space needed for the call.

## Frame Builder

`ffi_frame.h` prepares argument and return-value storage:

~~~c
typedef struct ffi_frame ffi_frame;

ffi_status ffi_frame_init(ffi_frame *frame, const ffi_cif *cif);
void ffi_frame_fini(ffi_frame *frame);
ffi_status ffi_frame_set_arg(ffi_frame *frame, size_t index, ffi_value val);
ffi_status ffi_frame_set_ptr(ffi_frame *frame, size_t index, void *ptr);
ffi_value ffi_frame_get_ret(const ffi_frame *frame);
~~~

- `ffi_frame_init()` allocates storage for arguments and return value.
- `ffi_frame_set_arg()` sets a scalar argument by value.
- `ffi_frame_set_ptr()` sets a pointer-sized argument.
- `ffi_frame_get_ret()` retrieves the return value after the call.

## Call

`ffi_call.h` performs the actual invocation:

~~~c
ffi_status ffi_call(const ffi_cif *cif, void *fn, ffi_frame *frame);
~~~

`ffi_call()` invokes the function at address `fn` with the signature described
by `cif` and the arguments stored in `frame`. The return value is written
into `frame` and can be retrieved with `ffi_frame_get_ret()`.

## Trampolines

`ffi_trampoline.h` creates executable trampolines — small code stubs that
bridge between calling conventions or marshal arguments:

~~~c
typedef struct ffi_trampoline ffi_trampoline;

ffi_status ffi_trampoline_alloc(const ffi_cif *cif, void *fn,
                                ffi_trampoline **out);
void ffi_trampoline_free(ffi_trampoline *t);
void *ffi_trampoline_code(const ffi_trampoline *t);
~~~

`ffi_trampoline_code()` returns a function pointer to the generated code.
Trampolines require executable memory; on platforms where this is not
available (e.g. strict W^X environments), the functions return `FFI_ENOSYS`.

## Closures

`ffi_closure.h` creates closures — callable C function pointers that wrap a
user-supplied callback:

~~~c
typedef void (*ffi_closure_fn)(const ffi_cif *cif, ffi_value *ret,
                               ffi_value *args, void *user_data);

typedef struct ffi_closure ffi_closure;

ffi_status ffi_closure_alloc(const ffi_cif *cif, ffi_closure_fn cb,
                             void *user_data, ffi_closure **out);
void ffi_closure_free(ffi_closure *cl);
void *ffi_closure_code(const ffi_closure *cl);
~~~

When the closure is called, the callback receives the CIF, a pointer to the
return-value slot, the argument array, and the user data pointer. Like
trampolines, closures require executable memory.

## Usage Example

~~~c
#define FFI_CIF_IMPLEMENTATION
#define FFI_FRAME_IMPLEMENTATION
#define FFI_CALL_IMPLEMENTATION
#include "FFItk/ffi_call.h"

/* int add(int a, int b) */
itk_type t_int = itk_type_prim(ITK_KIND_INT);
const itk_type *params[] = { &t_int, &t_int };
ffi_cif cif;
ffi_cif_init(&cif, &t_int, params, 2, ITK_CALLCONV_DEFAULT);

ffi_frame frame;
ffi_frame_init(&frame, &cif);
ffi_frame_set_arg(&frame, 0, (ffi_value){ .s64 = 3 });
ffi_frame_set_arg(&frame, 1, (ffi_value){ .s64 = 4 });

ffi_call(&cif, (void *)&add, &frame);
printf("result: %ld\n", (long)ffi_frame_get_ret(&frame).s64);

ffi_frame_fini(&frame);
ffi_cif_fini(&cif);
~~~
