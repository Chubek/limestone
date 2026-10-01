# ExolangTk {#mainpage}

**C99 Header-Only Toolkit for Compiler and Interpreter Interoperability**

Version 0.1.0 — MIT License

## Overview

ExolangTk is a collection of four subsystems — InteropTk, FFItk, DebugTk, and
ExtensionTk — that together provide the building blocks for C interoperability
in compilers, interpreters, and language runtimes. Every module is a single,
self-contained C99 header distributed under the MIT license.

| Subsystem | Prefix | Purpose |
|-----------|--------|---------|
| InteropTk | `itk_` | Platform introspection, C type model, layout, marshalling, strings, errors, allocators |
| FFItk | `ffi_` | Foreign function interface: loader, call interface, trampolines, closures |
| DebugTk | `dtk_` | Debugging primitives: symbol resolution, stack unwinding, breakpoints |
| ExtensionTk | `etk_` | Extension management: dynamic loading, versioning, registry, service tables |

## Quick Start

~~~c
#define ITK_PLATFORM_IMPLEMENTATION
#include "InteropTk/itk_platform.h"

int main(void) {
    itk_target_info info;
    itk_target_query(&info);
    return 0;
}
~~~

Compile with any C99 toolchain:

~~~
cc -std=c99 -Iinclude -o example example.c
~~~

## Manual

The ExolangTk Manual is an eighteen-chapter guide covering every subsystem
and module in depth:

- \subpage manual_01_introduction
- \subpage manual_02_build
- \subpage manual_03_platform
- \subpage manual_04_ctypes
- \subpage manual_05_layout
- \subpage manual_06_callconv
- \subpage manual_07_mangle
- \subpage manual_08_cdecl
- \subpage manual_09_marshal
- \subpage manual_10_cstring
- \subpage manual_11_error
- \subpage manual_12_alloc
- \subpage manual_13_export
- \subpage manual_14_ffi_arch
- \subpage manual_15_ffi_call
- \subpage manual_16_dtk_sym
- \subpage manual_17_dtk_bp
- \subpage manual_18_etk

## Generating Documentation

~~~
cmake -S . -B build -DEXOLANGTK_BUILD_DOCS=ON
cmake --build build --target exolangtk-docs
~~~

HTML output lands in `build/docs/doxygen/html/`. LaTeX and man pages are
generated alongside.
