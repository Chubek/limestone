/**
 * @file etk_platform.h
 * @brief ExtensionTk qualifier and platform detection shims.
 * @stability stable
 * @depends InteropTk::platform
 */
#ifndef ETK_PLATFORM_H
#define ETK_PLATFORM_H
#include "../InteropTk/itk_platform.h"
#ifndef ETK_DEF
#define ETK_DEF static
#endif
#define ETK_OS_WINDOWS ITK_OS_WINDOWS
#define ETK_OS_LINUX ITK_OS_LINUX
#define ETK_OS_MACOS ITK_OS_MACOS
#define ETK_ARCH_X86_64 ITK_ARCH_X86_64
#define ETK_ARCH_AARCH64 ITK_ARCH_AARCH64
#endif
