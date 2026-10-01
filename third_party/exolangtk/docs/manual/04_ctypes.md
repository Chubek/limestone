# InteropTk: The C Type System {#manual_04_ctypes}

Module: [itk_ctypes.h](../include/InteropTk/itk_ctypes.h) | Stability: stable

## Overview

`itk_ctypes.h` provides a canonical, runtime-introspectable model of the C
type system. Instead of scattering `sizeof()` assumptions throughout your
codebase, you build `itk_type` descriptors and query them for size, alignment,
signedness, and classification. This is essential for compiler and interpreter
authors who need to reason about C types without a full C frontend.

## Type Kinds

~~~c
typedef enum itk_type_kind {
    ITK_KIND_VOID,
    ITK_KIND_CHAR,
    ITK_KIND_SCHAR,
    ITK_KIND_UCHAR,
    ITK_KIND_SHORT,
    ITK_KIND_USHORT,
    ITK_KIND_INT,
    ITK_KIND_UINT,
    ITK_KIND_LONG,
    ITK_KIND_ULONG,
    ITK_KIND_LLONG,
    ITK_KIND_ULLONG,
    ITK_KIND_FLOAT,
    ITK_KIND_DOUBLE,
    ITK_KIND_LDOUBLE,
    ITK_KIND_PTR,
    ITK_KIND_ARRAY,
    ITK_KIND_FUNC,
    ITK_KIND_ENUM
} itk_type_kind;
~~~

## The Type Descriptor

~~~c
typedef struct itk_type {
    itk_type_kind kind;
    unsigned quals;
    const struct itk_type *child;
    size_t length;
    const struct itk_type *const *params;
    size_t param_count;
    itk_bool variadic;
} itk_type;
~~~

- `child` — pointee for pointers, element type for arrays, return type for
  functions.
- `params` — parameter-type array for function types.
- `length` — element count for arrays (0 means incomplete `T[]`).
- `quals` — bitmask of `ITK_QUAL_CONST`, `ITK_QUAL_VOLATILE`,
  `ITK_QUAL_RESTRICT`.

## Type Qualifier Constants

~~~c
#define ITK_QUAL_CONST    0x1u
#define ITK_QUAL_VOLATILE 0x2u
#define ITK_QUAL_RESTRICT 0x4u
~~~

## Building Types

Static inline builders let you construct complex types declaratively:

~~~c
itk_type t_int  = itk_type_prim(ITK_KIND_INT);
itk_type t_ptr  = itk_type_ptr_to(&t_int);
itk_type t_arr  = itk_type_array_of(&t_int, 16);
itk_type t_func = itk_type_func(&t_int, NULL, 0, ITK_FALSE);
itk_type t_cptr = itk_type_qualify(t_ptr, ITK_QUAL_CONST);
~~~

For pointer, array, and function types, the child/params pointers are
borrowed. The caller must ensure they outlive the descriptor.

## Query Functions

~~~c
size_t itk_type_size(const itk_type *t);
size_t itk_type_align(const itk_type *t);
itk_bool itk_type_equal(const itk_type *a, const itk_type *b);
itk_bool itk_type_is_complete(const itk_type *t);
~~~

- `itk_type_size()` returns 0 for void, function types, and zero-length
  arrays.
- `itk_type_align()` returns the ABI alignment for the target.
- `itk_type_equal()` performs structural comparison, recursing into children.
- `itk_type_is_complete()` returns `ITK_FALSE` for void, function types, and
  incomplete arrays.

## Classification Helpers

~~~c
itk_bool itk_type_is_integer(itk_type_kind kind);
itk_bool itk_type_is_float(itk_type_kind kind);
itk_bool itk_type_is_signed(itk_type_kind kind);
itk_bool itk_char_is_signed(void);
const char *itk_type_kind_name(itk_type_kind kind);
~~~

`itk_char_is_signed()` returns the platform's signedness of plain `char`.

## Usage Example

~~~c
#define ITK_PLATFORM_IMPLEMENTATION
#define ITK_CTYPES_IMPLEMENTATION
#include "InteropTk/itk_ctypes.h"

itk_type t = itk_type_prim(ITK_KIND_DOUBLE);
printf("double: size=%zu align=%zu\n",
       itk_type_size(&t), itk_type_align(&t));
printf("kind name: %s\n", itk_type_kind_name(t.kind));
~~~
