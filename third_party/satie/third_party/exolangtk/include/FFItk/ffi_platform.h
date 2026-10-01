/**
 * @file ffi_platform.h
 * @brief FFItk qualifier and platform-detection shims.
 * @stability stable
 * @depends InteropTk::platform
 */
#ifndef FFI_PLATFORM_H
#define FFI_PLATFORM_H
#include "../InteropTk/itk_platform.h"
#ifndef FFI_DEF
#define FFI_DEF static
#endif
#define FFI_OS_WINDOWS ITK_OS_WINDOWS
#define FFI_OS_LINUX ITK_OS_LINUX
#define FFI_OS_MACOS ITK_OS_MACOS
#define FFI_ARCH_X86_64 ITK_ARCH_X86_64
#define FFI_ARCH_X86 ITK_ARCH_X86
#define FFI_ARCH_AARCH64 ITK_ARCH_AARCH64
#define FFI_ARCH_ARM32 ITK_ARCH_ARM32
#endif
