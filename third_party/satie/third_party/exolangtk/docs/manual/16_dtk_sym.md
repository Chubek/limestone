# DebugTk: Symbol Resolution, Unwinding, and Stack Traces {#manual_16_dtk_sym}

Modules: [dtk_platform.h](../include/DebugTk/dtk_platform.h),
[dtk_types.h](../include/DebugTk/dtk_types.h),
[dtk_sym.h](../include/DebugTk/dtk_sym.h),
[dtk_unwind.h](../include/DebugTk/dtk_unwind.h),
[dtk_stack.h](../include/DebugTk/dtk_stack.h) | Stability: experimental

## Overview

DebugTk provides debugging primitives for compiler and interpreter toolchains.
This chapter covers platform shims, symbol resolution, stack unwinding, and
stack-trace capture.

## Platform Shims

`dtk_platform.h` defines `DTK_DEF` (the DebugTk function qualifier macro) and
re-exports target detection macros from InteropTk::platform as `DTK_OS_*` and
`DTK_ARCH_*` aliases.

## Common Types

`dtk_types.h` defines the shared types used across DebugTk:

~~~c
typedef enum dtk_status {
    DTK_OK        = 0,
    DTK_EINVAL    = -1,
    DTK_ENOMEM    = -2,
    DTK_ENOTFOUND = -4,
    DTK_EFAIL     = -6
} dtk_status;
~~~

## Symbol Resolution

`dtk_sym.h` resolves addresses to symbol names using platform-specific
facilities:

~~~c
typedef struct dtk_sym_info {
    const char *module;
    const char *symbol;
    size_t offset;
} dtk_sym_info;

dtk_status dtk_sym_resolve(uintptr_t address, dtk_sym_info *out);
~~~

- `dtk_sym_resolve()` fills `out` with the module path, symbol name, and
  byte offset from the symbol base.
- On platforms with `dladdr()` (POSIX with `_GNU_SOURCE`), this uses the
  dynamic linker's symbol tables.
- On platforms without symbol-resolution support, it returns `DTK_ENOSYS`.

## Stack Unwinding

`dtk_unwind.h` provides a generic unwinding interface:

~~~c
typedef struct dtk_unwind_state dtk_unwind_state;

dtk_status dtk_unwind_init(dtk_unwind_state *u);
dtk_status dtk_unwind_step(dtk_unwind_state *u, uintptr_t *ip);
dtk_status dtk_unwind_stack(dtk_unwind_state *u,
                            uintptr_t *out, size_t cap, size_t *count);
~~~

- `dtk_unwind_init()` initializes unwinding from the current frame.
- `dtk_unwind_step()` advances to the caller frame and returns the
  instruction pointer.
- `dtk_unwind_stack()` captures up to `cap` return addresses.

The unwind implementation uses `__builtin_return_address()` and frame-pointer
walking on supported platforms. On platforms without unwinding support, it
returns `DTK_ENOSYS`.

## Stack Traces

`dtk_stack.h` provides formatted stack traces:

~~~c
typedef struct dtk_stack_frame {
    uintptr_t address;
    dtk_sym_info sym;
} dtk_stack_frame;

dtk_status dtk_stack_capture(dtk_stack_frame *out, size_t cap,
                             size_t *count);
dtk_status dtk_stack_format(const dtk_stack_frame *frames, size_t count,
                            char *buf, size_t cap);
~~~

- `dtk_stack_capture()` combines unwinding and symbol resolution into a
  single call.
- `dtk_stack_format()` renders a stack trace to a human-readable string.

## Usage Example

~~~c
#define DTK_SYM_IMPLEMENTATION
#define DTK_UNWIND_IMPLEMENTATION
#define DTK_STACK_IMPLEMENTATION
#include "DebugTk/dtk_stack.h"

dtk_stack_frame frames[32];
size_t count = 0;
dtk_status st = dtk_stack_capture(frames, 32, &count);

char buf[4096];
dtk_stack_format(frames, count, buf, sizeof(buf));
printf("%s\n", buf);
~~~
