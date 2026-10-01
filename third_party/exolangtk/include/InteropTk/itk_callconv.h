/**
 * @file itk_callconv.h
 * @brief Declarative metadata for common C calling conventions and argument
 *        classifications used by foreign-call implementations.
 *
 * @stability experimental
 * @depends InteropTk::platform, InteropTk::ctypes, InteropTk::layout
 */
#ifndef ITK_CALLCONV_H
#define ITK_CALLCONV_H

#include "itk_platform.h"
#include "itk_ctypes.h"
#include "itk_layout.h"

typedef enum itk_callconv {
    ITK_CALLCONV_DEFAULT = 0,
    ITK_CALLCONV_SYSV64,
    ITK_CALLCONV_WIN64,
    ITK_CALLCONV_CDECL,
    ITK_CALLCONV_STDCALL,
    ITK_CALLCONV_FASTCALL,
    ITK_CALLCONV_AAPCS64,
    ITK_CALLCONV_AAPCS
} itk_callconv;

typedef enum itk_arg_class {
    ITK_ARG_INVALID = 0,
    ITK_ARG_INTEGER,
    ITK_ARG_FLOAT,
    ITK_ARG_MEMORY,
    ITK_ARG_POINTER
} itk_arg_class;

typedef struct itk_arg_info {
    itk_arg_class classification;
    unsigned register_index;
    size_t stack_offset;
} itk_arg_info;

ITK_DEF itk_arg_class itk_classify_return(itk_callconv cc,
                                          const itk_type *type);
ITK_DEF itk_bool itk_classify_args(itk_callconv cc, const itk_type *const *types,
                                   size_t count, itk_arg_info *out);

#ifdef ITK_CALLCONV_IMPLEMENTATION
ITK_DEF itk_arg_class itk_classify_return(itk_callconv cc, const itk_type *type)
{
    (void)cc;
    if (type == NULL || !itk_type_is_complete(type)) return ITK_ARG_INVALID;
    if (itk_type_is_float(type->kind)) return ITK_ARG_FLOAT;
    if (type->kind == ITK_KIND_PTR) return ITK_ARG_POINTER;
    if (itk_type_is_integer(type->kind) || type->kind == ITK_KIND_ENUM)
        return ITK_ARG_INTEGER;
    return ITK_ARG_MEMORY;
}

ITK_DEF itk_bool itk_classify_args(itk_callconv cc, const itk_type *const *types,
                                   size_t count, itk_arg_info *out)
{
    size_t i;
    size_t stack = 0;
    unsigned reg = 0;
    (void)cc;
    if (count != 0 && (types == NULL || out == NULL)) return ITK_FALSE;
    for (i = 0; i < count; ++i) {
        const itk_type *t = types[i];
        size_t size;
        if (t == NULL) return ITK_FALSE;
        out[i].classification = itk_classify_return(cc, t);
        out[i].register_index = reg++;
        size = itk_type_size(t);
        if (size == 0) size = sizeof(uintptr_t);
        out[i].stack_offset = stack;
        stack += (size + sizeof(uintptr_t) - 1u) / sizeof(uintptr_t) *
                 sizeof(uintptr_t);
    }
    return ITK_TRUE;
}
#endif

#endif
