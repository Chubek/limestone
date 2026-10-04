/**
 * @file ffi_call.h
 * @brief Foreign invocation entry points.
 * @stability stable
 * @depends FFItk::cif, FFItk::frame, InteropTk::callconv
 */
#ifndef FFI_CALL_H
#define FFI_CALL_H
#include "ffi_frame.h"
FFI_DEF ffi_status ffi_call(const ffi_cif *cif, void *fn,
                            const void *const *args, ffi_value *ret);
FFI_DEF ffi_status ffi_call_var(const ffi_cif *cif, void *fn,
                                const void *const *args, size_t count,
                                ffi_value *ret);
#ifdef FFI_CALL_IMPLEMENTATION
FFI_DEF ffi_status ffi_call(const ffi_cif *cif, void *fn,
                            const void *const *args, ffi_value *ret)
{ (void)cif; (void)fn; (void)args; (void)ret; return FFI_ENOSYS; }
FFI_DEF ffi_status ffi_call_var(const ffi_cif *cif, void *fn,
                                const void *const *args, size_t count,
                                ffi_value *ret)
{ (void)count; return ffi_call(cif, fn, args, ret); }
#endif
#endif
