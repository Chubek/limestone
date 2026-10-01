# InteropTk: Symbol Export Macros {#manual_13_export}

Module: [itk_export.h](../include/InteropTk/itk_export.h) | Stability: stable

## Overview

`itk_export.h` provides macros for controlling symbol visibility when
ExolangTk headers are compiled as part of a shared library. It defines
`ITK_EXPORT`, `ITK_IMPORT`, and `ITK_HIDDEN` macros that expand to the
appropriate compiler-specific directives on each platform.

## Export Macros

~~~c
#define ITK_EXPORT  /* platform-specific export directive */
#define ITK_IMPORT  /* platform-specific import directive */
#define ITK_HIDDEN  /* platform-specific hidden-visibility directive */
~~~

On GCC and Clang, these expand to `__attribute__((visibility("default")))`,
`__attribute__((visibility("default")))`, and
`__attribute__((visibility("hidden")))` respectively. On MSVC, they expand to
`__declspec(dllexport)`, `__declspec(dllimport)`, and nothing (MSVC hides by
default).

## Usage Pattern

When building a shared library that embeds ExolangTk, define `ITK_BUILD_DLL`
before including any ExolangTk header:

~~~c
#define ITK_BUILD_DLL
#define ITK_DEF extern
#define ITK_PLATFORM_IMPLEMENTATION
#include "InteropTk/itk_platform.h"
~~~

Consumers of the shared library omit `ITK_BUILD_DLL`:

~~~c
#include "InteropTk/itk_platform.h"  /* symbols are imported */
~~~

## Platform Mapping

| Macro | GCC/Clang | MSVC |
|-------|-----------|------|
| `ITK_EXPORT` | `__attribute__((visibility("default")))` | `__declspec(dllexport)` |
| `ITK_IMPORT` | `__attribute__((visibility("default")))` | `__declspec(dllimport)` |
| `ITK_HIDDEN` | `__attribute__((visibility("hidden")))` | (no-op) |

## Usage Example

~~~c
/* Building a shared library */
#define ITK_BUILD_DLL
#define ITK_DEF extern
#define ITK_PLATFORM_IMPLEMENTATION
#include "InteropTk/itk_platform.h"

/* itk_target_query is now exported */
~~~
