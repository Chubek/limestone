/**
 * @file ffi_frame.h
 * @brief Caller-owned argument and return storage associated with an ffi_cif.
 * @stability experimental
 * @depends FFItk::cif, InteropTk::marshal
 */
#ifndef FFI_FRAME_H
#define FFI_FRAME_H
#include "ffi_cif.h"
typedef struct ffi_frame {
    const ffi_cif *cif;
    const void *args[FFI_MAX_ARGS];
    size_t arg_count;
    ffi_value result;
} ffi_frame;
FFI_DEF ffi_status ffi_frame_init(ffi_frame *frame, const ffi_cif *cif);
FFI_DEF ffi_status ffi_frame_set_arg(ffi_frame *frame, size_t index,
                                     const void *value);
FFI_DEF ffi_status ffi_frame_get_return(const ffi_frame *frame,
                                        ffi_value *out);
#ifdef FFI_FRAME_IMPLEMENTATION
FFI_DEF ffi_status ffi_frame_init(ffi_frame *frame, const ffi_cif *cif)
{ if (frame == NULL || cif == NULL || !cif->prepared) return FFI_EINVAL;
  frame->cif = cif; frame->arg_count = cif->arg_count; return FFI_OK; }
FFI_DEF ffi_status ffi_frame_set_arg(ffi_frame *frame, size_t index,
                                     const void *value)
{ if (frame == NULL || value == NULL || frame->cif == NULL ||
      index >= frame->arg_count) return FFI_EINVAL;
  frame->args[index] = value; return FFI_OK; }
FFI_DEF ffi_status ffi_frame_get_return(const ffi_frame *frame, ffi_value *out)
{ if (frame == NULL || out == NULL) return FFI_EINVAL;
  *out = frame->result; return FFI_OK; }
#endif
#endif
