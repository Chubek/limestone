# InteropTk: Allocator Bridging {#manual_12_alloc}

Module: [itk_alloc.h](../include/InteropTk/itk_alloc.h) | Stability: stable

## Overview

`itk_alloc.h` provides the allocator abstraction used by every InteropTk
module that needs heap memory. It defines a vtable with context pointer so C
code can allocate through the host runtime's allocator or GC-pinned heap. It
also includes a simple arena allocator for transient interop buffers.

## Allocator Vtable

~~~c
typedef void *(*itk_alloc_fn)(void *ctx, size_t size, size_t align);
typedef void *(*itk_realloc_fn)(void *ctx, void *ptr, size_t old_size,
                                size_t new_size, size_t align);
typedef void (*itk_free_fn)(void *ctx, void *ptr);

typedef struct itk_allocator {
    void *ctx;
    itk_alloc_fn alloc;
    itk_realloc_fn realloc;
    itk_free_fn free_fn;
} itk_allocator;
~~~

Every callback receives the opaque `ctx` pointer. When any callback is NULL,
the corresponding call helper fails gracefully.

## Allocation Helpers

~~~c
const itk_allocator *itk_libc_allocator(void);
void *itk_allocator_alloc(const itk_allocator *a, size_t size, size_t align);
void *itk_allocator_zalloc(const itk_allocator *a, size_t size, size_t align);
void *itk_allocator_realloc(const itk_allocator *a, void *ptr,
                            size_t old_size, size_t new_size, size_t align);
void itk_allocator_free(const itk_allocator *a, void *ptr);
~~~

- `itk_libc_allocator()` returns a pointer to an immutable allocator backed
  by `malloc`/`free`. The pointer addresses static storage.
- `itk_allocator_alloc()` allocates aligned memory. `align` of 0 is treated
  as `alignof(void*)`.
- `itk_allocator_zalloc()` allocates and zero-initializes.
- `itk_allocator_realloc()` resizes a block. Semantics follow `realloc(3)`.
- `itk_allocator_free()` deallocates. NULL is silently accepted.

## Arena Allocator

~~~c
#define ITK_ARENA_BLOCK_SIZE ((size_t)4096)

typedef struct itk_arena {
    /* internal state */
} itk_arena;

itk_bool itk_arena_init(itk_arena *ar, const itk_allocator *a);
void *itk_arena_push(itk_arena *ar, size_t size, size_t align);
void itk_arena_reset(itk_arena *ar);
void itk_arena_destroy(itk_arena *ar);
~~~

The arena allocates memory in blocks of `ITK_ARENA_BLOCK_SIZE` bytes. It
never frees individual allocations; instead, `itk_arena_reset()` returns all
space for reuse, and `itk_arena_destroy()` releases all blocks.

## Usage Example

~~~c
#define ITK_PLATFORM_IMPLEMENTATION
#define ITK_ALLOC_IMPLEMENTATION
#include "InteropTk/itk_alloc.h"

const itk_allocator *a = itk_libc_allocator();
void *p = itk_allocator_alloc(a, 1024, 0);
itk_allocator_free(a, p);

itk_arena ar;
itk_arena_init(&ar, a);
void *buf = itk_arena_push(&ar, 256, 8);
itk_arena_reset(&ar);  /* buf is now invalid */
itk_arena_destroy(&ar);
~~~
