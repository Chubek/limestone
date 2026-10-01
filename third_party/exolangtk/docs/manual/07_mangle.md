# InteropTk: Symbol Mangling {#manual_07_mangle}

Module: [itk_mangle.h](../include/InteropTk/itk_mangle.h) | Stability: stable

## Overview

`itk_mangle.h` provides symbol name decoration and demangling for linking
against C toolchains. It handles leading-underscore conventions, stdcall
suffixes, and visibility naming quirks per platform.

## Mangling Flags

~~~c
#define ITK_MANGLE_NONE       0x0u
#define ITK_MANGLE_STDCALL    0x1u
#define ITK_MANGLE_FASTCALL   0x2u
#define ITK_MANGLE_UNDERSCORE 0x4u
#define ITK_MANGLE_NAME_MAX   512
~~~

## Visibility

~~~c
typedef enum itk_symbol_visibility {
    ITK_VIS_DEFAULT,
    ITK_VIS_HIDDEN,
    ITK_VIS_PROTECTED,
    ITK_VIS_INTERNAL
} itk_symbol_visibility;

const char *itk_symbol_visibility_name(itk_symbol_visibility vis);
~~~

## Mangling and Demangling

~~~c
itk_bool itk_mangle_has_leading_underscore(void);
itk_bool itk_mangle_c_symbol(const char *name, unsigned flags,
                             size_t arg_bytes, char *out, size_t cap);
itk_bool itk_demangle_c_symbol(const char *mangled, char *out, size_t cap,
                               unsigned *flags);
~~~

- `itk_mangle_has_leading_underscore()` returns `ITK_TRUE` on platforms that
  prepend `_` to C symbols (macOS, Windows 32-bit, some ELF targets).
- `itk_mangle_c_symbol()` decorates a name. `arg_bytes` is the argument-stack
  size for stdcall/fastcall suffixes.
- `itk_demangle_c_symbol()` strips decoration and reports the detected flags.

## Platform-Specific Behaviour

| Platform | Leading underscore | stdcall suffix |
|----------|-------------------|----------------|
| Linux x86-64 | No | None |
| Windows x86-64 | No | None |
| Windows x86 (32-bit) | Yes | `@N` |
| macOS x86-64 | Yes | None |
| macOS AArch64 | No | None |

## Usage Example

~~~c
#define ITK_PLATFORM_IMPLEMENTATION
#define ITK_MANGLE_IMPLEMENTATION
#include "InteropTk/itk_mangle.h"

char buf[ITK_MANGLE_NAME_MAX];
itk_mangle_c_symbol("my_function", ITK_MANGLE_STDCALL, 16, buf, sizeof(buf));
/* On Win32: "_my_function@16" */
/* On Linux x86-64: "my_function" */
~~~
