# InteropTk: Calling Conventions {#manual_06_callconv}

Module: [itk_callconv.h](../include/InteropTk/itk_callconv.h) | Stability: experimental

## Overview

`itk_callconv.h` provides declarative metadata for calling conventions. It
classifies function arguments and return values into register vs. stack
categories per the target ABI (SysV, Win64, AAPCS, etc.). This is metadata
only; actual foreign calls are performed by FFItk.

## Calling Convention Enum

~~~c
typedef enum itk_callconv {
    ITK_CALLCONV_DEFAULT,
    ITK_CALLCONV_CDECL,
    ITK_CALLCONV_STDCALL,
    ITK_CALLCONV_FASTCALL,
    ITK_CALLCONV_THISCALL,
    ITK_CALLCONV_SYSV,
    ITK_CALLCONV_WIN64,
    ITK_CALLCONV_AAPCS
} itk_callconv;
~~~

## Argument Classification

~~~c
typedef enum itk_arg_class {
    ITK_ARG_NO_CLASS,
    ITK_ARG_INTEGER,
    ITK_ARG_SSE,
    ITK_ARG_SSEUP,
    ITK_ARG_X87,
    ITK_ARG_X87UP,
    ITK_ARG_COMPLEX_X87,
    ITK_ARG_MEMORY
} itk_arg_class;
~~~

## Classification Functions

~~~c
itk_bool itk_classify_args(itk_callconv cc, const itk_type *ret,
                           const itk_type *const *params, size_t count,
                           itk_arg_class *ret_classes, size_t ret_class_count,
                           itk_arg_class *arg_classes, size_t arg_class_count);
itk_bool itk_classify_return(itk_callconv cc, const itk_type *ret,
                             itk_arg_class *classes, size_t class_count);
~~~

- `itk_classify_args()` assigns a classification to each argument and to the
  return value.
- `itk_classify_return()` classifies only the return value.
- Each argument may consume up to two classification slots (for eight-byte
  aggregates split across registers).

## How Classification Works

For each argument, the classifier determines whether it is passed:

- In integer registers (`ITK_ARG_INTEGER`)
- In SSE/vector registers (`ITK_ARG_SSE`, `ITK_ARG_SSEUP`)
- On the x87 stack (`ITK_ARG_X87`, `ITK_ARG_X87UP`)
- In memory via a hidden pointer (`ITK_ARG_MEMORY`)

The classification depends on the type, its size, and the calling convention.

## Usage Example

~~~c
#define ITK_PLATFORM_IMPLEMENTATION
#define ITK_CTYPES_IMPLEMENTATION
#define ITK_LAYOUT_IMPLEMENTATION
#define ITK_CALLCONV_IMPLEMENTATION
#include "InteropTk/itk_callconv.h"

itk_type t_int = itk_type_prim(ITK_KIND_INT);
itk_type t_dbl = itk_type_prim(ITK_KIND_DOUBLE);
const itk_type *params[] = { &t_int, &t_dbl };
itk_arg_class ret_class[2], arg_classes[4];

itk_classify_args(ITK_CALLCONV_SYSV, &t_int, params, 2,
                  ret_class, 2, arg_classes, 4);
~~~
