# InteropTk: Platform Detection {#manual_03_platform}

Module: [itk_platform.h](../include/InteropTk/itk_platform.h) | Stability: stable

## Overview

`itk_platform.h` is the foundation of the entire ExolangTk ecosystem. It
detects the target operating system, CPU architecture, data model (LP64,
LLP64, ILP32), byte order, and toolchain at compile time. Every downstream
module keys off the macros and enums defined here.

## Detection Macros

### Operating System

Exactly one `ITK_OS_*` macro is defined to 1:

| Macro | Platform |
|-------|----------|
| `ITK_OS_WINDOWS` | Windows (Win32 or Win64) |
| `ITK_OS_LINUX` | Linux |
| `ITK_OS_MACOS` | macOS / Darwin |
| `ITK_OS_FREEBSD` | FreeBSD |
| `ITK_OS_OPENBSD` | OpenBSD |
| `ITK_OS_NETBSD` | NetBSD |
| `ITK_OS_SOLARIS` | Solaris / illumos |
| `ITK_OS_AIX` | AIX |
| `ITK_OS_EMSCRIPTEN` | Emscripten / WASM |
| `ITK_OS_UNKNOWN` | Fallback |

Additionally, `ITK_POSIX` is defined to 1 on every OS except Windows and
Emscripten.

### Architecture

Exactly one `ITK_ARCH_*` macro is defined to 1:

| Macro | Architecture |
|-------|-------------|
| `ITK_ARCH_X86_64` | x86-64 / AMD64 |
| `ITK_ARCH_X86` | x86 32-bit |
| `ITK_ARCH_AARCH64` | AArch64 / ARM64 |
| `ITK_ARCH_ARM32` | ARM 32-bit |
| `ITK_ARCH_RISCV64` | RISC-V 64-bit |
| `ITK_ARCH_RISCV32` | RISC-V 32-bit |
| `ITK_ARCH_PPC64` | PowerPC 64-bit |
| `ITK_ARCH_S390X` | IBM Z |
| `ITK_ARCH_MIPS` | MIPS |
| `ITK_ARCH_LOONGARCH64` | LoongArch 64-bit |
| `ITK_ARCH_UNKNOWN` | Fallback |

`ITK_ARCH_BITS` is set to 16, 32, or 64 based on `INTPTR_MAX`.

### Data Model

| Macro | Meaning |
|-------|---------|
| `ITK_ABI_LP64` | `long` and pointers are 64-bit |
| `ITK_ABI_LLP64` | `long long` and pointers 64-bit, `long` 32-bit |
| `ITK_ABI_ILP32` | `int`, `long`, pointers all 32-bit |
| `ITK_ABI_UNKNOWN` | Fallback |

### Byte Order

| Macro | Meaning |
|-------|---------|
| `ITK_LITTLE_ENDIAN` | Least-significant byte first |
| `ITK_BIG_ENDIAN` | Most-significant byte first |
| `ITK_ENDIAN_UNKNOWN` | Fallback resolved at runtime |

### Toolchain

| Macro | Compiler |
|-------|----------|
| `ITK_COMPILER_CLANG` | Clang |
| `ITK_COMPILER_GCC` | GCC |
| `ITK_COMPILER_MSVC` | MSVC |
| `ITK_COMPILER_UNKNOWN` | Fallback |

## Function Qualifier

`ITK_DEF` is the function qualifier macro for all InteropTk implementation
bodies. It expands to `static` by default. Define `ITK_DEF` before the first
include to override it (e.g. to `extern`).

## Boolean Type

~~~c
typedef unsigned char itk_bool;
#define ITK_TRUE  1
#define ITK_FALSE 0
~~~

`itk_bool` is `unsigned char` because `<stdbool.h>` cannot be assumed on
every strict C99 target.

## Runtime Query

~~~c
typedef struct itk_target_info {
    itk_os os;
    itk_arch arch;
    itk_abi abi;
    itk_byteorder byteorder;
    unsigned pointer_bits;
    size_t page_size;
} itk_target_info;

itk_target_info *itk_target_query(itk_target_info *info);
~~~

`itk_target_query()` fills the struct with compile-time-detected properties.
The byte order may be resolved at runtime on targets where the compiler
exposes no endianness macro. The function is safe to call from any thread
simultaneously.

## Usage Example

~~~c
#define ITK_PLATFORM_IMPLEMENTATION
#include "InteropTk/itk_platform.h"

itk_target_info info;
itk_target_query(&info);

#ifdef ITK_OS_LINUX
/* Linux-specific path */
#endif

#ifdef ITK_ARCH_X86_64
/* x86-64-specific path */
#endif
~~~
