/**
 * @file itk_layout.h
 * @brief Struct, union, and bitfield layout computation matching the target
 *        ABI: field offsets, padding, tail padding, and overall
 *        size/alignment. Essential when a managed language must mirror C
 *        records byte-for-byte.
 *
 * @stability stable
 * @depends InteropTk::platform, InteropTk::ctypes
 *
 * Field types must be complete itk_type descriptors (see
 * itk_type_is_complete()); nested records are not modeled in v0.1 — model
 * the inner record first and embed it via a pointer or byte-array field.
 */

#ifndef ITK_LAYOUT_H
#define ITK_LAYOUT_H

#include "itk_platform.h"
#include "itk_ctypes.h"

/* ── public declarations ──────────────────────────────────────────────── */

/** @brief Aggregate flavor being laid out. */
typedef enum itk_record_kind {
    ITK_RECORD_STRUCT = 0, /**< Members at increasing offsets. */
    ITK_RECORD_UNION       /**< All members at offset zero. */
} itk_record_kind;

/** @brief ABI model governing layout decisions. */
typedef enum itk_abi_kind {
    ITK_ABI_GENERIC = 0,   /**< Portable fallback: natural alignment. */
    ITK_ABI_SYSV64,        /**< x86-64 System V (Linux/BSD/macOS). */
    ITK_ABI_WIN64,         /**< x86-64 Windows. */
    ITK_ABI_I386_SYSV,     /**< IA-32 System V. */
    ITK_ABI_I386_WIN32,    /**< IA-32 Windows (stdcall world). */
    ITK_ABI_AARCH64_AAPCS64, /**< Arm 64-bit AAPCS64. */
    ITK_ABI_ARM32_AAPCS    /**< Arm 32-bit AAPCS. */
} itk_abi_kind;

/** @brief Bitfield packing policy selected by the ABI. */
typedef enum itk_bitfield_policy {
    ITK_BITFIELD_SYSV = 0, /**< GCC/SysV: pack at next free bit; a field
                                never crosses a sizeof(T) boundary. */
    ITK_BITFIELD_MSVC      /**< MSVC: same no-crossing rule; units of the
                                declared type tile from offset zero. */
} itk_bitfield_policy;

/** @brief Maximum bitfield width accepted for a 64-bit storage unit. */
#define ITK_BITFIELD_MAX_WIDTH 64u

/**
 * @brief One member of a record, with computed placement.
 *
 * @var itk_field::name
 *      Borrowed field name; may be NULL (positional access only).
 * @var itk_field::type
 *      Field type by value; for bitfields this is the declared storage
 *      type, not the width-narrowed type.
 * @var itk_field::offset
 *      Byte offset of the field's storage; 0 for unions.
 * @var itk_field::is_bitfield
 *      ITK_TRUE when the field carries a width.
 * @var itk_field::bit_width
 *      Width in bits; 0 marks a zero-width aligner (structs only).
 * @var itk_field::bit_offset
 *      Bit index within the storage unit, least-significant-bit first.
 * @var itk_field::unit_size
 *      Size in bytes of the storage unit holding the bitfield
 *      (1, 2, 4, or 8); 0 for non-bitfields.
 */
typedef struct itk_field {
    const char *name;      /**< Borrowed name or NULL. */
    itk_type type;         /**< Declared type. */
    size_t offset;         /**< Computed byte offset. */
    itk_bool is_bitfield;  /**< Width-carrying member. */
    unsigned bit_width;    /**< Bits occupied; 0 for aligners. */
    unsigned bit_offset;   /**< LSB-first bit within the unit. */
    size_t unit_size;      /**< Bitfield storage unit bytes. */
} itk_field;

/**
 * @brief A record under construction, and its sealed result.
 *
 * @var itk_record::kind
 *      Struct or union flavor.
 * @var itk_record::abi
 *      ABI model used when sealing.
 * @var itk_record::fields
 *      Caller-owned writable storage for added members.
 * @var itk_record::field_count
 *      Members added so far.
 * @var itk_record::field_cap
 *      Capacity of @c fields.
 * @var itk_record::size
 *      Computed byte size including tail padding (valid after seal).
 * @var itk_record::align
 *      Computed natural alignment (valid after seal).
 * @var itk_record::sealed
 *      ITK_TRUE once itk_record_seal() succeeded.
 * @var itk_record::error
 *      Static description of the last failure, or NULL.
 */
typedef struct itk_record {
    itk_record_kind kind;   /**< Struct or union. */
    itk_abi_kind abi;      /**< ABI model. */
    itk_field *fields;      /**< Borrowed writable storage. */
    size_t field_count;     /**< Used slots. */
    size_t field_cap;       /**< Total slots. */
    size_t size;            /**< Sealed byte size. */
    size_t align;           /**< Sealed alignment. */
    itk_bool sealed;        /**< Seal state. */
    const char *error;      /**< Last failure text or NULL. */
} itk_record;

/** @brief Alias making builder intent explicit at call sites. */
typedef itk_record itk_record_builder;

/**
 * @brief Select the ABI model matching the compile target.
 * @return An #itk_abi_kind; ITK_ABI_GENERIC when detection is inconclusive.
 * @note Pure function of compile-time macros; safe from any thread.
 */
ITK_DEF itk_abi_kind itk_layout_default_abi(void);

/**
 * @brief Begin building a record.
 * @param b      Builder to initialize; must not be NULL.
 * @param kind   Struct or union.
 * @param abi    ABI model; ITK_ABI_GENERIC selects the portable rules.
 * @param fields Caller-owned array the builder will fill.
 * @param cap    Number of entries in @p fields.
 * @return ITK_TRUE on success; ITK_FALSE when @p b is NULL, @p fields is
 *         NULL, or @p cap is 0 (nothing is initialized then).
 * @note @p fields is borrowed, not copied; it must outlive the record.
 */
ITK_DEF itk_bool itk_record_builder_init(itk_record_builder *b, itk_record_kind kind,
                                 itk_abi_kind abi, itk_field *fields,
                                 size_t cap);

/**
 * @brief Append a non-bitfield member.
 * @param b     Builder; must not be NULL.
 * @param name  Borrowed field name; NULL allowed.
 * @param type  Borrowed complete type descriptor.
 * @return ITK_TRUE on success; ITK_FALSE when full, sealed, or @p type is
 *         NULL/incomplete (error text lands in itk_record::error).
 * @note The descriptor is copied by value; @p name stays borrowed.
 */
ITK_DEF itk_bool itk_record_field(itk_record_builder *b, const char *name,
                          const itk_type *type);

/**
 * @brief Append a bitfield member of declared storage type @p type.
 * @param b      Builder; must not be NULL.
 * @param name   Borrowed field name; NULL allowed.
 * @param type   Borrowed integer storage type (e.g. unsigned int).
 * @param width  Bits occupied; 0 inserts a zero-width aligner (structs
 *               only) that packs the next member to the type boundary.
 * @return ITK_TRUE on success; ITK_FALSE when full, sealed, @p type is not
 *         an integer, or @p width exceeds the type's bit count.
 * @note Placement is computed at seal time per the ABI's policy.
 */
ITK_DEF itk_bool itk_record_bitfield(itk_record_builder *b, const char *name,
                             const itk_type *type, unsigned width);

/**
 * @brief Compute every offset, the size, and the alignment.
 * @param b  Builder; must not be NULL.
 * @return The sealed record (== @p b) on success, NULL on error with
 *         itk_record::error set.
 * @note Idempotent: re-sealing recomputes from scratch.
 */
ITK_DEF const itk_record *itk_record_seal(itk_record_builder *b);

/**
 * @brief Byte offset of the member at @p index.
 * @param r      Sealed record; must not be NULL.
 * @param index  Member position (< itk_record::field_count).
 * @return Field offset, or (size_t)-1 when out of range.
 */
ITK_DEF size_t itk_field_offset(const itk_record *r, size_t index);

/**
 * @brief Byte size of the sealed record, tail padding included.
 * @param r  Sealed record; must not be NULL.
 * @return Record size, or 0 when unsealed.
 */
ITK_DEF size_t itk_record_size(const itk_record *r);

/**
 * @brief Natural alignment of the sealed record.
 * @param r  Sealed record; must not be NULL.
 * @return Alignment in bytes (power of two), or 0 when unsealed.
 */
ITK_DEF size_t itk_record_align(const itk_record *r);

/**
 * @brief Bitfield placement of the member at @p index.
 * @param r          Sealed record; must not be NULL.
 * @param index      Member position.
 * @param width      When non-NULL, receives the width in bits.
 * @param bit_offset When non-NULL, receives the LSB-first bit index
 *                   within the storage unit at itk_field_offset().
 * @param unit_size  When non-NULL, receives the storage-unit byte size.
 * @return ITK_TRUE when @p index is in range and is a bitfield;
 *         ITK_FALSE otherwise (outputs untouched).
 */
ITK_DEF itk_bool itk_field_bitfield_info(const itk_record *r, size_t index,
                                 unsigned *width, unsigned *bit_offset,
                                 size_t *unit_size);

#ifdef ITK_LAYOUT_IMPLEMENTATION
/* ── implementation section ─────────────────────────────────────────── */

ITK_DEF itk_abi_kind itk_layout_default_abi(void)
{
#if defined(ITK_ARCH_X86_64)
#  if defined(ITK_OS_WINDOWS)
    return ITK_ABI_WIN64;
#  else
    return ITK_ABI_SYSV64;
#  endif
#elif defined(ITK_ARCH_X86)
#  if defined(ITK_OS_WINDOWS)
    return ITK_ABI_I386_WIN32;
#  else
    return ITK_ABI_I386_SYSV;
#  endif
#elif defined(ITK_ARCH_AARCH64)
    return ITK_ABI_AARCH64_AAPCS64;
#elif defined(ITK_ARCH_ARM32)
    return ITK_ABI_ARM32_AAPCS;
#else
    return ITK_ABI_GENERIC;
#endif
}

/** Round @p v up to a multiple of @p a (a is a power of two, nonzero). */
static size_t itk_layout_round_up_(size_t v, size_t a)
{
    return (v + (a - 1)) & ~(a - 1);
}

/** The bitfield packing policy an ABI prescribes. */
static itk_bitfield_policy itk_layout_policy_(itk_abi_kind abi)
{
    return (abi == ITK_ABI_WIN64 || abi == ITK_ABI_I386_WIN32)
               ? ITK_BITFIELD_MSVC
               : ITK_BITFIELD_SYSV;
}

ITK_DEF itk_bool itk_record_builder_init(itk_record_builder *b,
                                         itk_record_kind kind,
                                         itk_abi_kind abi, itk_field *fields,
                                         size_t cap)
{
    size_t i;

    if (b == NULL || fields == NULL || cap == 0) {
        return ITK_FALSE;
    }
    b->kind = kind;
    b->abi = abi;
    b->fields = fields;
    b->field_count = (size_t)0;
    b->field_cap = cap;
    b->size = (size_t)0;
    b->align = (size_t)1;
    b->sealed = ITK_FALSE;
    b->error = NULL;
    for (i = 0; i < cap; i++) {
        fields[i].name = NULL;
        fields[i].type = itk_type_prim(ITK_KIND_VOID);
        fields[i].offset = (size_t)0;
        fields[i].is_bitfield = ITK_FALSE;
        fields[i].bit_width = 0u;
        fields[i].bit_offset = 0u;
        fields[i].unit_size = (size_t)0;
    }
    return ITK_TRUE;
}

ITK_DEF itk_bool itk_record_field(itk_record_builder *b, const char *name,
                                  const itk_type *type)
{
    itk_field *f;

    if (b == NULL) {
        return ITK_FALSE;
    }
    if (b->sealed) {
        b->error = "record already sealed";
        return ITK_FALSE;
    }
    if (b->field_count >= b->field_cap) {
        b->error = "field storage full";
        return ITK_FALSE;
    }
    if (type == NULL || !itk_type_is_complete(type)) {
        b->error = "field type is NULL or incomplete";
        return ITK_FALSE;
    }
    f = &b->fields[b->field_count++];
    f->name = name;
    f->type = *type;
    f->is_bitfield = ITK_FALSE;
    f->bit_width = 0u;
    f->bit_offset = 0u;
    f->unit_size = (size_t)0;
    f->offset = (size_t)0;
    return ITK_TRUE;
}

ITK_DEF itk_bool itk_record_bitfield(itk_record_builder *b,
                                     const char *name, const itk_type *type,
                                     unsigned width)
{
    itk_field *f;
    size_t type_bits;

    if (b == NULL) {
        return ITK_FALSE;
    }
    if (b->sealed) {
        b->error = "record already sealed";
        return ITK_FALSE;
    }
    if (b->field_count >= b->field_cap) {
        b->error = "field storage full";
        return ITK_FALSE;
    }
    if (type == NULL || !itk_type_is_integer(type->kind)) {
        b->error = "bitfield storage type must be an integer";
        return ITK_FALSE;
    }
    type_bits = itk_type_size(type) * 8u;
    if (type_bits == 0 || width > type_bits || width > ITK_BITFIELD_MAX_WIDTH) {
        b->error = "bitfield width exceeds storage type";
        return ITK_FALSE;
    }
    if (width == 0 && b->kind == ITK_RECORD_UNION) {
        b->error = "zero-width bitfield is only valid in structs";
        return ITK_FALSE;
    }
    f = &b->fields[b->field_count++];
    f->name = name;
    f->type = *type;
    f->is_bitfield = ITK_TRUE;
    f->bit_width = width;
    f->bit_offset = 0u;
    f->unit_size = itk_type_size(type);
    f->offset = (size_t)0;
    return ITK_TRUE;
}

/** Lay out one bitfield run: mutate bit cursor, set field placement.
 *  Returns ITK_TRUE, or ITK_FALSE with b->error set on overflow. */
static itk_bool itk_layout_bitfield_(itk_record *b, itk_field *f,
                                     size_t *bit_cursor,
                                     itk_bool prev_was_bitfield)
{
    const size_t unit_bytes = itk_type_size(&f->type);
    const size_t unit_bits = unit_bytes * 8u;
    const itk_bitfield_policy pol = itk_layout_policy_(b->abi);

    if (unit_bits == 0) {
        b->error = "bitfield storage type has zero size";
        return ITK_FALSE;
    }

    if (f->bit_width == 0u) {
        /* Zero-width: advance to the next storage-type boundary. */
        *bit_cursor = itk_layout_round_up_(*bit_cursor, unit_bits);
        return ITK_TRUE;
    }

    if (pol == ITK_BITFIELD_MSVC && !prev_was_bitfield &&
        (*bit_cursor % unit_bits) != 0) {
        /* MSVC opens a fresh, unit-aligned run after any non-bitfield. */
        *bit_cursor = itk_layout_round_up_(*bit_cursor, unit_bits);
    }

    {
        const size_t start = *bit_cursor;
        const size_t end = start + (size_t)f->bit_width;

        /* No field may straddle a storage-unit boundary. */
        if (start / unit_bits != (end - 1) / unit_bits) {
            *bit_cursor = itk_layout_round_up_(start, unit_bits);
        }
    }

    f->offset = (*bit_cursor / unit_bits) * unit_bytes;
    f->bit_offset = (unsigned)(*bit_cursor % unit_bits);
    f->unit_size = unit_bytes;
    *bit_cursor += (size_t)f->bit_width;
    return ITK_TRUE;
}

ITK_DEF const itk_record *itk_record_seal(itk_record_builder *b)
{
    size_t i;
    size_t cursor = 0;   /* next free byte (structs) */
    size_t bit_cursor = 0; /* next free bit (bitfield runs) */
    size_t max_align = 1;
    size_t max_end = 0;

    if (b == NULL) {
        return NULL;
    }
    b->error = NULL;
    b->sealed = ITK_FALSE;
    b->size = (size_t)0;
    b->align = (size_t)1;

    if (b->kind == ITK_RECORD_UNION) {
        for (i = 0; i < b->field_count; i++) {
            itk_field *f = &b->fields[i];
            size_t sz, al;

            if (f->is_bitfield) {
                if (!itk_layout_bitfield_(b, f, &bit_cursor, ITK_TRUE)) {
                    return NULL;
                }
                sz = f->unit_size;
                al = itk_type_align(&f->type);
            } else {
                sz = itk_type_size(&f->type);
                al = itk_type_align(&f->type);
            }
            if (sz == 0 || al == 0) {
                b->error = "union member has zero size or alignment";
                return NULL;
            }
            f->offset = (size_t)0;
            if (sz > max_end) {
                max_end = sz;
            }
            if (al > max_align) {
                max_align = al;
            }
            bit_cursor = 0; /* union members restart at bit zero */
        }
        b->align = max_align;
        b->size = itk_layout_round_up_(max_end, max_align);
        b->sealed = ITK_TRUE;
        return b;
    }

    for (i = 0; i < b->field_count; i++) {
        itk_field *f = &b->fields[i];
        const itk_bool prev_was_bit =
            (i > 0) ? b->fields[i - 1].is_bitfield : ITK_TRUE;

        if (f->is_bitfield) {
            if (!itk_layout_bitfield_(b, f, &bit_cursor, prev_was_bit)) {
                return NULL;
            }
            /* Bytes consumed so far by the bit run. */
            cursor = (bit_cursor / 8u) + ((bit_cursor % 8u) ? 1u : 0u);
            {   /* Record alignment includes the storage type's. */
                const size_t al = itk_type_align(&f->type);
                if (al > max_align) {
                    max_align = al;
                }
                if (f->offset + f->unit_size > max_end) {
                    max_end = f->offset + f->unit_size;
                }
            }
        } else {
            const size_t al = itk_type_align(&f->type);
            const size_t sz = itk_type_size(&f->type);

            if (sz == 0 || al == 0) {
                b->error = "field has zero size or alignment";
                return NULL;
            }
            /* A non-bitfield terminates any open bitfield run: the next
             * bitfield (MSVC) or byte cursor resumes after it. */
            cursor = itk_layout_round_up_(cursor, al);
            f->offset = cursor;
            cursor += sz;
            if (cursor > max_end) {
                max_end = cursor;
            }
            if (al > max_align) {
                max_align = al;
            }
            bit_cursor = cursor * 8u;
        }
    }
    b->align = max_align;
    b->size = itk_layout_round_up_(max_end, max_align);
    b->sealed = ITK_TRUE;
    return b;
}

ITK_DEF size_t itk_field_offset(const itk_record *r, size_t index)
{
    if (r == NULL || index >= r->field_count) {
        return (size_t)-1;
    }
    return r->fields[index].offset;
}

ITK_DEF size_t itk_record_size(const itk_record *r)
{
    return (r != NULL && r->sealed) ? r->size : (size_t)0;
}

ITK_DEF size_t itk_record_align(const itk_record *r)
{
    return (r != NULL && r->sealed) ? r->align : (size_t)0;
}

ITK_DEF itk_bool itk_field_bitfield_info(const itk_record *r, size_t index,
                                         unsigned *width,
                                         unsigned *bit_offset,
                                         size_t *unit_size)
{
    const itk_field *f;

    if (r == NULL || index >= r->field_count) {
        return ITK_FALSE;
    }
    f = &r->fields[index];
    if (!f->is_bitfield) {
        return ITK_FALSE;
    }
    if (width != NULL) {
        *width = f->bit_width;
    }
    if (bit_offset != NULL) {
        *bit_offset = f->bit_offset;
    }
    if (unit_size != NULL) {
        *unit_size = f->unit_size;
    }
    return ITK_TRUE;
}

#endif /* ITK_LAYOUT_IMPLEMENTATION */

#endif /* ITK_LAYOUT_H */
