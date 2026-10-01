/**
 * @file dtk_sym.h
 * @brief Address-to-symbol records and current-process resolution.
 * @stability stable
 * @depends DebugTk::types, InteropTk::cstring, InteropTk::error
 */
#ifndef DTK_SYM_H
#define DTK_SYM_H
#include "dtk_types.h"
#include "../InteropTk/itk_cstring.h"
typedef struct dtk_sym_info {
    const char *module;
    const char *symbol;
    uintptr_t address;
    uintptr_t offset;
} dtk_sym_info;
typedef struct dtk_module_info { const char *path; uintptr_t begin; uintptr_t end; } dtk_module_info;
DTK_DEF dtk_status dtk_sym_resolve(uintptr_t address, dtk_sym_info *out);
DTK_DEF dtk_status dtk_modules_enumerate(dtk_module_info *out, size_t cap,
                                         size_t *count);
DTK_DEF const char *dtk_sym_mangled(const dtk_sym_info *info);
DTK_DEF const char *dtk_sym_demangled(const dtk_sym_info *info);
#ifdef DTK_SYM_IMPLEMENTATION
DTK_DEF dtk_status dtk_sym_resolve(uintptr_t address, dtk_sym_info *out)
{
    if (out == NULL) return DTK_EINVAL;
    out->module = NULL;
    out->symbol = NULL;
    out->address = address;
    out->offset = 0;
    return DTK_ENOSYS;
}
DTK_DEF dtk_status dtk_modules_enumerate(dtk_module_info *out, size_t cap,
                                         size_t *count)
{ (void)out; (void)cap; if (count) *count = 0; return DTK_ENOSYS; }
DTK_DEF const char *dtk_sym_mangled(const dtk_sym_info *info)
{ return info ? info->symbol : NULL; }
DTK_DEF const char *dtk_sym_demangled(const dtk_sym_info *info)
{ return info ? info->symbol : NULL; }
#endif
#endif
