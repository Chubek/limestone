/**
 * @file itk_mangle.h
 * @brief Symbol name decoration and demangling helpers for linking against
 *        C toolchains: leading-underscore conventions, stdcall suffixes, and
 *        section/visibility naming quirks per platform.
 *
 * @stability stable
 * @depends InteropTk::platform
 */

#ifndef ITK_MANGLE_H
#define ITK_MANGLE_H

#include "itk_platform.h"

/* ── public declarations ──────────────────────────────────────────────── */

/**
 * @name Decoration flags
 * @brief Bitmask fed to itk_mangle_c_symbol(). Exactly one calling-convention
 *        bit (NONE/STDCALL/FASTCALL) may be set; the leading-underscore bit
 *        is normally supplied automatically per target.
 * @{ */
#define ITK_MANGLE_NONE     0x0u  /**< Plain cdecl-ish, no decoration. */
#define ITK_MANGLE_STDCALL  0x1u  /**< Win32 stdcall: name@bytes suffix. */
#define ITK_MANGLE_FASTCALL 0x2u  /**< Win32 fastcall: @name@bytes. */
#define ITK_MANGLE_UNDERSCORE 0x4u /**< Force a leading underscore. */
/** @} */

/** @brief Capacity suffices for any name this module will produce. */
#define ITK_MANGLE_NAME_MAX 512

/** @brief ELF/Generic symbol visibility classes. */
typedef enum itk_symbol_visibility {
    ITK_VIS_DEFAULT  = 0, /**< Exported, overridable (STV_DEFAULT). */
    ITK_VIS_INTERNAL = 1, /**< Not visible outside the object (STV_HIDDEN). */
    ITK_VIS_PROTECTED = 2 /**< Exported, non-preemptible (STV_PROTECTED). */
} itk_symbol_visibility;

/**
 * @brief Canonical lowercase name for a visibility class.
 * @param vis  Visibility value.
 * @return Static string "default", "internal", or "protected"; "default"
 *         for out-of-range inputs.
 * @note Points at immutable storage; never freed.
 */
ITK_DEF const char *itk_symbol_visibility_name(itk_symbol_visibility vis);

/**
 * @brief Whether the target toolchain prefixes C symbols with an underscore.
 * @return ITK_TRUE on macOS and classic BSD targets, ITK_FALSE on Linux,
 *         Windows (ELF-style PE names carry no underscore for cdecl).
 * @note Pure function of compile-time detection; safe from any thread.
 */
ITK_DEF itk_bool itk_mangle_has_leading_underscore(void);

/**
 * @brief Decorate a C symbol name for the target platform.
 * @param name          Bare symbol name; must not be NULL or empty.
 * @param flags         Combination of the @c ITK_MANGLE_* bits.
 * @param arg_bytes     Total bytes of the parameter list; only consulted
 *                      for STDCALL/FASTCALL decoration.
 * @param out           Destination buffer.
 * @param cap           Capacity of @p out including the terminator.
 * @return ITK_TRUE and a NUL-terminated symbol in @p out on success;
 *         ITK_FALSE (with @p out[0] cleared when @p out is writable) on
 *         invalid arguments or insufficient capacity.
 * @note Reads no global state; safe from any thread.
 */
ITK_DEF itk_bool itk_mangle_c_symbol(const char *name, unsigned flags,
                             size_t arg_bytes, char *out, size_t cap);

/**
 * @brief Recover the bare name and argument size from a decorated symbol.
 * @param mangled  Decorated symbol; must not be NULL.
 * @param out      Destination for the bare name.
 * @param cap      Capacity of @p out including the terminator.
 * @param arg_bytes  When non-NULL, receives the decoded @c @N suffix byte
 *                   count, or 0 when absent.
 * @return ITK_TRUE on successful decode; ITK_FALSE on invalid arguments or
 *         truncation. Underscore stripping follows the same per-platform
 *         rule as itk_mangle_c_symbol().
 * @note Pure function; safe from any thread.
 */
ITK_DEF itk_bool itk_demangle_c_symbol(const char *mangled, char *out, size_t cap,
                               size_t *arg_bytes);

#ifdef ITK_MANGLE_IMPLEMENTATION
/* ── implementation section ─────────────────────────────────────────── */

#include <string.h>

ITK_DEF const char *itk_symbol_visibility_name(itk_symbol_visibility vis)
{
    switch (vis) {
    case ITK_VIS_INTERNAL:  return "internal";
    case ITK_VIS_PROTECTED: return "protected";
    case ITK_VIS_DEFAULT:
    default:                return "default";
    }
}

ITK_DEF itk_bool itk_mangle_has_leading_underscore(void)
{
#if defined(ITK_OS_MACOS) || defined(ITK_OS_AIX)
    return ITK_TRUE;
#else
    return ITK_FALSE;
#endif
}

/** Append @p s to @p out at *pos, tracking capacity. Returns 0 on overflow. */
static itk_bool itk_mangle_append_(char *out, size_t cap, size_t *pos,
                                   const char *s)
{
    while (*s != '\0') {
        if ((*pos) + 1 >= cap) {
            return ITK_FALSE;
        }
        out[*pos] = *s;
        (*pos)++;
        s++;
    }
    return ITK_TRUE;
}

ITK_DEF itk_bool itk_mangle_c_symbol(const char *name, unsigned flags,
                                     size_t arg_bytes, char *out, size_t cap)
{
    size_t pos = 0;

    if (name == NULL || out == NULL || cap == 0 || name[0] == '\0') {
        if (out != NULL && cap > 0) {
            out[0] = '\0';
        }
        return ITK_FALSE;
    }
    if ((flags & (ITK_MANGLE_STDCALL | ITK_MANGLE_FASTCALL)) ==
        (ITK_MANGLE_STDCALL | ITK_MANGLE_FASTCALL)) {
        out[0] = '\0';
        return ITK_FALSE; /* conflicting convention bits */
    }

    if ((flags & ITK_MANGLE_UNDERSCORE) != 0 || itk_mangle_has_leading_underscore()) {
        if (!itk_mangle_append_(out, cap, &pos, "_")) {
            out[0] = '\0';
            return ITK_FALSE;
        }
    }
    if ((flags & ITK_MANGLE_FASTCALL) != 0) {
        if (!itk_mangle_append_(out, cap, &pos, "@")) {
            out[0] = '\0';
            return ITK_FALSE;
        }
    }
    if (!itk_mangle_append_(out, cap, &pos, name)) {
        out[0] = '\0';
        return ITK_FALSE;
    }
    if ((flags & (ITK_MANGLE_STDCALL | ITK_MANGLE_FASTCALL)) != 0) {
        char suffix[32];
        size_t n = 0;

        suffix[n++] = '@';
        {   /* render arg_bytes without snprintf's locale surface */
            char digits[24];
            size_t d = 0;
            size_t v = arg_bytes;

            do {
                digits[d++] = (char)('0' + (v % 10u));
                v /= 10u;
            } while (v > 0 && d < sizeof(digits));
            while (d > 0) {
                if ((n + 1) >= sizeof(suffix)) {
                    out[0] = '\0';
                    return ITK_FALSE;
                }
                suffix[n++] = digits[--d];
            }
        }
        suffix[n] = '\0';
        if (!itk_mangle_append_(out, cap, &pos, suffix)) {
            out[0] = '\0';
            return ITK_FALSE;
        }
    }
    out[pos] = '\0';
    return ITK_TRUE;
}

ITK_DEF itk_bool itk_demangle_c_symbol(const char *mangled, char *out,
                                       size_t cap, size_t *arg_bytes)
{
    size_t len, start = 0, end;

    if (mangled == NULL || out == NULL || cap == 0) {
        if (out != NULL && cap > 0) {
            out[0] = '\0';
        }
        return ITK_FALSE;
    }
    if (arg_bytes != NULL) {
        *arg_bytes = (size_t)0;
    }
    len = strlen(mangled);
    if (len == 0) {
        out[0] = '\0';
        return ITK_FALSE;
    }

    /* Strip a leading underscore when the platform adds one (and the name
     * does not consist solely of that underscore). */
    if (itk_mangle_has_leading_underscore() && mangled[0] == '_' && len > 1) {
        start = 1;
    }
    /* Win32 fastcall keeps a stray leading '@' after underscore strip. */
    if (mangled[start] == '@' && start + 1 < len) {
        start++;
    }

    end = len;
    if (end > start && mangled[end - 1] != '@') {
        size_t at = 0, i;

        /* find last '@' inside the body */
        for (i = start; i < end; i++) {
            if (mangled[i] == '@') {
                at = i;
            }
        }
        if (at > start) {
            size_t acc = 0;
            itk_bool digits = ITK_TRUE;

            for (i = at + 1; i < end; i++) {
                if (mangled[i] < '0' || mangled[i] > '9') {
                    digits = ITK_FALSE;
                    break;
                }
                if (acc > (((size_t)-1) - (size_t)(mangled[i] - '0')) / 10u) {
                    digits = ITK_FALSE; /* would overflow */
                    break;
                }
                acc = acc * 10u + (size_t)(mangled[i] - '0');
            }
            if (digits) {
                if (arg_bytes != NULL) {
                    *arg_bytes = acc;
                }
                end = at;
            }
        }
    } else if (end > start + 1 && mangled[end - 1] == '@') {
        /* Bare trailing '@' (e.g. Fortran-style): drop it. */
        end--;
    }

    if (end - start >= cap) {
        out[0] = '\0';
        return ITK_FALSE;
    }
    memcpy(out, mangled + start, end - start);
    out[end - start] = '\0';
    return ITK_TRUE;
}

#endif /* ITK_MANGLE_IMPLEMENTATION */

#endif /* ITK_MANGLE_H */
