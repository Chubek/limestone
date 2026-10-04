/**
 * @file ffi_closure.h
 * @brief Opaque callback binding records for host runtimes.
 * @stability experimental
 * @depends FFItk::cif, FFItk::frame, FFItk::trampoline
 */
#ifndef FFI_CLOSURE_H
#define FFI_CLOSURE_H
#include "ffi_trampoline.h"
typedef ffi_status (*ffi_closure_handler)(void *ctx, const ffi_value *args,
                                          size_t count, ffi_value *ret);
typedef struct ffi_closure {
    ffi_cif cif;
    ffi_closure_handler handler;
    void *ctx;
    ffi_code code;
} ffi_closure;
FFI_DEF ffi_status ffi_closure_alloc(ffi_closure *closure,
                                     const ffi_cif *cif);
FFI_DEF ffi_status ffi_closure_bind(ffi_closure *closure,
                                    ffi_closure_handler handler, void *ctx);
FFI_DEF void *ffi_closure_code(const ffi_closure *closure);
#ifdef FFI_CLOSURE_IMPLEMENTATION
FFI_DEF ffi_status ffi_closure_alloc(ffi_closure *closure, const ffi_cif *cif)
{ if (closure == NULL || cif == NULL) return FFI_EINVAL; closure->cif = *cif;
  closure->handler = NULL; closure->ctx = NULL; return FFI_OK; }
FFI_DEF ffi_status ffi_closure_bind(ffi_closure *closure,
                                    ffi_closure_handler handler, void *ctx)
{ if (closure == NULL || handler == NULL) return FFI_EINVAL;
  closure->handler = handler; closure->ctx = ctx; return FFI_OK; }
FFI_DEF void *ffi_closure_code(const ffi_closure *closure)
{ return closure == NULL ? NULL : closure->code.address; }
#endif
#endif
