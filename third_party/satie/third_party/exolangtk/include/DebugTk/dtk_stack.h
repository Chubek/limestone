/**
 * @file dtk_stack.h
 * @brief Bounded backtrace capture and formatting.
 * @stability experimental
 * @depends DebugTk::sym, DebugTk::unwind, InteropTk::cstring
 */
#ifndef DTK_STACK_H
#define DTK_STACK_H
#include "dtk_sym.h"
#include "dtk_unwind.h"
DTK_DEF size_t dtk_backtrace(dtk_frame *out, size_t cap);
DTK_DEF dtk_status dtk_backtrace_format(const dtk_frame *frames, size_t count,
                                        char *buf, size_t cap);
#ifdef DTK_STACK_IMPLEMENTATION
#include <stdio.h>
DTK_DEF size_t dtk_backtrace(dtk_frame *out, size_t cap)
{ (void)out; (void)cap; return 0; }
DTK_DEF dtk_status dtk_backtrace_format(const dtk_frame *frames, size_t count,
                                        char *buf, size_t cap)
{ size_t i, used = 0; if (buf == NULL || cap == 0) return DTK_EINVAL;
  for (i = 0; i < count; ++i) { int n = snprintf(buf + used, cap - used,
      "%zu: %p\n", i, (void *)frames[i].pc);
    if (n < 0 || (size_t)n >= cap - used) { buf[cap - 1] = '\0'; return DTK_EFAIL; }
    used += (size_t)n; } return DTK_OK; }
#endif
#endif
