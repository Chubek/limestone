# ExolangTk integration guide

This guide explains how to choose modules, place implementation code, and
build a small host runtime around ExolangTk.

## 1. Choose the smallest subsystem

Start with InteropTk unless you only need a standalone utility.

- Use `InteropTk` to describe C values, compute ABI-compatible layouts, parse
  declarations, marshal memory, and bridge strings/errors/allocators.
- Add `FFItk` when the host must load a foreign library or invoke a function.
- Add `DebugTk` for symbol names, stack traces, unwinding, or breakpoints.
- Add `ExtensionTk` for loadable plugins, version constraints, dependency
  activation, and host-provided services.

The dependency graph is strict: FFItk and DebugTk depend only on InteropTk;
ExtensionTk may use InteropTk and FFItk, but never DebugTk.

## 2. Include modules and implementation guards

Umbrella headers are convenient:

```c
#include "InteropTk.h"
#include "FFItk.h"
#include "DebugTk.h"
#include "ExtensionTk.h"
```

For smaller builds, include individual headers. Every non-trivial module has an
implementation macro. Define each macro in one `.c` file only:

```c
/* exolangtk_impl.c */
#define ITK_PLATFORM_IMPLEMENTATION
#define ITK_CTYPES_IMPLEMENTATION
#define ITK_LAYOUT_IMPLEMENTATION
#define FFI_LOADER_IMPLEMENTATION
#define FFI_CIF_IMPLEMENTATION
#define FFI_CALL_IMPLEMENTATION
#include "InteropTk.h"
#include "FFItk.h"
```

Other translation units include the same headers without the macros. Defining
an implementation guard in multiple translation units can produce duplicate
definitions. The complete guard list is maintained in
[`docs/manual/02_build.md`](docs/manual/02_build.md).

By default, `ITK_DEF`, `FFI_DEF`, `DTK_DEF`, and `ETK_DEF` expand to `static`.
Advanced integrations can override a qualifier before inclusion and put the
emitted implementation in a dedicated translation unit:

```c
#define ITK_DEF extern
#define ITK_PLATFORM_IMPLEMENTATION
#include "InteropTk/itk_platform.h"
```

Use the same qualifier policy consistently across all translation units.

## 3. A typical FFI flow

The usual foreign-call sequence is:

1. Describe primitive, pointer, array, or function types with InteropTk.
2. Prepare an `ffi_cif` with the return type, argument types, and calling
   convention.
3. Load a library and resolve a symbol through `ffi_loader`.
4. Pack arguments into an `ffi_frame` or argument slots.
5. Call through `ffi_call`, then decode the return slot.

For repeated calls, `ffi_library.h` combines loading, CIF preparation, and
invocation in an `ffi_binding`. Use `ffi_closure` when foreign code must call
back into a host-language function.

## 4. Memory and ownership

Pass an `itk_allocator` (or a subsystem adapter built on it) whenever a module
needs storage visible to the caller. Treat borrowed string views and handles
according to the ownership notes in each header. Destroy registries, arenas,
closures, code pages, and library handles through their matching API before
the allocator or backing library goes away.

No ExolangTk module owns a process-wide singleton. Keep runtime state in
caller-owned structs so multiple independent interpreters can coexist.

## 5. Errors and portability

Check every fallible return value. InteropTk maps operating-system failures to
`itk_status`; FFItk, DebugTk, and ExtensionTk expose their own status enums.
Preserve diagnostic buffers when an API provides them instead of relying on
`errno` as the application-facing channel.

Platform and architecture detection comes from each subsystem's `platform.h`.
Do not include platform SDK headers directly unless the module documents that
platform path; Windows-specific headers are gated internally.

## 6. Extension host pattern

An ExtensionTk host generally:

1. Opens an extension with `etk_dynload`.
2. Parses and checks its semantic version with `etk_version`.
3. Registers its descriptor and dependencies in an `etk_registry`.
4. Activates the registry, which performs dependency-ordered startup.
5. Publishes stable host capabilities through an `etk_service_table`.
6. Deactivates all extensions in reverse order during shutdown.

Service lookups should be version-constrained so an extension can reject an
incompatible host API before calling through a function pointer.

## 7. Building and testing

As a subdirectory:

```sh
cmake -S . -B build -DEXOLANGTK_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The smoke test exercises all umbrella headers and their implementation guards.
For installation, use [`INSTALL.md`](INSTALL.md). For module-level details,
read the numbered chapters under [`docs/manual`](docs/manual).
