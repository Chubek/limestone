# AGENTS.md — ExolangTk Contributor and AI-Agent Guidelines

This document is the authoritative reference for all contributors and AI agents
working in the ExolangTk repository. Read it fully before touching any file.
When this document conflicts with a code comment or README, this document wins.

Remember to implement `CMakeLists.txt` at the root, and subdirectories.

---

## 1. Repository Overview

ExolangTk is a **C99, header-only toolkit ecosystem** for compiler and
interpreter interoperability. It is organized into four subsystems, each
governed by a YAML manifest under `manifests/`.

| Subsystem      | Manifest file                       | Symbol prefix | Macro prefix |
|----------------|-------------------------------------|---------------|--------------|
| InteropTk      | `manifests/InteropTk-Modules.yaml`  | `itk_`        | `ITK_`       |
| FFItk          | `manifests/FFItk-Modules.yaml`      | `ffi_`        | `FFI_`       |
| DebugTk        | `manifests/DebugTk-Modules.yaml`    | `dtk_`        | `DTK_`       |
| ExtensionTk    | `manifests/ExtensionTk-Modules.yaml`| `etk_`        | `ETK_`       |

---

## 2. Dependency Architecture

Dependencies flow in **one direction only**:

```
FFItk  ──────────────────────────────┐
DebugTk  ──────────────────────────► InteropTk
ExtensionTk  (+ FFItk) ─────────────┘
```
Strict rules:

- **InteropTk** has no intra-project dependencies. It is the base layer.
- **FFItk** may depend on InteropTk only.
- **DebugTk** may depend on InteropTk only.
- **ExtensionTk** may depend on InteropTk and FFItk. It must not depend on
  DebugTk.
- No subsystem may introduce a circular dependency at any granularity.
- Cross-subsystem `depends_on` entries in YAML manifests are the source of
  truth. Never add a `#include` that is not reflected there.

When implementing a module, satisfy its `depends_on` list and nothing else.
Do not reach into a sibling module that is not listed.

---

## 3. Manifest Files Are the Source of Truth

Before writing or editing any header, read the relevant YAML manifest.


manifests/
  InteropTk-Modules.yaml
  FFItk-Modules.yaml
  DebugTk-Modules.yaml
  ExtensionTk-Modules.yaml

Every manifest entry contains:

- `name` — canonical module identifier.
- `header` — the exact path where the header lives.
- `brief` — the public description, usable verbatim as the `@file` Doxygen
  brief.
- `stability` — `stable` | `experimental` | `deprecated`.
- `depends_on` — explicit intra- and cross-subsystem dependencies.
- `provides` — exhaustive lists of types, functions, macros, enums, and
  constants the module exposes.

**Every symbol listed under `provides` must appear in the corresponding
header.** If a symbol is not in the manifest it must not be exported. If
you need a new symbol, add it to the manifest first, then implement it.

---

## 4. Implementation Pattern

### 4.1 Header-only with `*_IMPLEMENTATION` guard

Every module follows this split-compilation pattern:

```c
/* ── public declarations ──────────────────────────────────────────────── */
#ifndef ETK_DYNLOAD_H
#define ETK_DYNLOAD_H

#include <stdint.h>
/* ... other includes ... */

/* type definitions, static inline helpers, and declarations */

#ifdef ETK_DYNLOAD_IMPLEMENTATION
/* ── implementation section ─────────────────────────────────────────── */
/* non-trivial function bodies go here, guarded by the macro */
#endif /* ETK_DYNLOAD_IMPLEMENTATION */

#endif /* ETK_DYNLOAD_H */
```
Rules:

- The implementation guard name must be `<MACRO_PREFIX><MODULE_NAME_UPPER>_IMPLEMENTATION`.
- Only **one** translation unit in a project may define the guard.
- Never place a definition that generates object code outside the
  `*_IMPLEMENTATION` block. Violating this causes ODR violations in
  multi-TU builds.

### 4.2 Function qualifiers

Use the subsystem-specific qualifier macro for every non-trivial function
body. Qualify `static inline` helpers that truly belong in the header
without the guard.

| Subsystem   | Qualifier macro | Expands to (default) |
|-------------|-----------------|----------------------|
| InteropTk   | `ITK_DEF`       | `static`             |
| FFItk       | `FFI_DEF`       | `static`             |
| DebugTk     | `DTK_DEF`       | `static`             |
| ExtensionTk | `ETK_DEF`       | `static`             |

Each subsystem's `platform.h` defines its qualifier macro and allows the
user to override it (e.g. to `extern`) by defining
`<MACRO_PREFIX>DEF` before the first include.

### 4.3 No global mutable state

Every module that needs mutable state must accept it as a caller-supplied
struct pointer. No `static` variables with mutable values, no global
singletons, no thread-local storage that the user cannot control. This
allows multiple independent runtimes to coexist in a single process — a
hard requirement for all four subsystems.

---

## 5. C Standard and Portability

- **Target standard: C99.** No C11, no C17, no compiler extensions.
- Do not use VLAs (variable-length arrays) — they are optional in C11 and
  entirely absent from strict C99 targets.
- Do not use `__attribute__`, `__declspec`, `_Pragma`, or any
  compiler-specific keyword directly in module code. Gate them behind a
  platform macro defined in the subsystem's `platform.h`.
- Do not use `//` comments in `.h` files that may be processed by strict
  C89 preprocessors. Use `/* */`.
- Assume `<stdint.h>` and `<stddef.h>` are available. Do not assume
  `<stdbool.h>` is available; define a local `ETK_BOOL` / `ITK_BOOL` etc.
  alias if needed.
- Do not include `<windows.h>` unconditionally. Gate it behind
  `ETK_OS_WINDOWS` (or the subsystem equivalent) defined in `platform.h`.
- Arithmetic on pointer-sized integers must use `uintptr_t` or `intptr_t`,
  never `long` or `int`.

---

## 6. Naming Conventions

### 6.1 Symbol prefixes

| Kind             | Pattern                          | Example                          |
|------------------|----------------------------------|----------------------------------|
| Public function  | `<prefix>_<module>_<verb>`       | `etk_lib_open`, `itk_layout_size`|
| Type / struct    | `<prefix>_<noun>`                | `ffi_cif`, `dtk_breakpoint`      |
| Enum constant    | `<PREFIX>_<NOUN>`                | `ETK_OK`, `ITK_KIND_STRUCT`      |
| Macro constant   | `<PREFIX>_<NOUN>`                | `FFI_MAX_ARGS`, `DTK_PAGESIZE`   |
| Feature macro    | `<PREFIX>_HAS_<FEATURE>`         | `ETK_HAS_DLOPEN`                 |
| OS detect macro  | `<PREFIX>_OS_<NAME>`             | `ITK_OS_LINUX`                   |
| Arch detect macro| `<PREFIX>_ARCH_<NAME>`           | `DTK_ARCH_AARCH64`               |
| Impl guard       | `<PREFIX><MODULE_UPPER>_IMPLEMENTATION` | `FFI_CIF_IMPLEMENTATION` |
| Header guard     | `<PREFIX_UPPER>_<MODULE_UPPER>_H`| `ETK_DYNLOAD_H`, `ITK_LAYOUT_H`  |

### 6.2 File names

Headers are lowercase with the subsystem prefix:

```
include/InteropTk/itk_platform.h
include/FFItk/ffi_cif.h
include/DebugTk/dtk_unwind.h
include/ExtensionTk/etk_dynload.h
```
No mixed-case filenames.

### 6.3 Internal helpers

Symbols not listed in `provides` must be prefixed with a double underscore
inside the `*_IMPLEMENTATION` block **or** be `static` functions/macros
with a leading `_` that appear only inside the implementation guard. They
must not be visible outside their translation unit.

---

## 7. Documentation Requirements

All documentation is written in **Doxygen Javadoc style** (`/** ... */`).

### 7.1 Module header block

Every header file must open (after the include guard) with a `@file` block
that matches the manifest `brief` verbatim:

```c
/**
 * @file etk_dynload.h
 * @brief Thin, portable wrapper around dlopen/dlsym/dlclose (POSIX) and
 *        LoadLibraryEx/GetProcAddress/FreeLibrary (Windows). Provides a
 *        uniform etk_lib_handle / etk_sym_handle API with structured error
 *        reporting, path-search helpers, and RTLD flag abstraction.
 *
 * @stability experimental
 * @depends ExtensionTk::platform, ExtensionTk::types
 */
```
### 7.2 Every exported symbol must be documented

- Functions: `@brief`, `@param` for every parameter, `@return`, and
  `@note` when there are ownership or threading constraints.
- Types and structs: `@brief` plus a `@var` line for every field.
- Macros: `@brief` plus `@param` for function-like macros.
- Enum constants: at minimum an inline `/**< description */` comment.

Undocumented symbols fail review, regardless of how obvious the purpose
seems.

### 7.3 Thread safety and ownership annotations

Use `@note` to state:
- Who owns heap memory and who frees it.
- Whether a function is safe to call from multiple threads simultaneously.
- Whether a handle becomes invalid after a call.

---

## 8. Error Handling

- Every function that can fail returns an `<prefix>_status` code (e.g.
  `etk_status`, `itk_status`).
- Return values are the primary error channel. Do not `abort()`, `exit()`,
  or call `assert()` in library code.
- `assert()` is permitted **only** in `*_IMPLEMENTATION` blocks to catch
  programmer errors (null handle passed where non-null is required). It
  must be wrapped in a macro that the user can disable with `NDEBUG`.
- Do not use `errno` as a primary error mechanism. Map OS errors to the
  subsystem's status codes. Preserve the raw OS error in a caller-supplied
  diagnostic buffer when the API provides one.
- Status codes are defined in each subsystem's `types.h`. Do not invent
  new codes inline; add them to the manifest and `types.h` first.

---

## 9. Memory Management

- Libraries do **not** own an allocator. All heap allocation visible across
  a module boundary must accept a caller-supplied allocator pair
  (`alloc_fn` / `free_fn`) or an allocator context struct.
- Allocation helpers from `InteropTk::alloc` provide the standard
  allocator interface; use them rather than calling `malloc`/`free`
  directly in cross-boundary code.
- Never `free` a pointer that was not allocated by the paired allocator.
- Structures that own heap memory must document their own
 `*_destroy` or
  `*_fini` function clearly.

---

## 10. Pull Request and Agent Workflow

1.  **Check Manifest:** Read the YAML entry for the module you are about
    to touch. List the symbols it `provides`.
2.  **Verify Dependencies:** List the modules in its `depends_on`. Ensure
    those headers are included and their symbols used.
3.  **Low-level first:** If you need to implement `FFItk::cif`, but
    `InteropTk::layout` is missing, you must implement the `InteropTk`
    module first.
4.  **No shortcuts:** Do not bypass the manifest to "get something working."
    Add the symbol to the manifest first.
5.  **Test implementation:** Along with the header, provide a minimal
    `tests/` snippet showing how the implementation guard is used.
6.  **Refuse out-of-scope work:** If asked to add networking or GUI
    primitives, refuse. ExolangTk is restricted to Interoperability,
    FFI, Debugging, and Extension management.
