# Introduction to ExolangTk {#manual_01_introduction}

## What Is ExolangTk?

ExolangTk is a C99, header-only toolkit ecosystem designed for authors of
compilers, interpreters, debuggers, and language runtimes. It provides the
plumbing that every such project needs: platform introspection, a canonical C
type model, record-layout computation, symbol mangling, value marshalling,
foreign-function invocation, stack unwinding, breakpoint management, dynamic
library loading, version-constraint checking, and extension-registry
lifecycle management.

The project is split into four subsystems, each with its own namespace prefix
and manifest:

| Subsystem | Prefix | Role |
|-----------|--------|------|
| InteropTk | `itk_` | Foundation: types, layout, mangling, marshalling, strings, errors, allocators |
| FFItk | `ffi_` | Foreign function interface: loader, call interface, trampolines, closures |
| DebugTk | `dtk_` | Debugging: symbol resolution, stack unwinding, breakpoints |
| ExtensionTk | `etk_` | Extension management: dynamic loading, versioning, registry, service tables |

## Design Philosophy

**Header-only.** Every module is a single `.h` file. Non-trivial function
bodies live inside an `*_IMPLEMENTATION` guard so exactly one translation unit
emits object code. No build system is required beyond a C99 compiler and an
include path.

**No global state.** Every module that needs mutable state accepts it through
a caller-supplied struct pointer. Multiple independent runtimes can coexist in
a single process.

**Caller-owned allocators.** Libraries never call `malloc` or `free`
directly. Every heap allocation visible across a module boundary goes through
a caller-supplied allocator vtable, making it straightforward to integrate
with garbage-collected or arena-managed hosts.

**C99, no extensions.** The code targets ISO C99 with no VLAs, no
`__attribute__`, and no compiler-specific keywords. Portability macros are
defined in each subsystem's `platform.h`.

## Dependency Architecture

Dependencies flow strictly in one direction:

~~~
FFItk  ──────────────────────────────┐
DebugTk  ──────────────────────────► InteropTk
ExtensionTk  (+ FFItk) ─────────────┘
~~~

- **InteropTk** is the base layer with no intra-project dependencies.
- **FFItk** depends on InteropTk only.
- **DebugTk** depends on InteropTk only.
- **ExtensionTk** depends on InteropTk and FFItk. It must not depend on
  DebugTk.

No circular dependencies exist at any granularity.

## Manifest Files

Every module is governed by a YAML manifest under `manifests/`. The manifest
is the source of truth for:

- The module's canonical name and header path.
- Its stability level (`stable`, `experimental`, or `deprecated`).
- Its explicit dependency list.
- The exhaustive list of symbols it provides.

Before writing or editing any header, consult the manifest. Every symbol
listed under `provides` must appear in the header. If a symbol is not in the
manifest, it must not be exported.

## Implementation Pattern

Every module follows this structure:

~~~c
#ifndef ITK_MODULE_H
#define ITK_MODULE_H

/* public declarations: types, static inline helpers, function prototypes */

#ifdef ITK_MODULE_IMPLEMENTATION
/* non-trivial function bodies */
#endif /* ITK_MODULE_IMPLEMENTATION */

#endif /* ITK_MODULE_H */
~~~

Function qualifier macros (`ITK_DEF`, `FFI_DEF`, `DTK_DEF`, `ETK_DEF`)
expand to `static` by default. Users can override them to `extern` for
dedicated implementation translation units.

## Error Handling

Every function that can fail returns a status code. No library function calls
`abort()`, `exit()`, or `assert()` in library code. OS errors are mapped to
subsystem status codes; raw OS codes are preserved in caller-supplied
diagnostic buffers when the API provides one.

## Documentation Conventions

All public symbols are documented in Doxygen Javadoc style (`/** ... */`).
Every function carries `@brief`, `@param` for every parameter, and `@return`.
Thread-safety and ownership constraints are noted with `@note`.

## Where to Go Next

Start with \subpage manual_02_build to learn how to integrate ExolangTk into
your project. Then read the InteropTk chapters (3–13) for the foundation
layer, followed by the subsystem-specific chapters for FFItk, DebugTk, and
ExtensionTk.

## License

ExolangTk is distributed under the MIT License. See the repository root for
the full license text.
