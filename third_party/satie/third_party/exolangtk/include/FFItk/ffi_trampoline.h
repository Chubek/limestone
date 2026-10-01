/**
 * @file ffi_trampoline.h
 * @brief Caller-owned executable code-page allocation hooks.
 * @stability experimental
 * @depends FFItk::platform, InteropTk::alloc
 */
#ifndef FFI_TRAMPOLINE_H
#define FFI_TRAMPOLINE_H
#include "ffi_types.h"
#include "../InteropTk/itk_alloc.h"
typedef struct ffi_code {
    void *address;
    size_t size;
    itk_allocator allocator;
} ffi_code;
FFI_DEF ffi_status ffi_code_alloc(ffi_code *code, size_t size,
                                  const itk_allocator *allocator);
FFI_DEF ffi_status ffi_code_commit(ffi_code *code);
FFI_DEF void ffi_code_free(ffi_code *code);
FFI_DEF void ffi_icache_flush(void *begin, void *end);
#ifdef FFI_TRAMPOLINE_IMPLEMENTATION
FFI_DEF ffi_status ffi_code_alloc(ffi_code *code, size_t size,
                                  const itk_allocator *allocator)
{ if (code == NULL || size == 0) return FFI_EINVAL;
  code->allocator = allocator ? *allocator : *itk_libc_allocator();
  code->address = itk_allocator_alloc(&code->allocator, size, sizeof(void *));
  code->size = code->address ? size : 0;
  return code->address ? FFI_OK : FFI_ENOMEM; }
FFI_DEF ffi_status ffi_code_commit(ffi_code *code)
{ return (code != NULL && code->address != NULL) ? FFI_OK : FFI_EINVAL; }
FFI_DEF void ffi_code_free(ffi_code *code)
{ if (code == NULL) return; itk_allocator_free(&code->allocator, code->address);
  code->address = NULL; code->size = 0; }
FFI_DEF void ffi_icache_flush(void *begin, void *end)
{ (void)begin; (void)end; }
#endif
#endif
