/**
 * @file ffi_cif.h
 * @brief Prepared foreign-call signature descriptors.
 * @stability stable
 * @depends FFItk::types, InteropTk::ctypes, InteropTk::layout,
 *          InteropTk::callconv
 */
#ifndef FFI_CIF_H
#define FFI_CIF_H
#include "ffi_types.h"
#include "../InteropTk/itk_callconv.h"
#define FFI_MAX_ARGS 32
typedef struct ffi_cif {
    itk_callconv callconv;
    const itk_type *return_type;
    const itk_type *args[FFI_MAX_ARGS];
    size_t arg_count;
    itk_bool variadic;
    itk_bool prepared;
} ffi_cif;
FFI_DEF ffi_status ffi_cif_prepare(ffi_cif *cif, itk_callconv cc,
                                   const itk_type *ret,
                                   const itk_type *const *args, size_t count);
FFI_DEF ffi_status ffi_cif_prepare_var(ffi_cif *cif, itk_callconv cc,
                                       const itk_type *ret,
                                       const itk_type *const *args,
                                       size_t fixed_count, size_t total_count);
#ifdef FFI_CIF_IMPLEMENTATION
FFI_DEF ffi_status ffi_cif_prepare(ffi_cif *cif, itk_callconv cc,
                                   const itk_type *ret,
                                   const itk_type *const *args, size_t count)
{
    size_t i;
    if (cif == NULL || ret == NULL || count > FFI_MAX_ARGS ||
        (count != 0 && args == NULL)) return FFI_EINVAL;
    cif->callconv = cc; cif->return_type = ret; cif->arg_count = count;
    cif->variadic = ITK_FALSE;
    for (i = 0; i < count; ++i) {
        if (args[i] == NULL) return FFI_EINVAL;
        cif->args[i] = args[i];
    }
    cif->prepared = ITK_TRUE;
    return FFI_OK;
}
FFI_DEF ffi_status ffi_cif_prepare_var(ffi_cif *cif, itk_callconv cc,
                                       const itk_type *ret,
                                       const itk_type *const *args,
                                       size_t fixed_count, size_t total_count)
{
    ffi_status s;
    if (fixed_count > total_count) return FFI_EINVAL;
    s = ffi_cif_prepare(cif, cc, ret, args, total_count);
    if (s == FFI_OK) cif->variadic = ITK_TRUE;
    return s;
}
#endif
#endif
