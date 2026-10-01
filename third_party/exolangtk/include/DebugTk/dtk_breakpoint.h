/**
 * @file dtk_breakpoint.h
 * @brief Caller-mediated software breakpoint table.
 * @stability experimental
 * @depends DebugTk::platform, DebugTk::types, InteropTk::alloc
 */
#ifndef DTK_BREAKPOINT_H
#define DTK_BREAKPOINT_H
#include "dtk_types.h"
#define DTK_BREAKPOINT_MAX_TRAPS 128
typedef struct dtk_breakpoint { uintptr_t address; unsigned char original[8];
                                size_t size; dtk_bool active; } dtk_breakpoint;
typedef struct dtk_breakpoint_table { dtk_breakpoint traps[DTK_BREAKPOINT_MAX_TRAPS];
                                      size_t count; } dtk_breakpoint_table;
DTK_DEF dtk_status dtk_bp_insert(dtk_breakpoint_table *table, uintptr_t address);
DTK_DEF dtk_status dtk_bp_remove(dtk_breakpoint_table *table, uintptr_t address);
DTK_DEF dtk_breakpoint *dtk_bp_find(dtk_breakpoint_table *table, uintptr_t address);
DTK_DEF uintptr_t dtk_bp_hit_address(uintptr_t pc);
#ifdef DTK_BREAKPOINT_IMPLEMENTATION
DTK_DEF dtk_status dtk_bp_insert(dtk_breakpoint_table *table, uintptr_t address)
{
    dtk_breakpoint *bp;
    if (table == NULL || address == 0 ||
        table->count >= DTK_BREAKPOINT_MAX_TRAPS) return DTK_EINVAL;
    if (dtk_bp_find(table, address) != NULL) return DTK_EFAIL;
    bp = &table->traps[table->count++];
    bp->address = address;
    bp->active = DTK_TRUE;
    bp->size = 1;
    return DTK_OK;
}
DTK_DEF dtk_status dtk_bp_remove(dtk_breakpoint_table *table, uintptr_t address)
{
    size_t i;
    if (table == NULL) return DTK_EINVAL;
    for (i = 0; i < table->count; ++i) {
        if (table->traps[i].address == address) {
            table->traps[i] = table->traps[--table->count];
            return DTK_OK;
        }
    }
    return DTK_ENOTFOUND;
}
DTK_DEF dtk_breakpoint *dtk_bp_find(dtk_breakpoint_table *table, uintptr_t address)
{
    size_t i;
    if (table == NULL) return NULL;
    for (i = 0; i < table->count; ++i) {
        if (table->traps[i].address == address) return &table->traps[i];
    }
    return NULL;
}
DTK_DEF uintptr_t dtk_bp_hit_address(uintptr_t pc) { return pc ? pc - 1u : 0; }
#endif
#endif
