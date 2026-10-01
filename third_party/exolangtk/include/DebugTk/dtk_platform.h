/**
 * @file dtk_platform.h
 * @brief DebugTk qualifier and platform detection shims.
 * @stability stable
 * @depends InteropTk::platform
 */
#ifndef DTK_PLATFORM_H
#define DTK_PLATFORM_H
#include "../InteropTk/itk_platform.h"
#ifndef DTK_DEF
#define DTK_DEF static
#endif
#define DTK_OS_WINDOWS ITK_OS_WINDOWS
#define DTK_OS_LINUX ITK_OS_LINUX
#define DTK_OS_MACOS ITK_OS_MACOS
#define DTK_ARCH_X86_64 ITK_ARCH_X86_64
#define DTK_ARCH_X86 ITK_ARCH_X86
#define DTK_ARCH_AARCH64 ITK_ARCH_AARCH64
#define DTK_ARCH_ARM32 ITK_ARCH_ARM32
#ifndef DTK_PAGESIZE
#define DTK_PAGESIZE 4096u
#endif
#endif
