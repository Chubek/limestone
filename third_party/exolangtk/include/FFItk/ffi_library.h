/**
 * @file ffi_library.h
 * @brief Convenience binding records combining loader and call-interface data.
 * @stability stable
 * @depends FFItk::loader, FFItk::cif, FFItk::call, InteropTk::cdecl
 */
#ifndef FFI_LIBRARY_H
#define FFI_LIBRARY_H
#include "ffi_call.h"
#include "ffi_loader.h"
typedef struct ffi_binding {
    ffi_library library;
    void *symbol;
    ffi_cif cif;
} ffi_binding;
FFI_DEF ffi_status ffi_bind(ffi_binding *binding, const char *path,
                            const char *symbol, const ffi_cif *cif);
FFI_DEF ffi_status ffi_invoke(const ffi_binding *binding,
                              const void *const *args, ffi_value *ret);
#ifdef FFI_LIBRARY_IMPLEMENTATION
FFI_DEF ffi_status ffi_bind(ffi_binding *binding, const char *path,
                            const char *symbol, const ffi_cif *cif)
{ ffi_status s;
  if (binding == NULL || path == NULL || symbol == NULL || cif == NULL)
    return FFI_EINVAL;
  s = ffi_open(path, 0, &binding->library); if (s != FFI_OK) return s;
  binding->symbol = ffi_symbol(&binding->library, symbol);
  if (binding->symbol == NULL) { (void)ffi_close(&binding->library);
    return FFI_ENOTFOUND; }
  binding->cif = *cif; return FFI_OK; }
FFI_DEF ffi_status ffi_invoke(const ffi_binding *binding,
                              const void *const *args, ffi_value *ret)
{ if (binding == NULL) return FFI_EINVAL;
  return ffi_call(&binding->cif, binding->symbol, args, ret); }
#endif
#endif
