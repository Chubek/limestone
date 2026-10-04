/**
 * @file itk_alloc.h
 * @brief Allocator bridging: a small vtable (alloc/realloc/free with context
 *        pointer) so C code can allocate through the host runtime's allocator
 *        or GC-pinned heap, plus an arena built on top for transient interop
 *        buffers.
 *
 * @stability stable
 * @depends InteropTk::platform
 */

#ifndef ITK_ALLOC_H
#define ITK_ALLOC_H

#include "itk_platform.h"

/* ── public declarations ──────────────────────────────────────────────── */

/**
 * @brief Allocation callback. Receives the caller context, the byte count,
 *        and the required alignment (always a power of two).
 * @param ctx    Opaque context supplied in #itk_allocator::ctx.
 * @param size   Bytes to allocate; zero is an error (return NULL).
 * @param align  Required alignment in bytes, at least sizeof(void *).
 * @return Pointer suitably aligned, or NULL on failure.
 * @return NULL when the request cannot be satisfied.
 */
typedef void *(*itk_alloc_fn)(void *ctx, size_t size, size_t align);

/**
 * @brief Resizing callback. Semantics follow realloc(3): the block may move,
 *        contents up to min(old, new) are preserved, and returning NULL
 *        signals failure leaving @p ptr untouched (NULL never frees).
 * @param ctx       Opaque context.
 * @param ptr       Block previously returned by #itk_alloc_fn (may be NULL,
 *                  in which case this acts as an allocation).
 * @param old_size  Current size of @p ptr in bytes.
 * @param new_size  Requested new size in bytes.
 * @param align     Alignment the block was allocated with.
 * @return Resized block, or NULL on failure (original untouched).
 */
typedef void *(*itk_realloc_fn)(void *ctx, void *ptr, size_t old_size,
                                size_t new_size, size_t align);

/**
 * @brief Deallocation callback. Receives NULL silently (no-op).
 * @param ctx  Opaque context.
 * @param ptr  Block to release; NULL is accepted and ignored.
 */
typedef void (*itk_free_fn)(void *ctx, void *ptr);

/**
 * @brief Caller-supplied allocator vtable with context.
 *
 * Every InteropTk module that needs heap memory takes one of these; none
 * hold a default. When any callback is NULL the call helpers below fail
 * gracefully.
 *
 * @var itk_allocator::ctx
 *      Opaque pointer passed verbatim to every callback.
 * @var itk_allocator::alloc
 *      Allocation callback (may be NULL to reject allocation).
 * @var itk_allocator::realloc
 *      Resize callback (may be NULL to reject resizing).
 * @var itk_allocator::free_fn
 *      Deallocation callback (may be NULL to reject freeing).
 */
typedef struct itk_allocator {
    void *ctx;             /**< Callback context. */
    itk_alloc_fn alloc;    /**< Allocate callback. */
    itk_realloc_fn realloc;/**< Resize callback. */
    itk_free_fn free_fn;   /**< Deallocate callback. */
} itk_allocator;

/**
 * @brief Return the built-in libc adapter.
 * @return Pointer to an immutable allocator backed by malloc/free.
 * @note The returned pointer addresses static immutable storage; do not
 *       write through it. Safe to share across threads.
 */
ITK_DEF const itk_allocator *itk_libc_allocator(void);

/**
 * @brief Allocate @p size bytes at @p align through @p a.
 * @param a      Allocator vtable; NULL selects the libc adapter.
 * @param size   Bytes to allocate (zero fails).
 * @param align  Power-of-two alignment; 0 is treated as alignof(void*).
 * @return Aligned block, or NULL on failure or a NULL callback.
 * @note The caller owns the returned block and releases it with
 *       itk_allocator_free().
 */
ITK_DEF void *itk_allocator_alloc(const itk_allocator *a, size_t size, size_t align);

/**
 * @brief Allocate and zero-initialize @p size bytes at @p align.
 * @param a      Allocator vtable; NULL selects the libc adapter.
 * @param size   Bytes to allocate (zero fails).
 * @param align  Power-of-two alignment; 0 is treated as alignof(void*).
 * @return Zeroed aligned block, or NULL on failure.
 * @note Caller owns the block; same ownership as itk_allocator_alloc().
 */
ITK_DEF void *itk_allocator_zalloc(const itk_allocator *a, size_t size, size_t align);

/**
 * @brief Resize a block previously obtained from @p a.
 * @param a         Allocator vtable; NULL selects the libc adapter.
 * @param ptr       Existing block (NULL behaves as allocation).
 * @param old_size  Current size in bytes.
 * @param new_size  Requested size in bytes.
 * @param align     Power-of-two alignment originally requested.
 * @return Resized block or NULL on failure (original block untouched).
 */
ITK_DEF void *itk_allocator_realloc(const itk_allocator *a, void *ptr,
                            size_t old_size, size_t new_size, size_t align);

/**
 * @brief Release a block obtained from @p a.
 * @param a    Allocator vtable; NULL selects the libc adapter.
 * @param ptr  Block to release; NULL is a no-op.
 * @note Never pass a pointer from a different allocator.
 */
ITK_DEF void itk_allocator_free(const itk_allocator *a, void *ptr);

/** @brief Default data capacity of each arena block (bytes). */
#define ITK_ARENA_BLOCK_SIZE ((size_t)4096)

/**
 * @brief Arena block header. Blocks form a singly linked list; data follows
 *        the header at the next suitable alignment.
 *
 * @var itk_arena_block::next
 *      Next block in the chain (most recent first) or NULL.
 * @var itk_arena_block::used
 *      Bytes of the data area already handed out.
 * @var itk_arena_block::cap
 *      Total bytes of the data area.
 */
typedef struct itk_arena_block {
    struct itk_arena_block *next; /**< Chain link. */
    size_t used;                  /**< Bytes handed out. */
    size_t cap;                   /**< Data-area capacity. */
} itk_arena_block;

/**
 * @brief Bump allocator for transient interop buffers.
 *
 * All state lives in the caller-supplied struct; distinct instances are
 * fully independent and may be used from different threads.
 *
 * @var itk_arena::alloc
 *      Backing allocator copied at init time.
 * @var itk_arena::head
 *      Most recently allocated block, or NULL.
 * @var itk_arena::total
 *      Cumulative bytes requested across all blocks (diagnostic).
 */
typedef struct itk_arena {
    itk_allocator alloc;   /**< Backing allocator. */
    itk_arena_block *head; /**< Newest block. */
    size_t total;          /**< Cumulative bytes handed out. */
} itk_arena;

/**
 * @brief Prepare @p ar for use with backing allocator @p a.
 * @param ar  Arena to initialize; must not be NULL.
 * @param a   Allocator copied by value; NULL selects the libc adapter.
 * @return ITK_TRUE on success, ITK_FALSE if @p ar is NULL.
 * @note No allocation happens until the first push.
 */
ITK_DEF itk_bool itk_arena_init(itk_arena *ar, const itk_allocator *a);

/**
 * @brief Reserve @p size bytes at @p align from the arena.
 * @param ar     Arena; must not be NULL.
 * @param size   Bytes to reserve (zero fails).
 * @param align  Power-of-two alignment; 0 is treated as alignof(void*).
 * @return Pointer into an arena block, or NULL on allocation failure.
 * @note Valid until itk_arena_reset() or itk_arena_destroy(); never freed
 *       individually. Not for use with the arena's backing free callback.
 */
ITK_DEF void *itk_arena_push(itk_arena *ar, size_t size, size_t align);

/**
 * @brief Return every block's space for reuse, keeping one block resident.
 * @param ar  Arena; must not be NULL.
 * @note Existing pointers become invalid: they may overlap future pushes.
 */
ITK_DEF void itk_arena_reset(itk_arena *ar);

/**
 * @brief Release every block, including the resident one.
 * @param ar  Arena previously initialized with itk_arena_init().
 * @note The arena is left zeroed and must be re-initialized before reuse.
 */
ITK_DEF void itk_arena_destroy(itk_arena *ar);

#ifdef ITK_ALLOC_IMPLEMENTATION
/* ── implementation section ─────────────────────────────────────────── */

#include <stdlib.h>
#include <string.h>

#if defined(ITK_OS_WINDOWS)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

static void itk_libc_free_(void *ctx, void *ptr);

/** Libc allocation with over-aligned requests routed to the OS helpers. */
static void *itk_libc_alloc_(void *ctx, size_t size, size_t align)
{
    (void)ctx;
    if (size == 0 || (align & (align - 1)) != 0) {
        return NULL;
    }
    if (align <= (size_t)(2 * sizeof(void *))) {
        return malloc(size);
    }
#if defined(ITK_OS_WINDOWS)
    return _aligned_malloc(size, align);
#else
    return NULL; /* No portable over-aligned allocation on this target. */
#endif
}

static void *itk_libc_realloc_(void *ctx, void *ptr, size_t old_size,
                               size_t new_size, size_t align)
{
    (void)ctx;
    (void)old_size;
    if (new_size == 0 || (align & (align - 1)) != 0) {
        return NULL;
    }
    if (align <= (size_t)(2 * sizeof(void *))) {
        return realloc(ptr, new_size);
    }
    /* No portable aligned realloc: allocate-copy-free. */
    {
        void *p = itk_libc_alloc_(NULL, new_size, align);
        if (p == NULL) {
            return NULL;
        }
        if (ptr != NULL) {
            const size_t n = (old_size < new_size) ? old_size : new_size;
            memcpy(p, ptr, n);
            itk_libc_free_(NULL, ptr);
        }
        return p;
    }
}

static void itk_libc_free_(void *ctx, void *ptr)
{
    (void)ctx;
    if (ptr == NULL) {
        return;
    }
#if defined(ITK_OS_WINDOWS)
    _aligned_free(ptr); /* Also releases plain malloc blocks. */
#else
    free(ptr);
#endif
}

ITK_DEF const itk_allocator *itk_libc_allocator(void)
{
    static const itk_allocator libc = { NULL, itk_libc_alloc_,
                                        itk_libc_realloc_, itk_libc_free_ };
    return &libc;
}

/** Resolve NULL allocator or NULL callbacks to the libc adapter entry. */
static itk_alloc_fn itk_resolve_alloc_(const itk_allocator *a)
{
    if (a != NULL && a->alloc != NULL) {
        return a->alloc;
    }
    return itk_libc_allocator()->alloc;
}

static itk_free_fn itk_resolve_free_(const itk_allocator *a)
{
    if (a != NULL && a->free_fn != NULL) {
        return a->free_fn;
    }
    return itk_libc_allocator()->free_fn;
}

static void *itk_ctx_of_(const itk_allocator *a)
{
    return (a != NULL) ? a->ctx : NULL;
}

static size_t itk_norm_align_(size_t align)
{
    return (align < (size_t)(2 * sizeof(void *)))
               ? (size_t)(2 * sizeof(void *))
               : align;
}

ITK_DEF void *itk_allocator_alloc(const itk_allocator *a, size_t size,
                                  size_t align)
{
    itk_alloc_fn fn = itk_resolve_alloc_(a);
    if (size == 0) {
        return NULL;
    }
    return fn(itk_ctx_of_(a), size, itk_norm_align_(align));
}

ITK_DEF void *itk_allocator_zalloc(const itk_allocator *a, size_t size,
                                   size_t align)
{
    void *p = itk_allocator_alloc(a, size, align);
    if (p != NULL) {
        memset(p, 0, size);
    }
    return p;
}

ITK_DEF void *itk_allocator_realloc(const itk_allocator *a, void *ptr,
                                    size_t old_size, size_t new_size,
                                    size_t align)
{
    itk_realloc_fn fn;
    if (new_size == 0) {
        return NULL;
    }
    if (a != NULL && a->realloc != NULL) {
        fn = a->realloc;
    } else {
        fn = itk_libc_allocator()->realloc;
    }
    return fn(itk_ctx_of_(a), ptr, old_size, new_size,
              itk_norm_align_(align));
}

ITK_DEF void itk_allocator_free(const itk_allocator *a, void *ptr)
{
    itk_resolve_free_(a)(itk_ctx_of_(a), ptr);
}

ITK_DEF itk_bool itk_arena_init(itk_arena *ar, const itk_allocator *a)
{
    if (ar == NULL) {
        return ITK_FALSE;
    }
    ar->alloc = (a != NULL) ? *a : *itk_libc_allocator();
    ar->head = NULL;
    ar->total = (size_t)0;
    return ITK_TRUE;
}

/** Allocate and prepend a block whose data area can satisfy @p need bytes
 *  at @p align. Returns the new head, or NULL on failure. */
static itk_arena_block *itk_arena_new_block_(itk_arena *ar, size_t need,
                                             size_t align)
{
    const size_t cap = (need > ITK_ARENA_BLOCK_SIZE) ? need
                                                     : ITK_ARENA_BLOCK_SIZE;
    const size_t over = align - 1;
    itk_arena_block *blk;
    size_t size;

    /* header + worst-case alignment skew + data */
    if (cap > (size_t)-1 - sizeof(itk_arena_block) - over) {
        return NULL;
    }
    size = sizeof(itk_arena_block) + over + cap;
    blk = (itk_arena_block *)itk_allocator_alloc(&ar->alloc, size,
                                                 (size_t)2 * sizeof(void *));
    if (blk == NULL) {
        return NULL;
    }
    blk->next = ar->head;
    blk->used = (size_t)0;
    /* The data area (right after the header) spans `over + cap` bytes, so
     * pushes re-align downward within that slack. */
    blk->cap = cap + over;
    return blk;
}

ITK_DEF void *itk_arena_push(itk_arena *ar, size_t size, size_t align)
{
    itk_arena_block *blk;
    uintptr_t data, cursor, aligned;

    if (ar == NULL || size == 0) {
        return NULL;
    }
    align = itk_norm_align_(align);
    if ((align & (align - 1)) != 0) {
        return NULL;
    }

    blk = ar->head;
    if (blk == NULL ||
        (blk->cap - blk->used) < size + align) { /* conservative fit test */
        blk = itk_arena_new_block_(ar, size, align);
        if (blk == NULL) {
            return NULL;
        }
        ar->head = blk;
    }

    data = (uintptr_t)blk + (uintptr_t)sizeof(itk_arena_block);
    cursor = data + (uintptr_t)blk->used;
    aligned = (cursor + (uintptr_t)(align - 1)) &
              ~((uintptr_t)(align - 1));
    blk->used = (size_t)(aligned - data) + size;
    ar->total += size;
    return (void *)aligned;
}

ITK_DEF void itk_arena_reset(itk_arena *ar)
{
    itk_arena_block *keep, *cur, *next;

    if (ar == NULL || ar->head == NULL) {
        return;
    }
    keep = ar->head;
    cur = keep->next;
    while (cur != NULL) {
        next = cur->next;
        itk_allocator_free(&ar->alloc, cur);
        cur = next;
    }
    keep->next = NULL;
    keep->used = (size_t)0;
    ar->total = (size_t)0;
}

ITK_DEF void itk_arena_destroy(itk_arena *ar)
{
    itk_arena_block *cur, *next;

    if (ar == NULL) {
        return;
    }
    cur = ar->head;
    while (cur != NULL) {
        next = cur->next;
        itk_allocator_free(&ar->alloc, cur);
        cur = next;
    }
    ar->head = NULL;
    ar->total = (size_t)0;
}

#endif /* ITK_ALLOC_IMPLEMENTATION */

#endif /* ITK_ALLOC_H */
