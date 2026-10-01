/**
 * @file itk_error.h
 * @brief Uniform bridging of C error channels (errno, return codes,
 *        GetLastError) into a single itk_status type that a host runtime can
 *        translate into its own exception or result mechanism.
 *
 * @stability stable
 * @depends InteropTk::platform
 */

#ifndef ITK_ERROR_H
#define ITK_ERROR_H

#include "itk_platform.h"

/* ── public declarations ──────────────────────────────────────────────── */

/** @brief Maximum characters (incl. NUL) stored in #itk_status::message. */
#define ITK_STATUS_MESSAGE_MAX 128

/**
 * @name Status domains
 * @brief Coarse classification of where a failure originated. The numeric
 *        @c domain value never collides between domains.
 * @{ */
#define ITK_DOMAIN_OK    0  /**< Success; @c code is always 0. */
#define ITK_DOMAIN_ERRNO 1  /**< POSIX/ISO C @c errno value. */
#define ITK_DOMAIN_WIN32 2  /**< Windows @c GetLastError() value. */
#define ITK_DOMAIN_ITK   3  /**< InteropTk-internal logic error. */
#define ITK_DOMAIN_HOST  4  /**< Reserved for host-runtime codes. */
/** @} */

/**
 * @brief Uniform error record threaded through every InteropTk API.
 *
 * A zero-initialized record (or one produced by itk_status_ok()) means
 * success. The struct is small enough to return by value.
 *
 * @var itk_status::domain
 *      One of the @c ITK_DOMAIN_* constants.
 * @var itk_status::code
 *      Domain-specific raw code (@c errno value, @c GetLastError() value,
 *      or an #itk_status_code for ITK_DOMAIN_ITK).
 * @var itk_status::message
 *      NUL-terminated human-readable description; truncated (never
 *      overrun) to #ITK_STATUS_MESSAGE_MAX characters.
 */
typedef struct itk_status {
    int domain;  /**< Failure origin classification. */
    int code;    /**< Raw domain-specific code. */
    char message[ITK_STATUS_MESSAGE_MAX]; /**< Bounded description. */
} itk_status;

/** @brief Internal InteropTk logic codes used with ITK_DOMAIN_ITK. */
typedef enum itk_status_code {
    ITK_ENOERR   = 0,  /**< No error. */
    ITK_EINVAL   = 1,  /**< Invalid argument (NULL handle, bad index). */
    ITK_ENOMEM   = 2,  /**< Allocator returned NULL. */
    ITK_ERANGE   = 3,  /**< Value out of representable range. */
    ITK_ENOSYS   = 4,  /**< Operation unsupported on this target. */
    ITK_EOVERFLOW= 5,  /**< Computation would overflow a size type. */
    ITK_ENOTFOUND= 6,  /**< Requested entity does not exist. */
    ITK_ETRUNC   = 7,  /**< Output truncated by a capacity limit. */
    ITK_EFAIL    = 8   /**< Unclassified internal failure. */
} itk_status_code;

/**
 * @brief Return a success status.
 * @return An #itk_status with domain/code zero and an empty message.
 * @note Pure function; safe from any thread.
 */
ITK_DEF itk_status itk_status_ok(void);

/**
 * @brief Report whether @p st represents success.
 * @param st  Status to test.
 * @return ITK_TRUE when @p st has domain @c ITK_DOMAIN_OK and code 0.
 * @note Pure function; safe from any thread.
 */
ITK_DEF itk_bool itk_status_is_ok(itk_status st);

/**
 * @brief Construct a failure status from an InteropTk logic code.
 * @param code    One of the #itk_status_code values (nonzero).
 * @param message Human-readable description copied into the record; may be
 *                NULL for a generic per-code description.
 * @return The constructed #itk_status.
 * @note @p message is copied, never stored by reference.
 */
ITK_DEF itk_status itk_status_set(int code, const char *message);

/**
 * @brief Capture the current @c errno into an #itk_status.
 * @param fallback_message  Description used when @c errno yields no text;
 *                          may be NULL.
 * @return Status with domain ITK_DOMAIN_ERRNO and the current @c errno.
 * @note Reads @c errno; call immediately after the failing operation.
 *       @c strerror is used verbatim and may not be reentrant.
 */
ITK_DEF itk_status itk_status_from_errno(const char *fallback_message);

/**
 * @brief Capture the platform's per-thread last error into an #itk_status.
 * @param fallback_message  Description used when no text is available; may
 *                          be NULL.
 * @return On Windows: domain ITK_DOMAIN_WIN32 with @c GetLastError() and
 *         its FormatMessage text. Elsewhere: domain ITK_DOMAIN_ERRNO with
 *         the current @c errno.
 * @note Call immediately after the failing operation.
 */
ITK_DEF itk_status itk_status_from_lasterror(const char *fallback_message);

/**
 * @brief Translate a raw OS error code into a coarse InteropTk code.
 * @param domain  The domain @p code belongs to (ITK_DOMAIN_ERRNO or
 *                ITK_DOMAIN_WIN32).
 * @param code    Raw OS code.
 * @return The closest #itk_status_code (e.g. ENOMEM -> ITK_ENOMEM).
 * @note Pure mapping; safe from any thread.
 */
ITK_DEF int itk_status_from_os(int domain, int code);

#ifdef ITK_ERROR_IMPLEMENTATION
/* ── implementation section ─────────────────────────────────────────── */

#include <string.h>
#include <errno.h>
#include <stdio.h>

#if defined(ITK_OS_WINDOWS)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

/** Copy @p msg into @p dst with guaranteed NUL termination. */
static void itk_status_copy_msg_(char *dst, const char *msg)
{
    size_t i = 0;
    if (msg == NULL) {
        dst[0] = '\0';
        return;
    }
    while ((i + 1) < ITK_STATUS_MESSAGE_MAX && msg[i] != '\0') {
        dst[i] = msg[i];
        i++;
    }
    dst[i] = '\0';
}

ITK_DEF itk_status itk_status_ok(void)
{
    itk_status st;
    st.domain = ITK_DOMAIN_OK;
    st.code = 0;
    st.message[0] = '\0';
    return st;
}

ITK_DEF itk_bool itk_status_is_ok(itk_status st)
{
    return (st.domain == ITK_DOMAIN_OK && st.code == 0) ? ITK_TRUE
                                                        : ITK_FALSE;
}

ITK_DEF itk_status itk_status_set(int code, const char *message)
{
    itk_status st;
    const char *generic = "InteropTk failure";

    st.domain = ITK_DOMAIN_ITK;
    st.code = code;
    switch (code) {
    case ITK_EINVAL:    generic = "invalid argument"; break;
    case ITK_ENOMEM:    generic = "allocation failed"; break;
    case ITK_ERANGE:    generic = "value out of range"; break;
    case ITK_ENOSYS:    generic = "operation not supported"; break;
    case ITK_EOVERFLOW: generic = "size computation overflow"; break;
    case ITK_ENOTFOUND: generic = "entity not found"; break;
    case ITK_ETRUNC:    generic = "output truncated"; break;
    case ITK_EFAIL:     generic = "unclassified failure"; break;
    default:            break;
    }
    itk_status_copy_msg_(st.message, (message != NULL) ? message : generic);
    return st;
}

ITK_DEF itk_status itk_status_from_errno(const char *fallback_message)
{
    itk_status st;
    const int e = errno;

    st.domain = ITK_DOMAIN_ERRNO;
    st.code = e;
    if (e == 0) {
        itk_status_copy_msg_(st.message,
                             (fallback_message != NULL) ? fallback_message
                                                        : "unknown errno failure");
    } else {
        /* strerror text is truncated to fit; strerror is not guaranteed
         * reentrant, so callers should capture the status promptly. */
        itk_status_copy_msg_(st.message, strerror(e));
    }
    return st;
}

ITK_DEF itk_status itk_status_from_lasterror(const char *fallback_message)
{
#if defined(ITK_OS_WINDOWS)
    {
        itk_status st;
        const DWORD e = GetLastError();
        char buf[ITK_STATUS_MESSAGE_MAX];
        DWORD n;

        st.domain = ITK_DOMAIN_WIN32;
        st.code = (int)e;
        n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, e,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           buf, (DWORD)sizeof(buf), NULL);
        if (n > 0) {
            /* Strip trailing CR/LF the formatter appends. */
            while (n > 0 && (buf[n - 1] == '\r' || buf[n - 1] == '\n')) {
                buf[n - 1] = '\0';
                n--;
            }
            itk_status_copy_msg_(st.message, buf);
        } else {
            itk_status_copy_msg_(st.message,
                                 (fallback_message != NULL)
                                     ? fallback_message
                                     : "unknown Windows error");
        }
        return st;
    }
#else
    return itk_status_from_errno(fallback_message);
#endif
}

ITK_DEF int itk_status_from_os(int domain, int code)
{
#if defined(ITK_OS_WINDOWS)
    if (domain == ITK_DOMAIN_WIN32) {
        switch (code) {
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:       return ITK_ENOMEM;
        case ERROR_INVALID_HANDLE:
        case ERROR_INVALID_PARAMETER: return ITK_EINVAL;
        default:                      return ITK_EFAIL;
        }
    }
#else
    (void)domain;
#endif
    switch (code) {
    case 0:          return ITK_ENOERR;
    case ENOMEM:     return ITK_ENOMEM;
    case EINVAL:     return ITK_EINVAL;
    case ERANGE:     return ITK_ERANGE;
    case ENOSYS:     return ITK_ENOSYS;
    case ENOENT:     return ITK_ENOTFOUND;
    default:         return ITK_EINVAL;
    }
}

#endif /* ITK_ERROR_IMPLEMENTATION */

#endif /* ITK_ERROR_H */
