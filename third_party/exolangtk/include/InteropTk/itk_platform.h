/**
 * @file itk_platform.h
 * @brief Compile-time detection of target OS, architecture, endianness, data
 *        model (LP64/LLP64/ILP32), and toolchain. Everything downstream keys
 *        off the macros and enums defined here.
 *
 * @stability stable
 * @depends none
 */

#ifndef ITK_PLATFORM_H
#define ITK_PLATFORM_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

/* ── public declarations ──────────────────────────────────────────────── */

/* Operating system detection. Exactly one ITK_OS_* is defined to 1; the
 * others remain undefined so "#ifdef ITK_OS_LINUX" style tests work. */
#if defined(_WIN32) || defined(_WIN64)
#  define ITK_OS_WINDOWS 1
#elif defined(__linux__) || defined(__linux)
#  define ITK_OS_LINUX 1
#elif defined(__APPLE__)
#  define ITK_OS_MACOS 1
#elif defined(__FreeBSD__)
#  define ITK_OS_FREEBSD 1
#elif defined(__OpenBSD__)
#  define ITK_OS_OPENBSD 1
#elif defined(__NetBSD__)
#  define ITK_OS_NETBSD 1
#elif defined(__sun) && defined(__SVR4)
#  define ITK_OS_SOLARIS 1
#elif defined(_AIX)
#  define ITK_OS_AIX 1
#elif defined(__EMSCRIPTEN__)
#  define ITK_OS_EMSCRIPTEN 1
#else
#  define ITK_OS_UNKNOWN 1
#endif

/* POSIX flavour: true on everything Windows is not (best-effort; wasm
 * targets are excluded because they have no process model). */
#if !defined(ITK_OS_WINDOWS) && !defined(ITK_OS_EMSCRIPTEN)
#  define ITK_POSIX 1
#endif

/* Architecture detection. Exactly one ITK_ARCH_* is defined to 1. */
#if defined(__x86_64__) || defined(_M_X64)
#  define ITK_ARCH_X86_64 1
#elif defined(__i386__) || defined(_M_IX86)
#  define ITK_ARCH_X86 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#  define ITK_ARCH_AARCH64 1
#elif defined(__arm__) || defined(_M_ARM)
#  define ITK_ARCH_ARM32 1
#elif defined(__riscv) && (__riscv_xlen == 64)
#  define ITK_ARCH_RISCV64 1
#elif defined(__riscv) && (__riscv_xlen == 32)
#  define ITK_ARCH_RISCV32 1
#elif defined(__powerpc64__)
#  define ITK_ARCH_PPC64 1
#elif defined(__s390x__)
#  define ITK_ARCH_S390X 1
#elif defined(__mips__) || defined(__mips)
#  define ITK_ARCH_MIPS 1
#elif defined(__loongarch64)
#  define ITK_ARCH_LOONGARCH64 1
#else
#  define ITK_ARCH_UNKNOWN 1
#endif

/* Pointer width in bits, derived from <stdint.h> limits. */
#if INTPTR_MAX == INT64_MAX
#  define ITK_ARCH_BITS 64
#elif INTPTR_MAX == INT32_MAX
#  define ITK_ARCH_BITS 32
#elif INTPTR_MAX == INT16_MAX
#  define ITK_ARCH_BITS 16
#else
#  error "ExolangTk: unsupported pointer width"
#endif

/* Data model (size of int / long / pointer). */
#if (ITK_ARCH_BITS == 64) && (LONG_MAX == INT64_MAX)
#  define ITK_ABI_LP64 1          /**< long and pointers are 64-bit. */
#elif (ITK_ARCH_BITS == 64) && (LONG_MAX == INT32_MAX)
#  define ITK_ABI_LLP64 1         /**< long long and pointers 64-bit, long 32. */
#elif (ITK_ARCH_BITS == 32) && (INT_MAX == INT32_MAX)
#  define ITK_ABI_ILP32 1         /**< int, long, pointers all 32-bit. */
#else
#  define ITK_ABI_UNKNOWN 1
#endif

/* Byte order. Falls back to ITK_ENDIAN_UNKNOWN when no telltale macro is
 * present; itk_target_query() resolves it at runtime in that case. */
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#  define ITK_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#  define ITK_BIG_ENDIAN 1
#elif defined(_MSC_VER) || defined(__i386__) || defined(__x86_64__)
#  define ITK_LITTLE_ENDIAN 1
#else
#  define ITK_ENDIAN_UNKNOWN 1
#endif

/* Toolchain detection (informational only; no behaviour branches on it). */
#if defined(__clang__)
#  define ITK_COMPILER_CLANG 1
#elif defined(__GNUC__)
#  define ITK_COMPILER_GCC 1
#elif defined(_MSC_VER)
#  define ITK_COMPILER_MSVC 1
#else
#  define ITK_COMPILER_UNKNOWN 1
#endif

/**
 * @brief Function-qualifier macro for InteropTk implementation bodies.
 *
 * Expands to @c static by default so a header-only translation unit gets
 * internal linkage. Define @c ITK_DEF before the first include to override
 * (e.g. to @c extern when linking a dedicated implementation TU).
 * @note Must be defined identically in every TU of a program.
 */
#ifndef ITK_DEF
#  define ITK_DEF static
#endif

/**
 * @brief Boolean type for InteropTk APIs.
 *
 * Defined as @c unsigned char because @c <stdbool.h> cannot be assumed on
 * every strict C99 target (see AGENTS.md section 5).
 */
typedef unsigned char itk_bool;

#define ITK_TRUE  1  /**< Truth value for #itk_bool. */
#define ITK_FALSE 0  /**< Falsity value for #itk_bool. */

/** @brief Enumerated operating system reported by itk_target_query(). */
typedef enum itk_os {
    ITK_OS_ENUM_UNKNOWN = 0, /**< Unrecognized operating system. */
    ITK_OS_ENUM_WINDOWS,     /**< Windows (Win32 or Win64). */
    ITK_OS_ENUM_LINUX,       /**< Linux. */
    ITK_OS_ENUM_MACOS,       /**< macOS / Darwin. */
    ITK_OS_ENUM_FREEBSD,     /**< FreeBSD. */
    ITK_OS_ENUM_OPENBSD,     /**< OpenBSD. */
    ITK_OS_ENUM_NETBSD,      /**< NetBSD. */
    ITK_OS_ENUM_SOLARIS,     /**< Solaris / illumos. */
    ITK_OS_ENUM_AIX,         /**< AIX. */
    ITK_OS_ENUM_EMSCRIPTEN   /**< Emscripten / wasm. */
} itk_os;

/** @brief Enumerated CPU architecture reported by itk_target_query(). */
typedef enum itk_arch {
    ITK_ARCH_ENUM_UNKNOWN = 0, /**< Unrecognized architecture. */
    ITK_ARCH_ENUM_X86_64,      /**< AMD64 / x86-64. */
    ITK_ARCH_ENUM_X86,         /**< IA-32. */
    ITK_ARCH_ENUM_AARCH64,     /**< AArch64 / ARM64. */
    ITK_ARCH_ENUM_ARM32,       /**< 32-bit ARM. */
    ITK_ARCH_ENUM_RISCV64,     /**< RV64. */
    ITK_ARCH_ENUM_RISCV32,     /**< RV32. */
    ITK_ARCH_ENUM_PPC64,       /**< 64-bit PowerPC. */
    ITK_ARCH_ENUM_S390X,       /**< IBM s390x. */
    ITK_ARCH_ENUM_MIPS,        /**< MIPS (32 or 64 bit). */
    ITK_ARCH_ENUM_LOONGARCH64  /**< LoongArch64. */
} itk_arch;

/** @brief Enumerated data model reported by itk_target_query(). */
typedef enum itk_abi {
    ITK_ABI_ENUM_UNKNOWN = 0, /**< Unrecognized data model. */
    ITK_ABI_ENUM_LP64,        /**< LP64: 64-bit long and pointers. */
    ITK_ABI_ENUM_LLP64,       /**< LLP64: 64-bit long long/pointers. */
    ITK_ABI_ENUM_ILP32        /**< ILP32: everything 32-bit. */
} itk_abi;

/** @brief Enumerated byte order reported by itk_target_query(). */
typedef enum itk_byteorder {
    ITK_BYTEORDER_UNKNOWN = 0, /**< Not determinable. */
    ITK_BYTEORDER_LITTLE,      /**< Least-significant byte first. */
    ITK_BYTEORDER_BIG          /**< Most-significant byte first. */
} itk_byteorder;

/**
 * @brief Snapshot of every platform property InteropTk can detect.
 *
 * @var itk_target_info::os
 *      Operating system as an #itk_os value.
 * @var itk_target_info::arch
 *      CPU architecture as an #itk_arch value.
 * @var itk_target_info::abi
 *      Data model as an #itk_abi value.
 * @var itk_target_info::byteorder
 *      Byte order as an #itk_byteorder value.
 * @var itk_target_info::pointer_bits
 *      Pointer width in bits (16, 32, or 64).
 * @var itk_target_info::page_size
 *      Best-effort virtual-memory page size in bytes (0 if unknown).
 */
typedef struct itk_target_info {
    itk_os os;              /**< Operating system. */
    itk_arch arch;          /**< CPU architecture. */
    itk_abi abi;            /**< Data model. */
    itk_byteorder byteorder;/**< Byte order. */
    unsigned pointer_bits;  /**< Pointer width in bits. */
    size_t page_size;       /**< VM page size in bytes, 0 if unknown. */
} itk_target_info;

/**
 * @brief Fill @p info with the compile-time-detected platform properties.
 * @param info  Output record; must not be NULL.
 * @return Pointer to @p info on success, NULL if @p info is NULL.
 * @note Reads no global state; safe to call from any thread simultaneously.
 * @note The endianness may be resolved at runtime when the compiler exposes
 *       no endianness macro (ITK_ENDIAN_UNKNOWN builds).
 */
ITK_DEF itk_target_info *itk_target_query(itk_target_info *info);

#ifdef ITK_PLATFORM_IMPLEMENTATION
/* ── implementation section ─────────────────────────────────────────── */

#include <limits.h>

#if defined(ITK_POSIX)
#  include <unistd.h> /* sysconf(_SC_PAGESIZE) */
#endif

/** Detect byte order at runtime by storing then inspecting a known word. */
static unsigned char itk_byteorder_probe_(void)
{
    const uint16_t probe = (uint16_t)0x0102u;
    const unsigned char *bytes = (const unsigned char *)&probe;
    return (bytes[0] == 0x02u) ? (unsigned char)ITK_BYTEORDER_LITTLE
                               : (unsigned char)ITK_BYTEORDER_BIG;
}

ITK_DEF itk_target_info *itk_target_query(itk_target_info *info)
{
    if (info == NULL) {
        return NULL;
    }

#if defined(ITK_OS_WINDOWS)
    info->os = ITK_OS_ENUM_WINDOWS;
#elif defined(ITK_OS_LINUX)
    info->os = ITK_OS_ENUM_LINUX;
#elif defined(ITK_OS_MACOS)
    info->os = ITK_OS_ENUM_MACOS;
#elif defined(ITK_OS_FREEBSD)
    info->os = ITK_OS_ENUM_FREEBSD;
#elif defined(ITK_OS_OPENBSD)
    info->os = ITK_OS_ENUM_OPENBSD;
#elif defined(ITK_OS_NETBSD)
    info->os = ITK_OS_ENUM_NETBSD;
#elif defined(ITK_OS_SOLARIS)
    info->os = ITK_OS_ENUM_SOLARIS;
#elif defined(ITK_OS_AIX)
    info->os = ITK_OS_ENUM_AIX;
#elif defined(ITK_OS_EMSCRIPTEN)
    info->os = ITK_OS_ENUM_EMSCRIPTEN;
#else
    info->os = ITK_OS_ENUM_UNKNOWN;
#endif

#if defined(ITK_ARCH_X86_64)
    info->arch = ITK_ARCH_ENUM_X86_64;
#elif defined(ITK_ARCH_X86)
    info->arch = ITK_ARCH_ENUM_X86;
#elif defined(ITK_ARCH_AARCH64)
    info->arch = ITK_ARCH_ENUM_AARCH64;
#elif defined(ITK_ARCH_ARM32)
    info->arch = ITK_ARCH_ENUM_ARM32;
#elif defined(ITK_ARCH_RISCV64)
    info->arch = ITK_ARCH_ENUM_RISCV64;
#elif defined(ITK_ARCH_RISCV32)
    info->arch = ITK_ARCH_ENUM_RISCV32;
#elif defined(ITK_ARCH_PPC64)
    info->arch = ITK_ARCH_ENUM_PPC64;
#elif defined(ITK_ARCH_S390X)
    info->arch = ITK_ARCH_ENUM_S390X;
#elif defined(ITK_ARCH_MIPS)
    info->arch = ITK_ARCH_ENUM_MIPS;
#elif defined(ITK_ARCH_LOONGARCH64)
    info->arch = ITK_ARCH_ENUM_LOONGARCH64;
#else
    info->arch = ITK_ARCH_ENUM_UNKNOWN;
#endif

#if defined(ITK_ABI_LP64)
    info->abi = ITK_ABI_ENUM_LP64;
#elif defined(ITK_ABI_LLP64)
    info->abi = ITK_ABI_ENUM_LLP64;
#elif defined(ITK_ABI_ILP32)
    info->abi = ITK_ABI_ENUM_ILP32;
#else
    info->abi = ITK_ABI_ENUM_UNKNOWN;
#endif

#if defined(ITK_LITTLE_ENDIAN)
    info->byteorder = ITK_BYTEORDER_LITTLE;
#elif defined(ITK_BIG_ENDIAN)
    info->byteorder = ITK_BYTEORDER_BIG;
#else
    info->byteorder = (itk_byteorder)itk_byteorder_probe_();
#endif

    info->pointer_bits = (unsigned)ITK_ARCH_BITS;

#if defined(ITK_POSIX)
    {
        const long ps = sysconf(_SC_PAGESIZE);
        info->page_size = (ps > 0) ? (size_t)ps : (size_t)0;
    }
#else
    info->page_size = (size_t)0; /* Windows hosts query GetSystemInfo. */
#endif

    return info;
}

#endif /* ITK_PLATFORM_IMPLEMENTATION */

#endif /* ITK_PLATFORM_H */
