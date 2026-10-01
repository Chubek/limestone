/**
 * @file itk_export.h
 * @brief Boilerplate macros for producing C-consumable APIs from a
 *        compiler's generated code: ITK_EXPORT/ITK_IMPORT visibility,
 *        ITK_EXTERN_C guards, ITK_API versioned symbol annotations, and
 *        static-assert shims for C99.
 *
 * @stability stable
 * @depends InteropTk::platform
 */

#ifndef ITK_EXPORT_H
#define ITK_EXPORT_H

#include "itk_platform.h"

/* ── public declarations ──────────────────────────────────────────────── */

/**
 * @brief Mark a symbol as exported from a shared library.
 *
 * Expands to the platform visibility attribute through the ITK_ATTR_*
 * shims below; empty when the toolchain offers nothing.
 */
#if defined(ITK_COMPILER_GCC) || defined(ITK_COMPILER_CLANG)
#  define ITK_ATTR_VISIBILITY_DEFAULT __attribute__((visibility("default")))
#  define ITK_ATTR_VISIBILITY_HIDDEN   __attribute__((visibility("hidden")))
#else
#  define ITK_ATTR_VISIBILITY_DEFAULT
#  define ITK_ATTR_VISIBILITY_HIDDEN
#endif

#if defined(ITK_OS_WINDOWS) && defined(_MSC_VER)
#  define ITK_ATTR_DLLEXPORT __declspec(dllexport)
#  define ITK_ATTR_DLLIMPORT __declspec(dllimport)
#else
#  define ITK_ATTR_DLLEXPORT ITK_ATTR_VISIBILITY_DEFAULT
#  define ITK_ATTR_DLLIMPORT
#endif

/**
 * @brief Declare a symbol as coming from a shared library being built.
 * @param T  The declaration to annotate, e.g. @c ITK_EXPORT(int) f(void);
 */
#define ITK_EXPORT(T) ITK_ATTR_DLLEXPORT T

/**
 * @brief Declare a symbol as consumed from a shared library.
 * @param T  The declaration to annotate.
 */
#define ITK_IMPORT(T) ITK_ATTR_DLLIMPORT T

/**
 * @brief Declare a symbol with explicit hidden visibility (internal to the
 *        shared object even when compiled with -fvisibility=default).
 * @param T  The declaration to annotate.
 */
#define ITK_LOCAL(T) ITK_ATTR_VISIBILITY_HIDDEN T

/**
 * @brief C++ linkage guard (expansion begin).
 *
 * Use in headers includable from C++: @c ITK_EXTERN_C_BEGIN ... declarations
 * ... @c ITK_EXTERN_C_END. Empty in C translation units.
 */
#ifdef __cplusplus
#  define ITK_EXTERN_C_BEGIN extern "C" {
#  define ITK_EXTERN_C_END   }
#else
#  define ITK_EXTERN_C_BEGIN
#  define ITK_EXTERN_C_END
#endif

/**
 * @brief Versioned API annotation for generated symbols.
 * @param T     The declaration to annotate.
 * @param name  Bare symbol name (informational; aids bindgen scanners).
 * @param ver   API version as an integral literal, e.g. 2.
 *
 * On ELF toolchains with symbol-versioning support the macro is a no-op
 * placeholder: C99 offers no portable version-script integration, so
 * versioning stays a link-time concern. The annotation keeps the intent in
 * the source for tooling to consume.
 */
#define ITK_API(T, name, ver) ITK_EXPORT(T)

/**
 * @brief Compile-time assertion for C99 targets.
 * @param cond  Integer constant expression; nonzero to pass.
 * @param msg   Identifier fragment surfaced in diagnostics.
 * @note Emits a negative-array-size error on failure. Has no runtime cost.
 */
#define ITK_STATIC_ASSERT(cond, msg) \
    typedef char itk_static_assert_##msg[(cond) ? 1 : -1]

/**
 * @brief Offset-of shim usable in constant expressions on C99.
 * @param type  Aggregate type.
 * @param memb  Member within @p type.
 * @note GCC/Clang/__builtin_offsetof is used when available, otherwise the
 *       classic null-pointer cast (still constant-foldable there).
 */
#if defined(ITK_COMPILER_GCC) || defined(ITK_COMPILER_CLANG)
#  define ITK_OFFSETOF(type, memb) __builtin_offsetof(type, memb)
#else
#  define ITK_OFFSETOF(type, memb) ((size_t)&(((type *)0)->memb))
#endif

#endif /* ITK_EXPORT_H */
