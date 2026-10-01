# DebugTk: Breakpoints {#manual_17_dtk_bp}

Module: [dtk_breakpoint.h](../include/DebugTk/dtk_breakpoint.h) | Stability: experimental

## Overview

`dtk_breakpoint.h` provides a software-breakpoint API that works through
caller-supplied read, write, and commit callbacks. The same API can drive
self-debugging (where the callbacks touch the process's own memory) or
ptrace-based tracers (where callbacks operate on a remote process).

## Breakpoint Table

~~~c
#define DTK_BREAKPOINT_MAX_TRAPS 256

typedef struct dtk_breakpoint {
    uintptr_t address;
    unsigned char original_byte;
    dtk_bool active;
} dtk_breakpoint;

typedef struct dtk_bp_table {
    dtk_breakpoint traps[DTK_BREAKPOINT_MAX_TRAPS];
    size_t count;
    /* caller-supplied callbacks */
    dtk_bool (*read)(void *ctx, uintptr_t addr, unsigned char *out);
    dtk_bool (*write)(void *ctx, uintptr_t addr, unsigned char byte);
    dtk_bool (*commit)(void *ctx);
    void *ctx;
} dtk_bp_table;
~~~

The table tracks up to `DTK_BREAKPOINT_MAX_TRAPS` breakpoints. The three
callbacks give the caller control over how memory is accessed:

- `read` — reads a single byte from an address.
- `write` — writes a single byte to an address.
- `commit` — flushes instruction caches or performs any post-write
  synchronization.

## API

~~~c
dtk_status dtk_bp_insert(dtk_bp_table *table, uintptr_t address);
dtk_status dtk_bp_remove(dtk_bp_table *table, uintptr_t address);
dtk_breakpoint *dtk_bp_find(dtk_bp_table *table, uintptr_t address);
dtk_breakpoint *dtk_bp_hit_address(dtk_bp_table *table, uintptr_t address);
~~~

- `dtk_bp_insert()` reads the original byte at `address`, writes the trap
  instruction (INT3 on x86, BRK on AArch64), and records the entry.
- `dtk_bp_remove()` restores the original byte and removes the entry.
- `dtk_bp_find()` looks up a breakpoint by address.
- `dtk_bp_hit_address()` adjusts the address for the trap-instruction offset
  and returns the matching breakpoint.

## Trap-Instruction Adjustment

Different architectures place the trap at different offsets:

- x86/x86-64: `INT3` is 1 byte; the reported IP points to the byte after the
  trap, so `hit_address` subtracts 1.
- AArch64: `BRK #0` is 4 bytes; `hit_address` subtracts 4.

## Callback Examples

**Self-debugging (same process):**

~~~c
static dtk_bool self_read(void *ctx, uintptr_t addr, unsigned char *out) {
    (void)ctx;
    *out = *(volatile unsigned char *)addr;
    return DTK_TRUE;
}
~~~

**Remote process (via ptrace or similar):**

~~~c
static dtk_bool remote_read(void *ctx, uintptr_t addr, unsigned char *out) {
    /* use ptrace(PTRACE_PEEKDATA, pid, addr, NULL) on Linux */
    (void)ctx; (void)addr; (void)out;
    return DTK_FALSE;  /* stub */
}
~~~

## Usage Example

~~~c
#define DTK_BREAKPOINT_IMPLEMENTATION
#include "DebugTk/dtk_breakpoint.h"

dtk_bp_table table = {0};
table.read = self_read;
table.write = self_write;
table.commit = self_commit;

dtk_bp_insert(&table, (uintptr_t)&some_function);
dtk_bp_remove(&table, (uintptr_t)&some_function);
~~~
