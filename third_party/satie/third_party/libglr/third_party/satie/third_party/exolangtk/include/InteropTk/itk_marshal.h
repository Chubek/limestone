/**
 * @file itk_marshal.h
 * @brief Value marshalling primitives between a host language's
 *        representation and raw C memory: bounds-checked scalar
 *        reads/writes, aggregate copy-in/copy-out, and endian-aware
 *        accessors driven by itk_type descriptors.
 *
 * @stability stable
 * @depends InteropTk::ctypes, InteropTk::layout
 *
 * Every accessor takes the object's address together with its available
 * byte count; nothing is read or written past that bound. Aggregate helpers
 * walk a sealed itk_record, so bitfields are marshalled with the exact
 * unit/bit placement the layout engine computed.
 */

#ifndef ITK_MARSHAL_H
#define ITK_MARSHAL_H

#include "itk_platform.h"
#include "itk_ctypes.h"
#include "itk_layout.h"

/* ── public declarations ──────────────────────────────────────────────── */

/**
 * @brief Universal scalar carrier.
 *
 * Integer reads land zero- or sign-extended in @c s / @c u (same bits);
 * float reads land promoted in @c d or @c ld. Hosts store one of these per
 * value and translate freely.
 *
 * @var itk_scalar::u
 *      Unsigned 64-bit view.
 * @var itk_scalar::s
 *      Signed 64-bit view.
 * @var itk_scalar::d
 *      double view (float values promoted, long double narrowed).
 * @var itk_scalar::ld
 *      long double view.
 */
typedef union itk_scalar {
    uint64_t u;       /**< Unsigned bits. */
    int64_t s;        /**< Signed bits. */
    double d;         /**< Floating value. */
    long double ld;   /**< Extended floating value. */
} itk_scalar;

/**
 * @brief Read a scalar at @p buf in the target's native byte order.
 * @param buf  Object address; must not be NULL.
 * @param len  Bytes available at @p buf (the bounds check).
 * @param t    Scalar type descriptor; must not be NULL.
 * @param out  Receiving carrier; must not be NULL.
 * @return ITK_TRUE on success; ITK_FALSE when anything is NULL, @p t is
 *         not a complete scalar type, or @p len < itk_type_size(t).
 * @note Integers are zero-extended when unsigned and sign-extended when
 *       signed (ITK_KIND_CHAR follows the platform). Reads never write
 *       to @p buf. Safe from any thread.
 */
ITK_DEF itk_bool itk_read_scalar(const void *buf, size_t len, const itk_type *t,
                         itk_scalar *out);

/**
 * @brief Write a scalar at @p buf in the target's native byte order.
 * @param buf    Object address; must not be NULL.
 * @param len    Bytes available at @p buf.
 * @param t      Scalar type descriptor; must not be NULL.
 * @param value  Carrier whose low bits (or @c d/@c ld) are stored.
 * @return ITK_TRUE on success; ITK_FALSE on the same conditions as
 *         itk_read_scalar().
 * @note Narrow integers are truncated to the type's width; floats are
 *       narrowed on store. Not a synchronization primitive.
 */
ITK_DEF itk_bool itk_write_scalar(void *buf, size_t len, const itk_type *t,
                          itk_scalar value);

/**
 * @brief Read a scalar given as explicit little-endian bytes.
 * @param buf  Object address; must not be NULL.
 * @param len  Bytes available at @p buf.
 * @param t    Scalar type descriptor; must not be NULL.
 * @param out  Receiving carrier.
 * @return ITK_TRUE on success; ITK_FALSE on invalid arguments.
 * @note Bytes are assembled LSB-first regardless of host byte order.
 */
ITK_DEF itk_bool itk_read_scalar_le(const void *buf, size_t len, const itk_type *t,
                            itk_scalar *out);

/**
 * @brief Read a scalar given as explicit big-endian bytes.
 * @param buf  Object address; must not be NULL.
 * @param len  Bytes available at @p buf.
 * @param t    Scalar type descriptor; must not be NULL.
 * @param out  Receiving carrier.
 * @return ITK_TRUE on success; ITK_FALSE on invalid arguments.
 * @note Bytes are assembled MSB-first regardless of host byte order.
 */
ITK_DEF itk_bool itk_read_scalar_be(const void *buf, size_t len, const itk_type *t,
                            itk_scalar *out);

/**
 * @brief Write a scalar as explicit little-endian bytes.
 * @param buf    Object address; must not be NULL.
 * @param len    Bytes available at @p buf.
 * @param t      Scalar type descriptor; must not be NULL.
 * @param value  Carrier to store.
 * @return ITK_TRUE on success; ITK_FALSE on invalid arguments.
 */
ITK_DEF itk_bool itk_write_scalar_le(void *buf, size_t len, const itk_type *t,
                             itk_scalar value);

/**
 * @brief Write a scalar as explicit big-endian bytes.
 * @param buf    Object address; must not be NULL.
 * @param len    Bytes available at @p buf.
 * @param t      Scalar type descriptor; must not be NULL.
 * @param value  Carrier to store.
 * @return ITK_TRUE on success; ITK_FALSE on invalid arguments.
 */
ITK_DEF itk_bool itk_write_scalar_be(void *buf, size_t len, const itk_type *t,
                             itk_scalar value);

/**
 * @brief Read one member of a sealed record, bitfields included.
 * @param r     Sealed record; must not be NULL.
 * @param index Member position (< itk_record::field_count).
 * @param data  Record storage of at least itk_record_size(r) bytes.
 * @param out   Receiving carrier.
 * @return ITK_TRUE on success; ITK_FALSE when the member is not a scalar
 *         (array/function) or any pointer is out of range.
 * @note Bitfields are extracted from their storage unit with the declared
 *       type's signedness. Read-only with respect to @p data.
 */
ITK_DEF itk_bool itk_record_read_field(const itk_record *r, size_t index,
                               const void *data, itk_scalar *out);

/**
 * @brief Write one member of a sealed record, bitfields included.
 * @param r      Sealed record; must not be NULL.
 * @param index  Member position.
 * @param data   Record storage of at least itk_record_size(r) bytes.
 * @param value  Carrier to store.
 * @return ITK_TRUE on success; ITK_FALSE when the member is not a scalar.
 * @note Bitfield writes are read-modify-write on the enclosing unit; bits
 *       outside the field are preserved.
 */
ITK_DEF itk_bool itk_record_write_field(const itk_record *r, size_t index,
                                void *data, itk_scalar value);

/**
 * @brief Copy-in: store an array of host scalars into a record buffer.
 * @param r       Sealed record whose scalar members are written in order.
 * @param data    Record storage; must not be NULL.
 * @param values  Carrier array of at least itk_record::field_count entries;
 *                must not be NULL.
 * @param written When non-NULL, receives how many members were stored.
 * @return ITK_TRUE when every member is scalar and was stored;
 *         ITK_FALSE when a non-scalar member is met (earlier members
 *         remain written; @p written reports the count).
 * @note Pure memory traffic; no allocation, no global state.
 */
ITK_DEF itk_bool itk_marshal_record(const itk_record *r, void *data,
                            const itk_scalar *values, size_t *written);

/**
 * @brief Copy-out: load a record buffer into an array of host scalars.
 * @param r       Sealed record whose scalar members are read in order.
 * @param data    Record storage; must not be NULL.
 * @param values  Carrier array of at least itk_record::field_count entries;
 *                must not be NULL.
 * @param read    When non-NULL, receives how many members were loaded.
 * @return ITK_TRUE when every member is scalar and was loaded;
 *         ITK_FALSE when a non-scalar member is met.
 */
ITK_DEF itk_bool itk_unmarshal_record(const itk_record *r, const void *data,
                              itk_scalar *values, size_t *read);

#ifdef ITK_MARSHAL_IMPLEMENTATION
/* ── implementation section ─────────────────────────────────────────── */

#include <string.h>

/** Load the low @p n (1..8) bytes at @p p into a u64, LSB-first. */
static uint64_t itk_marshal_load_le_(const unsigned char *p, unsigned n)
{
    uint64_t v = 0;
    unsigned i;

    for (i = 0; i < n; i++) {
        v |= ((uint64_t)p[i]) << (8u * i);
    }
    return v;
}

/** Load the low @p n (1..8) bytes at @p p into a u64, MSB-first. */
static uint64_t itk_marshal_load_be_(const unsigned char *p, unsigned n)
{
    uint64_t v = 0;
    unsigned i;

    for (i = 0; i < n; i++) {
        v = (v << 8) | (uint64_t)p[i];
    }
    return v;
}

/** Store the low @p n bytes of @p v at @p p, LSB-first. */
static void itk_marshal_store_le_(unsigned char *p, unsigned n, uint64_t v)
{
    unsigned i;

    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)((v >> (8u * i)) & 0xffu);
    }
}

/** Store the low @p n bytes of @p v at @p p, MSB-first. */
static void itk_marshal_store_be_(unsigned char *p, unsigned n, uint64_t v)
{
    unsigned i;

    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)((v >> (8u * (n - 1 - i))) & 0xffu);
    }
}

/** Sign-extend the low @p bits of @p v. */
static uint64_t itk_marshal_sext_(uint64_t v, unsigned bits)
{
    if (bits == 0 || bits >= 64) {
        return v;
    }
    const uint64_t sign = (uint64_t)1 << (bits - 1);

    if ((v & sign) != 0) {
        return v | (~((uint64_t)0) << bits);
    }
    return v & (((uint64_t)1 << bits) - 1);
}

/** Validate a scalar descriptor; returns its byte size or 0. */
static size_t itk_marshal_scalar_size_(const itk_type *t)
{
    if (t == NULL) {
        return 0;
    }
    switch (t->kind) {
    case ITK_KIND_VOID:
    case ITK_KIND_ARRAY:
    case ITK_KIND_FUNC:
        return 0;
    case ITK_KIND_PTR:
        return sizeof(void *);
    default:
        return itk_type_size(t);
    }
}

/** Interpret raw bits as a carrier per the type's class/signedness. */
static void itk_marshal_lift_(const itk_type *t, uint64_t raw,
                              itk_scalar *out)
{
    if (itk_type_is_float(t->kind)) {
        switch (t->kind) {
        case ITK_KIND_FLOAT: {
            float f;
            memcpy(&f, &raw, sizeof(f));
            out->d = (double)f;
            return;
        }
        case ITK_KIND_DOUBLE:
            memcpy(&out->d, &raw, sizeof(out->d));
            return;
        default: /* LDOUBLE: raw accessors carry only 64 bits. */
            out->ld = (long double)(double)0.0;
            memcpy(&out->ld, &raw,
                   sizeof(raw) < sizeof(out->ld) ? sizeof(raw)
                                                 : sizeof(out->ld));
            return;
        }
    }
    if (itk_type_is_signed(t->kind)) {
        out->s = (int64_t)itk_marshal_sext_(raw,
                                             (unsigned)(itk_type_size(t) * 8u));
    } else {
        out->u = raw;
    }
}

/** Lower a carrier to raw bits per the type's class and width. */
static uint64_t itk_marshal_lower_(const itk_type *t, itk_scalar v)
{
    if (itk_type_is_float(t->kind)) {
        switch (t->kind) {
        case ITK_KIND_FLOAT: {
            float f = (float)v.d;
            uint32_t raw = 0;
            memcpy(&raw, &f, sizeof(raw));
            return (uint64_t)raw;
        }
        case ITK_KIND_DOUBLE: {
            uint64_t raw = 0;
            memcpy(&raw, &v.d, sizeof(raw));
            return raw;
        }
        default: { /* LDOUBLE */
            uint64_t raw = 0;
            memcpy(&raw, &v.ld, sizeof(raw)); /* low bytes; best effort */
            return raw;
        }
        }
    }
    return v.u;
}

/** Shared LE read: assemble bytes, lift into the carrier. */
static itk_bool itk_marshal_read_le_(const void *buf, size_t len,
                                     const itk_type *t, itk_scalar *out)
{
    const size_t sz = itk_marshal_scalar_size_(t);

    if (buf == NULL || t == NULL || out == NULL || sz == 0 || len < sz) {
        return ITK_FALSE;
    }
    itk_marshal_lift_(t, itk_marshal_load_le_((const unsigned char *)buf,
                                              (unsigned)sz),
                      out);
    return ITK_TRUE;
}

/** Shared BE read. */
static itk_bool itk_marshal_read_be_(const void *buf, size_t len,
                                     const itk_type *t, itk_scalar *out)
{
    const size_t sz = itk_marshal_scalar_size_(t);

    if (buf == NULL || t == NULL || out == NULL || sz == 0 || len < sz) {
        return ITK_FALSE;
    }
    itk_marshal_lift_(t, itk_marshal_load_be_((const unsigned char *)buf,
                                              (unsigned)sz),
                      out);
    return ITK_TRUE;
}

/** Shared LE/BE write. */
static itk_bool itk_marshal_write_(void *buf, size_t len, const itk_type *t,
                                   itk_scalar v, int big_endian)
{
    const size_t sz = itk_marshal_scalar_size_(t);

    if (buf == NULL || t == NULL || sz == 0 || len < sz) {
        return ITK_FALSE;
    }
    if (big_endian) {
        itk_marshal_store_be_((unsigned char *)buf, (unsigned)sz,
                              itk_marshal_lower_(t, v));
    } else {
        itk_marshal_store_le_((unsigned char *)buf, (unsigned)sz,
                              itk_marshal_lower_(t, v));
    }
    return ITK_TRUE;
}

ITK_DEF itk_bool itk_read_scalar_le(const void *buf, size_t len,
                                    const itk_type *t, itk_scalar *out)
{
    return itk_marshal_read_le_(buf, len, t, out);
}

ITK_DEF itk_bool itk_read_scalar_be(const void *buf, size_t len,
                                    const itk_type *t, itk_scalar *out)
{
    return itk_marshal_read_be_(buf, len, t, out);
}

ITK_DEF itk_bool itk_write_scalar_le(void *buf, size_t len,
                                     const itk_type *t, itk_scalar v)
{
    return itk_marshal_write_(buf, len, t, v, 0);
}

ITK_DEF itk_bool itk_write_scalar_be(void *buf, size_t len,
                                     const itk_type *t, itk_scalar v)
{
    return itk_marshal_write_(buf, len, t, v, 1);
}

ITK_DEF itk_bool itk_read_scalar(const void *buf, size_t len,
                                 const itk_type *t, itk_scalar *out)
{
#if defined(ITK_BIG_ENDIAN)
    return itk_marshal_read_be_(buf, len, t, out);
#else
    return itk_marshal_read_le_(buf, len, t, out);
#endif
}

ITK_DEF itk_bool itk_write_scalar(void *buf, size_t len, const itk_type *t,
                                  itk_scalar v)
{
#if defined(ITK_BIG_ENDIAN)
    return itk_marshal_write_(buf, len, t, v, 1);
#else
    return itk_marshal_write_(buf, len, t, v, 0);
#endif
}

/** Load a 1/2/4/8-byte storage unit natively (endian-neutral via memcpy). */
static uint64_t itk_marshal_unit_load_(const void *unit, size_t unit_size)
{
    switch (unit_size) {
    case 1: {
        uint8_t v;
        memcpy(&v, unit, sizeof(v));
        return (uint64_t)v;
    }
    case 2: {
        uint16_t v;
        memcpy(&v, unit, sizeof(v));
        return (uint64_t)v;
    }
    case 4: {
        uint32_t v;
        memcpy(&v, unit, sizeof(v));
        return (uint64_t)v;
    }
    default: {
        uint64_t v = 0;
        memcpy(&v, unit, sizeof(v));
        return v;
    }
    }
}

/** Store a 1/2/4/8-byte storage unit natively. */
static void itk_marshal_unit_store_(void *unit, size_t unit_size,
                                    uint64_t v)
{
    switch (unit_size) {
    case 1: {
        uint8_t b = (uint8_t)v;
        memcpy(unit, &b, sizeof(b));
        break;
    }
    case 2: {
        uint16_t h = (uint16_t)v;
        memcpy(unit, &h, sizeof(h));
        break;
    }
    case 4: {
        uint32_t w = (uint32_t)v;
        memcpy(unit, &w, sizeof(w));
        break;
    }
    default:
        memcpy(unit, &v, sizeof(v));
        break;
    }
}

ITK_DEF itk_bool itk_record_read_field(const itk_record *r, size_t index,
                                       const void *data, itk_scalar *out)
{
    const itk_field *f;
    uint64_t raw;

    if (r == NULL || data == NULL || out == NULL || index >= r->field_count) {
        return ITK_FALSE;
    }
    f = &r->fields[index];

    if (f->is_bitfield) {
        const unsigned width = f->bit_width;
        const uint64_t mask =
            (width >= 64) ? ~(uint64_t)0 : (((uint64_t)1 << width) - 1);
        uint64_t unit;

        if (f->bit_width == 0u) {
            return ITK_FALSE; /* aligner pseudo-field */
        }
        unit = itk_marshal_unit_load_((const unsigned char *)data + f->offset,
                                      f->unit_size);
        raw = (unit >> f->bit_offset) & mask;
        if (itk_type_is_signed(f->type.kind)) {
            raw = itk_marshal_sext_(raw, width);
            out->s = (int64_t)raw;
        } else {
            out->u = raw;
        }
        return ITK_TRUE;
    }

    /* Non-bitfield: scalar sizes only. */
    if (f->type.kind == ITK_KIND_ARRAY || f->type.kind == ITK_KIND_FUNC ||
        f->type.kind == ITK_KIND_VOID) {
        return ITK_FALSE;
    }
    {
        const size_t sz = itk_marshal_scalar_size_(&f->type);
        const unsigned char *src = (const unsigned char *)data + f->offset;

#if defined(ITK_BIG_ENDIAN)
        raw = itk_marshal_load_be_(src, (unsigned)sz);
#else
        raw = itk_marshal_load_le_(src, (unsigned)sz);
#endif
    }
    itk_marshal_lift_(&f->type, raw, out);
    return ITK_TRUE;
}

ITK_DEF itk_bool itk_record_write_field(const itk_record *r, size_t index,
                                        void *data, itk_scalar value)
{
    const itk_field *f;

    if (r == NULL || data == NULL || index >= r->field_count) {
        return ITK_FALSE;
    }
    f = &r->fields[index];

    if (f->is_bitfield) {
        const unsigned width = f->bit_width;
        const uint64_t mask =
            (width >= 64) ? ~(uint64_t)0 : (((uint64_t)1 << width) - 1);
        uint64_t unit, raw;

        if (width == 0u) {
            return ITK_FALSE;
        }
        raw = itk_marshal_lower_(&f->type, value) & mask;
        unit = itk_marshal_unit_load_((unsigned char *)data + f->offset,
                                      f->unit_size);
        unit &= ~(mask << f->bit_offset);
        unit |= raw << f->bit_offset;
        itk_marshal_unit_store_((unsigned char *)data + f->offset,
                                f->unit_size, unit);
        return ITK_TRUE;
    }

    if (f->type.kind == ITK_KIND_ARRAY || f->type.kind == ITK_KIND_FUNC ||
        f->type.kind == ITK_KIND_VOID) {
        return ITK_FALSE;
    }
    {
        const size_t sz = itk_marshal_scalar_size_(&f->type);
        const uint64_t raw = itk_marshal_lower_(&f->type, value);
        unsigned char *dst = (unsigned char *)data + f->offset;

#if defined(ITK_BIG_ENDIAN)
        itk_marshal_store_be_(dst, (unsigned)sz, raw);
#else
        itk_marshal_store_le_(dst, (unsigned)sz, raw);
#endif
    }
    return ITK_TRUE;
}

ITK_DEF itk_bool itk_marshal_record(const itk_record *r, void *data,
                                    const itk_scalar *values,
                                    size_t *written)
{
    size_t i;

    if (r == NULL || data == NULL || values == NULL) {
        if (written != NULL) {
            *written = 0;
        }
        return ITK_FALSE;
    }
    for (i = 0; i < r->field_count; i++) {
        if (!itk_record_write_field(r, i, data, values[i])) {
            if (written != NULL) {
                *written = i;
            }
            return ITK_FALSE;
        }
    }
    if (written != NULL) {
        *written = i;
    }
    return ITK_TRUE;
}

ITK_DEF itk_bool itk_unmarshal_record(const itk_record *r, const void *data,
                                      itk_scalar *values, size_t *read)
{
    size_t i;

    if (r == NULL || data == NULL || values == NULL) {
        if (read != NULL) {
            *read = 0;
        }
        return ITK_FALSE;
    }
    for (i = 0; i < r->field_count; i++) {
        if (!itk_record_read_field(r, i, data, &values[i])) {
            if (read != NULL) {
                *read = i;
            }
            return ITK_FALSE;
        }
    }
    if (read != NULL) {
        *read = i;
    }
    return ITK_TRUE;
}

#endif /* ITK_MARSHAL_IMPLEMENTATION */

#endif /* ITK_MARSHAL_H */
