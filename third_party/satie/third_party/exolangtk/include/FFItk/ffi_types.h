/**
 * @file ffi_types.h
 * @brief Common FFItk status and machine-word value types.
 * @stability stable
 * @depends FFItk::platform, InteropTk::platform
 */
#ifndef FFI_TYPES_H
#define FFI_TYPES_H
#include "ffi_platform.h"
typedef enum ffi_status {
    FFI_OK = 0, FFI_EINVAL, FFI_ENOMEM, FFI_ENOSYS, FFI_ENOTFOUND,
    FFI_EOVERFLOW, FFI_EFAIL
} ffi_status;
typedef uintptr_t ffi_arg;
typedef intptr_t ffi_sarg;
typedef union ffi_value {
    ffi_arg word;
    ffi_sarg sword;
    float f;
    double d;
    long double ld;
    void *ptr;
} ffi_value;
#endif
