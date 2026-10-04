/**
 * @file dtk_unwind.h
 * @brief Caller-owned frame records and frame-pointer unwinding hooks.
 * @stability experimental
 * @depends DebugTk::types, InteropTk::alloc
 */
#ifndef DTK_UNWIND_H
#define DTK_UNWIND_H
#include "dtk_types.h"
typedef struct dtk_frame { uintptr_t pc, frame_pointer, stack_pointer; } dtk_frame;
typedef struct dtk_unwinder { dtk_status (*next)(void *, dtk_frame *); void *ctx; } dtk_unwinder;
DTK_DEF dtk_status dtk_unwind_frame(const dtk_unwinder *u, dtk_frame *frame);
DTK_DEF dtk_status dtk_unwind_stack(const dtk_unwinder *u, dtk_frame *out,
                                    size_t cap, size_t *count);
DTK_DEF dtk_status dtk_unwinder_register_fp(dtk_unwinder *u, void *ctx);
#ifdef DTK_UNWIND_IMPLEMENTATION
DTK_DEF dtk_status dtk_unwind_frame(const dtk_unwinder *u, dtk_frame *frame)
{ if (u == NULL || u->next == NULL || frame == NULL) return DTK_EINVAL;
  return u->next(u->ctx, frame); }
DTK_DEF dtk_status dtk_unwind_stack(const dtk_unwinder *u, dtk_frame *out,
                                    size_t cap, size_t *count)
{
    size_t n = 0;
    dtk_status s = DTK_ENOTFOUND;
    if (count != NULL) *count = 0;
    if (u == NULL || out == NULL || cap == 0) return DTK_EINVAL;
    while (n < cap && (s = dtk_unwind_frame(u, &out[n])) == DTK_OK) ++n;
    if (count != NULL) *count = n;
    return n ? DTK_OK : s;
}
DTK_DEF dtk_status dtk_unwinder_register_fp(dtk_unwinder *u, void *ctx)
{ (void)ctx; if (u == NULL) return DTK_EINVAL; u->next = NULL; u->ctx = NULL;
  return DTK_ENOSYS; }
#endif
#endif
