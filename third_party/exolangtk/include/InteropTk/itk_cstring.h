/**
 * @file itk_cstring.h
 * @brief Ownership-aware bridging of strings across the boundary: borrowed
 *        vs. owned views, NUL-termination guarantees, length-prefixed
 *        conversion, and UTF-8 validation for hosts with non-C string
 *        representations.
 *
 * @stability stable
 * @depends InteropTk::platform, InteropTk::alloc
 */

#ifndef ITK_CSTRING_H
#define ITK_CSTRING_H

#include "itk_platform.h"
#include "itk_alloc.h"

/* ── public declarations ──────────────────────────────────────────────── */

/**
 * @brief Borrowed, length-carrying view of a byte sequence.
 *
 * The view never owns or copies; @c data need not be NUL-terminated.
 *
 * @var itk_str_view::data
 *      Start of the bytes; NULL only for the empty initializer.
 * @var itk_str_view::len
 *      Number of bytes referenced.
 */
typedef struct itk_str_view {
    const char *data; /**< First byte. */
    size_t len;       /**< Byte count. */
} itk_str_view;

/** @brief Initializer for an empty (NULL, 0) view. */
#define ITK_STR_VIEW_EMPTY { NULL, (size_t)0 }

/**
 * @brief Owned, growable, always-NUL-terminated string buffer.
 *
 * @var itk_str_buf::data
 *      Heap bytes, NUL-terminated at index @c len; NULL while capacity 0.
 * @var itk_str_buf::len
 *      Bytes stored, excluding the terminator.
 * @var itk_str_buf::cap
 *      Allocated capacity, excluding the terminator.
 * @var itk_str_buf::alloc
 *      Backing allocator copied at init time.
 */
typedef struct itk_str_buf {
    char *data;             /**< NUL-terminated payload. */
    size_t len;             /**< Payload bytes. */
    size_t cap;             /**< Allocation size. */
    itk_allocator alloc;    /**< Owner of @c data. */
} itk_str_buf;

/** @brief Outcome codes of itk_utf8_validate(). */
typedef enum itk_utf8_status {
    ITK_UTF8_OK        = 0, /**< Whole sequence is valid UTF-8. */
    ITK_UTF8_EMPTY     = 1, /**< View had zero length (treated as valid). */
    ITK_UTF8_TRUNCATED = 2, /**< Multi-byte sequence cut short at the end. */
    ITK_UTF8_BAD_LEAD  = 3, /**< Invalid lead byte (0x80-0xC1, 0xF5-0xFF). */
    ITK_UTF8_BAD_CONT  = 4, /**< Continuation byte where 0x80-0xBF required. */
    ITK_UTF8_OVERLONG  = 5, /**< Encodes a codepoint with more bytes than
                                 needed (e.g. C0/C1 lead, E0 80..). */
    ITK_UTF8_SURROGATE = 6, /**< Encodes a UTF-16 surrogate (ED A0..BF). */
    ITK_UTF8_TOO_LARGE = 7  /**< Encodes a codepoint above U+10FFFF. */
} itk_utf8_status;

/**
 * @brief Wrap a NUL-terminated C string in a borrowed view.
 * @param s  C string; may be NULL (yields an empty view).
 * @return View spanning strlen(@p s) bytes.
 * @note Borrows @p s; the view is invalid once @p s is released.
 */
ITK_DEF itk_str_view itk_str_from_c(const char *s);

/**
 * @brief Wrap an explicit byte range in a borrowed view.
 * @param data  Start of the bytes; NULL is only legal with @p len 0.
 * @param len   Number of bytes.
 * @return The constructed view.
 * @note No copy is made; the range must outlive the view.
 */
ITK_DEF itk_str_view itk_str_view_from_len(const char *data, size_t len);

/**
 * @brief Copy a view into a caller buffer with guaranteed NUL termination.
 * @param v    Source bytes.
 * @param dst  Destination of at least @p cap bytes; must not be NULL.
 * @param cap  Capacity of @p dst including the terminator.
 * @return ITK_TRUE on success; ITK_FALSE when @p dst is NULL or
 *         @p cap < v.len + 1 (nothing is written then).
 * @note Length-prefixed hosts use the return plus v.len; C hosts read
 *       @p dst directly.
 */
ITK_DEF itk_bool itk_str_to_c(itk_str_view v, char *dst, size_t cap);

/**
 * @brief Byte-wise equality of two views.
 * @param a  Left operand.
 * @param b  Right operand.
 * @return ITK_TRUE when lengths and contents match exactly.
 * @note Embedded NULs participate; this is not strcmp semantics.
 */
ITK_DEF itk_bool itk_str_view_equals(itk_str_view a, itk_str_view b);

/**
 * @brief Bounds-checked byte access into a view.
 * @param v      View to index.
 * @param index  Byte offset.
 * @param out    Receives the byte; may be NULL to just probe.
 * @return ITK_TRUE when @p index < v.len, ITK_FALSE otherwise.
 */
ITK_DEF itk_bool itk_str_view_index(itk_str_view v, size_t index, char *out);

/**
 * @brief Prepare @p buf for growth through @p a.
 * @param buf  Buffer to initialize; must not be NULL.
 * @param a    Allocator copied by value; NULL selects the libc adapter.
 * @return ITK_TRUE on success, ITK_FALSE when @p buf is NULL.
 * @note No allocation happens until the first append/reserve.
 */
ITK_DEF itk_bool itk_str_buf_init(itk_str_buf *buf, const itk_allocator *a);

/**
 * @brief Release @p buf's heap bytes and zero the struct.
 * @param buf  Buffer previously initialized with itk_str_buf_init().
 * @note The buffer must be re-initialized before reuse.
 */
ITK_DEF void itk_str_buf_free(itk_str_buf *buf);

/**
 * @brief Ensure capacity for @p extra additional bytes plus terminator.
 * @param buf    Target buffer.
 * @param extra  Additional bytes to make room for.
 * @return ITK_TRUE on success or when capacity already suffices;
 *         ITK_FALSE on allocation failure (buffer untouched).
 */
ITK_DEF itk_bool itk_str_buf_reserve(itk_str_buf *buf, size_t extra);

/**
 * @brief Append raw bytes to @p buf.
 * @param buf   Target buffer.
 * @param data  Bytes to append; NULL only legal with @p len 0.
 * @param len   Byte count.
 * @return ITK_TRUE on success; ITK_FALSE leaves the buffer untouched.
 */
ITK_DEF itk_bool itk_str_buf_append(itk_str_buf *buf, const char *data, size_t len);

/**
 * @brief Append a NUL-terminated C string's bytes (not its terminator).
 * @param buf  Target buffer.
 * @param s    C string; NULL is ignored.
 * @return ITK_TRUE on success; ITK_FALSE leaves the buffer untouched.
 */
ITK_DEF itk_bool itk_str_buf_append_c(itk_str_buf *buf, const char *s);

/**
 * @brief Validate @p v as UTF-8, reporting the first offending byte.
 * @param v       Bytes to scan.
 * @param offset  When non-NULL, receives the index of the first byte of the
 *                offending sequence (or v.len for truncation at end).
 * @return An #itk_utf8_status; ITK_UTF8_OK or ITK_UTF8_EMPTY mean valid.
 * @note Pure scan of caller memory; safe from any thread.
 */
ITK_DEF itk_utf8_status itk_utf8_validate(itk_str_view v, size_t *offset);

#ifdef ITK_CSTRING_IMPLEMENTATION
/* ── implementation section ─────────────────────────────────────────── */

#include <string.h>

ITK_DEF itk_str_view itk_str_from_c(const char *s)
{
    itk_str_view v;
    v.data = s;
    v.len = (s != NULL) ? strlen(s) : (size_t)0;
    return v;
}

ITK_DEF itk_str_view itk_str_view_from_len(const char *data, size_t len)
{
    itk_str_view v;
    v.data = (len == 0) ? NULL : data;
    v.len = len;
    return v;
}

ITK_DEF itk_bool itk_str_to_c(itk_str_view v, char *dst, size_t cap)
{
    if (dst == NULL || cap < v.len + 1) {
        return ITK_FALSE;
    }
    if (v.len > 0) {
        memcpy(dst, v.data, v.len);
    }
    dst[v.len] = '\0';
    return ITK_TRUE;
}

ITK_DEF itk_bool itk_str_view_equals(itk_str_view a, itk_str_view b)
{
    if (a.len != b.len) {
        return ITK_FALSE;
    }
    if (a.len == 0) {
        return ITK_TRUE;
    }
    return (memcmp(a.data, b.data, a.len) == 0) ? ITK_TRUE : ITK_FALSE;
}

ITK_DEF itk_bool itk_str_view_index(itk_str_view v, size_t index, char *out)
{
    if (index >= v.len) {
        return ITK_FALSE;
    }
    if (out != NULL) {
        *out = v.data[index];
    }
    return ITK_TRUE;
}

ITK_DEF itk_bool itk_str_buf_init(itk_str_buf *buf, const itk_allocator *a)
{
    if (buf == NULL) {
        return ITK_FALSE;
    }
    buf->data = NULL;
    buf->len = (size_t)0;
    buf->cap = (size_t)0;
    buf->alloc = (a != NULL) ? *a : *itk_libc_allocator();
    return ITK_TRUE;
}

ITK_DEF void itk_str_buf_free(itk_str_buf *buf)
{
    if (buf == NULL) {
        return;
    }
    if (buf->data != NULL) {
        itk_allocator_free(&buf->alloc, buf->data);
    }
    buf->data = NULL;
    buf->len = (size_t)0;
    buf->cap = (size_t)0;
}

ITK_DEF itk_bool itk_str_buf_reserve(itk_str_buf *buf, size_t extra)
{
    size_t want;
    char *grown;

    if (buf == NULL || (extra == 0 && buf->cap > buf->len)) {
        return (buf != NULL);
    }
    /* +1 for the terminator, included in cap accounting. */
    if (extra > (size_t)-1 - buf->len - 1) {
        return ITK_FALSE;
    }
    want = buf->len + extra + 1;
    if (buf->cap >= want) {
        return ITK_TRUE;
    }
    {   /* Grow 1.5x to amortize, at least to `want`. */
        const size_t half = buf->cap / 2;
        const size_t geometric = (buf->cap + half > want && buf->cap > 0)
                                     ? buf->cap + half
                                     : want;
        want = geometric;
    }
    grown = (char *)itk_allocator_realloc(&buf->alloc, buf->data, buf->cap,
                                          want, (size_t)(2 * sizeof(void *)));
    if (grown == NULL) {
        return ITK_FALSE;
    }
    buf->data = grown;
    buf->cap = want;
    return ITK_TRUE;
}

ITK_DEF itk_bool itk_str_buf_append(itk_str_buf *buf, const char *data,
                                    size_t len)
{
    if (buf == NULL) {
        return ITK_FALSE;
    }
    if (len == 0) {
        return ITK_TRUE;
    }
    if (data == NULL) {
        return ITK_FALSE;
    }
    if (!itk_str_buf_reserve(buf, len)) {
        return ITK_FALSE;
    }
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    buf->data[buf->len] = '\0';
    return ITK_TRUE;
}

ITK_DEF itk_bool itk_str_buf_append_c(itk_str_buf *buf, const char *s)
{
    if (s == NULL) {
        return ITK_TRUE;
    }
    return itk_str_buf_append(buf, s, strlen(s));
}

ITK_DEF itk_utf8_status itk_utf8_validate(itk_str_view v, size_t *offset)
{
    size_t i = 0;

    if (v.len == 0) {
        if (offset != NULL) {
            *offset = (size_t)0;
        }
        return ITK_UTF8_EMPTY;
    }
    while (i < v.len) {
        const unsigned char b0 = (unsigned char)v.data[i];
        unsigned need;   /* continuation bytes expected after the lead */
        unsigned char lo = 0x80u, hi = 0xBFu; /* range of first cont. */
        itk_utf8_status bad = ITK_UTF8_OK;
        size_t j;

        if (b0 < 0x80u) {
            i++;
            continue;
        }
        if (b0 >= 0xC2u && b0 <= 0xDFu) {
            need = 1;
        } else if (b0 == 0xE0u) {
            need = 2; lo = 0xA0u; hi = 0xBFu; bad = ITK_UTF8_OVERLONG;
        } else if (b0 >= 0xE1u && b0 <= 0xECu) {
            need = 2;
        } else if (b0 == 0xEDu) {
            need = 2; lo = 0x80u; hi = 0x9Fu; bad = ITK_UTF8_SURROGATE;
        } else if (b0 >= 0xEEu && b0 <= 0xEFu) {
            need = 2;
        } else if (b0 == 0xF0u) {
            need = 3; lo = 0x90u; hi = 0xBFu; bad = ITK_UTF8_OVERLONG;
        } else if (b0 >= 0xF1u && b0 <= 0xF3u) {
            need = 3;
        } else if (b0 == 0xF4u) {
            need = 3; lo = 0x80u; hi = 0x8Fu; bad = ITK_UTF8_TOO_LARGE;
        } else if (b0 == 0xC0u || b0 == 0xC1u) {
            if (offset != NULL) { *offset = i; }
            return ITK_UTF8_OVERLONG;
        } else { /* 0x80-0xBF stray, or 0xF5-0xFF */
            if (offset != NULL) { *offset = i; }
            return (b0 < 0xC0u) ? ITK_UTF8_BAD_CONT : ITK_UTF8_BAD_LEAD;
        }

        for (j = 1; j <= need; j++) {
            if (i + j >= v.len) {
                if (offset != NULL) { *offset = v.len; }
                return ITK_UTF8_TRUNCATED;
            }
            {
                const unsigned char bj = (unsigned char)v.data[i + j];
                if (bj < 0x80u || bj > 0xBFu) {
                    if (offset != NULL) { *offset = i; }
                    return ITK_UTF8_BAD_CONT;
                }
                if (j == 1 && (bj < lo || bj > hi)) {
                    if (offset != NULL) { *offset = i; }
                    return bad;
                }
            }
        }
        i += need + 1;
    }
    return ITK_UTF8_OK;
}

#endif /* ITK_CSTRING_IMPLEMENTATION */

#endif /* ITK_CSTRING_H */
