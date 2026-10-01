/**
 * @file itk_ctypes.h
 * @brief Canonical model of the C type system: primitive kinds, size,
 *        alignment, signedness, qualifiers, pointers, arrays, functions.
 *        Gives compiler and interpreter authors one authoritative table
 *        instead of scattered sizeof() assumptions.
 *
 * @stability stable
 * @depends InteropTk::platform
 *
 * Record (struct/union) types are deliberately not modeled here: they live
 * in InteropTk::layout, which builds on this module. Derived-type builders
 * borrow the types they reference — the callee never copies or owns them.
 */

#ifndef ITK_CTYPES_H
#define ITK_CTYPES_H

#include "itk_platform.h"

/* ── public declarations ──────────────────────────────────────────────── */

/**
 * @name Type-qualifier bits
 * @brief OR-ed into #itk_type::quals. Two types differing only in
 *        qualifiers are not itk_type_equal().
 * @{ */
#define ITK_QUAL_CONST    0x1u /**< const-qualified. */
#define ITK_QUAL_VOLATILE 0x2u /**< volatile-qualified. */
#define ITK_QUAL_RESTRICT 0x4u /**< restrict-qualified. */
/** @} */

/** @brief Every kind of type this module models. */
typedef enum itk_type_kind {
    ITK_KIND_VOID = 0,  /**< void (incomplete as a value type). */
    ITK_KIND_BOOL,      /**< _Bool / bool. */
    ITK_KIND_CHAR,      /**< char (platform signedness). */
    ITK_KIND_SCHAR,     /**< signed char. */
    ITK_KIND_UCHAR,     /**< unsigned char. */
    ITK_KIND_SHORT,     /**< short. */
    ITK_KIND_USHORT,    /**< unsigned short. */
    ITK_KIND_INT,       /**< int. */
    ITK_KIND_UINT,      /**< unsigned int. */
    ITK_KIND_LONG,      /**< long. */
    ITK_KIND_ULONG,     /**< unsigned long. */
    ITK_KIND_LLONG,     /**< long long. */
    ITK_KIND_ULLONG,    /**< unsigned long long. */
    ITK_KIND_I8,        /**< int8_t. */
    ITK_KIND_I16,       /**< int16_t. */
    ITK_KIND_I32,       /**< int32_t. */
    ITK_KIND_I64,       /**< int64_t. */
    ITK_KIND_U8,        /**< uint8_t. */
    ITK_KIND_U16,       /**< uint16_t. */
    ITK_KIND_U32,       /**< uint32_t. */
    ITK_KIND_U64,       /**< uint64_t. */
    ITK_KIND_FLOAT,     /**< float. */
    ITK_KIND_DOUBLE,    /**< double. */
    ITK_KIND_LDOUBLE,   /**< long double. */
    ITK_KIND_SIZE,      /**< size_t. */
    ITK_KIND_PTRDIFF,   /**< ptrdiff_t. */
    ITK_KIND_INTPTR,    /**< intptr_t. */
    ITK_KIND_UINTPTR,   /**< uintptr_t. */
    ITK_KIND_ENUM,      /**< Unscoped enum; int-sized and signed. */
    ITK_KIND_PTR,       /**< Pointer to #itk_type::child. */
    ITK_KIND_ARRAY,     /**< Array of #itk_type::child, #itk_type::length. */
    ITK_KIND_FUNC,      /**< Function type (parameters borrowed). */
    ITK_KIND_COUNT      /**< Sentinel: number of kinds. */
} itk_type_kind;

/**
 * @brief Canonical C type descriptor.
 *
 * Instances are plain values: copyable, stashable in arrays, comparable
 * with itk_type_equal(). Derived kinds reference their constituents by
 * borrowed pointer — the referenced types must outlive the descriptor.
 *
 * @var itk_type::kind
 *      Discriminant from #itk_type_kind.
 * @var itk_type::quals
 *      OR of #ITK_QUAL_CONST, #ITK_QUAL_VOLATILE, #ITK_QUAL_RESTRICT.
 * @var itk_type::child
 *      Pointee for pointers, element for arrays, return type for
 *      functions; NULL otherwise.
 * @var itk_type::length
 *      Element count for arrays; 0 marks an incomplete array @c T[].
 * @var itk_type::params
 *      Borrowed array of parameter descriptors for functions.
 * @var itk_type::param_count
 *      Number of entries in @c params (excluding the ellipsis).
 * @var itk_type::variadic
 *      ITK_TRUE when the function accepts trailing @c ... arguments.
 */
typedef struct itk_type {
    itk_type_kind kind;          /**< Discriminant. */
    unsigned quals;              /**< Qualifier bits. */
    const struct itk_type *child;/**< Pointee / element / return. */
    size_t length;               /**< Array element count (0 = unsized). */
    const struct itk_type *const *params; /**< Borrowed parameter list. */
    size_t param_count;          /**< Parameter count. */
    itk_bool variadic;           /**< Trailing-ellipsis flag. */
} itk_type;

/**
 * @brief Build a primitive-type descriptor.
 * @param kind  Any primitive kind (VOID..ENUM).
 * @return A zero-initialized descriptor carrying only @p kind.
 * @note Derived fields are left NULL/0; use the *_of() builders instead.
 */
static inline itk_type itk_type_prim(itk_type_kind kind)
{
    itk_type t;
    t.kind = kind;
    t.quals = 0u;
    t.child = NULL;
    t.length = (size_t)0;
    t.params = NULL;
    t.param_count = (size_t)0;
    t.variadic = ITK_FALSE;
    return t;
}

/**
 * @brief Build a pointer-to-@p pointee descriptor.
 * @param pointee  Borrowed referent; must not be NULL.
 * @return ITK_KIND_PTR descriptor whose size/alignment match void *.
 * @note @p pointee is borrowed for the lifetime of the descriptor.
 */
static inline itk_type itk_type_ptr_to(const itk_type *pointee)
{
    itk_type t = itk_type_prim(ITK_KIND_PTR);
    t.child = pointee;
    return t;
}

/**
 * @brief Build an array-of-@p element descriptor.
 * @param element  Borrowed element type; must not be NULL.
 * @param count    Element count; 0 produces the incomplete @c T[] form.
 * @return ITK_KIND_ARRAY descriptor.
 * @note @p element is borrowed; array stride is itk_type_align(element).
 */
static inline itk_type itk_type_array_of(const itk_type *element,
                                         size_t count)
{
    itk_type t = itk_type_prim(ITK_KIND_ARRAY);
    t.child = element;
    t.length = count;
    return t;
}

/**
 * @brief Build a function-type descriptor.
 * @param ret          Borrowed return type; must not be NULL.
 * @param params       Borrowed array of parameter descriptors (may be NULL
 *                     when @p count is 0).
 * @param count        Parameter count (ellipsis excluded).
 * @param variadic     ITK_TRUE appends a trailing @c ... after @p count
 *                     fixed parameters.
 * @return ITK_KIND_FUNC descriptor (incomplete: size 0, like C itself).
 * @note @p ret and @p params are borrowed, never copied.
 */
static inline itk_type itk_type_func(const itk_type *ret,
                                     const itk_type *const *params,
                                     size_t count, itk_bool variadic)
{
    itk_type t = itk_type_prim(ITK_KIND_FUNC);
    t.child = ret;
    t.params = params;
    t.param_count = count;
    t.variadic = variadic;
    return t;
}

/**
 * @brief Re-qualify a descriptor (e.g. add const).
 * @param t      Base descriptor.
 * @param quals  New qualifier bitmask, replacing the old.
 * @return A copy of @p t with @c quals set.
 */
static inline itk_type itk_type_qualify(itk_type t, unsigned quals)
{
    t.quals = quals;
    return t;
}

/**
 * @brief Size in bytes of @p t on the target platform.
 * @param t  Descriptor; must not be NULL.
 * @return Byte size, or 0 for incomplete types (void, functions,
 *         zero-length arrays).
 * @note Pointers report sizeof(void *); arrays report
 *       length * itk_type_size(child).
 */
ITK_DEF size_t itk_type_size(const itk_type *t);

/**
 * @brief Natural alignment in bytes of @p t on the target platform.
 * @param t  Descriptor; must not be NULL.
 * @return Alignment (a power of two), or 0 for incomplete types.
 */
ITK_DEF size_t itk_type_align(const itk_type *t);

/**
 * @brief Deep structural equality, qualifiers included.
 * @param a  Left operand; must not be NULL.
 * @param b  Right operand; must not be NULL.
 * @return ITK_TRUE when the descriptors describe the same C type.
 * @note Incomplete arrays compare equal regardless of length only when
 *       both are incomplete; otherwise lengths must match.
 */
ITK_DEF itk_bool itk_type_equal(const itk_type *a, const itk_type *b);

/**
 * @brief Whether @p t can hold an integral value.
 * @param kind  Kind to test.
 * @return ITK_TRUE for every integer/enum/bool kind.
 */
ITK_DEF itk_bool itk_type_is_integer(itk_type_kind kind);

/**
 * @brief Whether @p t is a floating-point kind.
 * @param kind  Kind to test.
 * @return ITK_TRUE for FLOAT, DOUBLE, LDOUBLE.
 */
ITK_DEF itk_bool itk_type_is_float(itk_type_kind kind);

/**
 * @brief Signedness of an integer kind.
 * @param kind  Integer kind to test.
 * @return ITK_TRUE for signed kinds; ITK_FALSE for unsigned and
 *         non-integers. ITK_KIND_CHAR follows the platform char
 *         signedness (see itk_char_is_signed()).
 */
ITK_DEF itk_bool itk_type_is_signed(itk_type_kind kind);

/**
 * @brief Whether @p t has a computable size on the target.
 * @param t  Descriptor; must not be NULL.
 * @return ITK_FALSE for void, functions, and @c T[] arrays.
 */
ITK_DEF itk_bool itk_type_is_complete(const itk_type *t);

/**
 * @brief Canonical spelling of a kind, usable in diagnostics.
 * @param kind  Kind to name.
 * @return Static lowercase string ("unsigned long long", "ptr", ...);
 *         "?kind" for out-of-range values.
 * @note Points at immutable storage; never freed.
 */
ITK_DEF const char *itk_type_kind_name(itk_type_kind kind);

/**
 * @brief Whether plain @c char is signed on the target.
 * @return ITK_TRUE on targets where char ranges [-128,127].
 * @note Pure function of compile-time detection; safe from any thread.
 */
ITK_DEF itk_bool itk_char_is_signed(void);

#ifdef ITK_CTYPES_IMPLEMENTATION
/* ── implementation section ─────────────────────────────────────────── */

ITK_DEF itk_bool itk_type_is_integer(itk_type_kind kind)
{
    switch (kind) {
    case ITK_KIND_BOOL:
    case ITK_KIND_CHAR: case ITK_KIND_SCHAR: case ITK_KIND_UCHAR:
    case ITK_KIND_SHORT: case ITK_KIND_USHORT:
    case ITK_KIND_INT: case ITK_KIND_UINT:
    case ITK_KIND_LONG: case ITK_KIND_ULONG:
    case ITK_KIND_LLONG: case ITK_KIND_ULLONG:
    case ITK_KIND_I8: case ITK_KIND_I16:
    case ITK_KIND_I32: case ITK_KIND_I64:
    case ITK_KIND_U8: case ITK_KIND_U16:
    case ITK_KIND_U32: case ITK_KIND_U64:
    case ITK_KIND_SIZE: case ITK_KIND_PTRDIFF:
    case ITK_KIND_INTPTR: case ITK_KIND_UINTPTR:
    case ITK_KIND_ENUM:
        return ITK_TRUE;
    default:
        return ITK_FALSE;
    }
}

ITK_DEF itk_bool itk_type_is_float(itk_type_kind kind)
{
    return (kind == ITK_KIND_FLOAT || kind == ITK_KIND_DOUBLE ||
            kind == ITK_KIND_LDOUBLE)
               ? ITK_TRUE
               : ITK_FALSE;
}

ITK_DEF itk_bool itk_type_is_signed(itk_type_kind kind)
{
    switch (kind) {
    case ITK_KIND_UCHAR: case ITK_KIND_USHORT: case ITK_KIND_UINT:
    case ITK_KIND_ULONG: case ITK_KIND_ULLONG:
    case ITK_KIND_U8: case ITK_KIND_U16:
    case ITK_KIND_U32: case ITK_KIND_U64:
    case ITK_KIND_UINTPTR: case ITK_KIND_SIZE:
    case ITK_KIND_BOOL:
        return ITK_FALSE;
    case ITK_KIND_CHAR:
        return itk_char_is_signed();
    default:
        return itk_type_is_integer(kind);
    }
}

ITK_DEF itk_bool itk_char_is_signed(void)
{
#if defined(__CHAR_UNSIGNED__) || ('x' > 127)
    return ITK_FALSE;
#else
    return ITK_TRUE;
#endif
}

/** Alignment-of idiom for C99: the offset a compiler would give the member
 *  when packed after a char is the type's natural alignment. */
#define itk_alignof_(ct) \
    ((size_t) & (((struct { char itk_pad_; ct itk_m_; } *)0)->itk_m_))

/** Primitive size/align via the one true table: the compiler itself. */
static void itk_prim_layout_(itk_type_kind kind, size_t *sz, size_t *al)
{
#define ITK_CASE(k, ct)                                                     \
    case ITK_KIND_##k:                                                      \
        *sz = sizeof(ct);                                                   \
        *al = itk_alignof_(ct);                                             \
        break
    switch (kind) {
    ITK_CASE(BOOL, itk_bool);
    ITK_CASE(CHAR, char);
    ITK_CASE(SCHAR, signed char);
    ITK_CASE(UCHAR, unsigned char);
    ITK_CASE(SHORT, short);
    ITK_CASE(USHORT, unsigned short);
    ITK_CASE(INT, int);
    ITK_CASE(UINT, unsigned int);
    ITK_CASE(LONG, long);
    ITK_CASE(ULONG, unsigned long);
    ITK_CASE(LLONG, long long);
    ITK_CASE(ULLONG, unsigned long long);
    ITK_CASE(I8, int8_t);
    ITK_CASE(I16, int16_t);
    ITK_CASE(I32, int32_t);
    ITK_CASE(I64, int64_t);
    ITK_CASE(U8, uint8_t);
    ITK_CASE(U16, uint16_t);
    ITK_CASE(U32, uint32_t);
    ITK_CASE(U64, uint64_t);
    ITK_CASE(FLOAT, float);
    ITK_CASE(DOUBLE, double);
    ITK_CASE(LDOUBLE, long double);
    ITK_CASE(SIZE, size_t);
    ITK_CASE(PTRDIFF, ptrdiff_t);
    ITK_CASE(INTPTR, intptr_t);
    ITK_CASE(UINTPTR, uintptr_t);
    ITK_CASE(ENUM, int);
    default:
        *sz = 0;
        *al = 0;
        break;
    }
#undef ITK_CASE
#undef itk_alignof_
}

ITK_DEF size_t itk_type_size(const itk_type *t)
{
    size_t sz = 0, al = 0;

    if (t == NULL) {
        return 0;
    }
    switch (t->kind) {
    case ITK_KIND_PTR:
        return sizeof(void *);
    case ITK_KIND_ARRAY:
        if (t->child == NULL || t->length == 0) {
            return 0; /* incomplete */
        }
        {
            const size_t elem = itk_type_size(t->child);
            if (elem == 0) {
                return 0;
            }
            if (t->length > (size_t)-1 / elem) {
                return 0; /* would overflow */
            }
            return t->length * elem;
        }
    case ITK_KIND_FUNC:
        return 0;
    case ITK_KIND_VOID:
        return 0;
    default:
        itk_prim_layout_(t->kind, &sz, &al);
        return sz;
    }
}

ITK_DEF size_t itk_type_align(const itk_type *t)
{
    size_t sz = 0, al = 0;

    if (t == NULL) {
        return 0;
    }
    switch (t->kind) {
    case ITK_KIND_PTR:
        return sizeof(void *);
    case ITK_KIND_ARRAY:
        return (t->child != NULL) ? itk_type_align(t->child) : 0;
    case ITK_KIND_FUNC:
    case ITK_KIND_VOID:
        return 0;
    default:
        itk_prim_layout_(t->kind, &sz, &al);
        return al;
    }
}

ITK_DEF itk_bool itk_type_is_complete(const itk_type *t)
{
    if (t == NULL) {
        return ITK_FALSE;
    }
    if (t->kind == ITK_KIND_VOID || t->kind == ITK_KIND_FUNC) {
        return ITK_FALSE;
    }
    if (t->kind == ITK_KIND_ARRAY) {
        return (t->child != NULL && t->length > 0) ? ITK_TRUE : ITK_FALSE;
    }
    return ITK_TRUE;
}

ITK_DEF itk_bool itk_type_equal(const itk_type *a, const itk_type *b)
{
    size_t i;

    if (a == NULL || b == NULL) {
        return ITK_FALSE;
    }
    if (a->kind != b->kind || a->quals != b->quals) {
        return ITK_FALSE;
    }
    switch (a->kind) {
    case ITK_KIND_PTR:
        return itk_type_equal(a->child, b->child);
    case ITK_KIND_ARRAY:
        if (a->length != b->length) {
            return ITK_FALSE;
        }
        return itk_type_equal(a->child, b->child);
    case ITK_KIND_FUNC:
        if (a->param_count != b->param_count ||
            a->variadic != b->variadic) {
            return ITK_FALSE;
        }
        if (!itk_type_equal(a->child, b->child)) {
            return ITK_FALSE;
        }
        for (i = 0; i < a->param_count; i++) {
            if (!itk_type_equal(a->params[i], b->params[i])) {
                return ITK_FALSE;
            }
        }
        return ITK_TRUE;
    default:
        return ITK_TRUE;
    }
}

ITK_DEF const char *itk_type_kind_name(itk_type_kind kind)
{
    switch (kind) {
    case ITK_KIND_VOID:    return "void";
    case ITK_KIND_BOOL:    return "bool";
    case ITK_KIND_CHAR:    return "char";
    case ITK_KIND_SCHAR:   return "signed char";
    case ITK_KIND_UCHAR:   return "unsigned char";
    case ITK_KIND_SHORT:   return "short";
    case ITK_KIND_USHORT:  return "unsigned short";
    case ITK_KIND_INT:     return "int";
    case ITK_KIND_UINT:    return "unsigned int";
    case ITK_KIND_LONG:    return "long";
    case ITK_KIND_ULONG:   return "unsigned long";
    case ITK_KIND_LLONG:   return "long long";
    case ITK_KIND_ULLONG:  return "unsigned long long";
    case ITK_KIND_I8:      return "int8_t";
    case ITK_KIND_I16:     return "int16_t";
    case ITK_KIND_I32:     return "int32_t";
    case ITK_KIND_I64:     return "int64_t";
    case ITK_KIND_U8:      return "uint8_t";
    case ITK_KIND_U16:     return "uint16_t";
    case ITK_KIND_U32:     return "uint32_t";
    case ITK_KIND_U64:     return "uint64_t";
    case ITK_KIND_FLOAT:   return "float";
    case ITK_KIND_DOUBLE:  return "double";
    case ITK_KIND_LDOUBLE: return "long double";
    case ITK_KIND_SIZE:    return "size_t";
    case ITK_KIND_PTRDIFF: return "ptrdiff_t";
    case ITK_KIND_INTPTR:  return "intptr_t";
    case ITK_KIND_UINTPTR: return "uintptr_t";
    case ITK_KIND_ENUM:    return "enum";
    case ITK_KIND_PTR:     return "ptr";
    case ITK_KIND_ARRAY:   return "array";
    case ITK_KIND_FUNC:    return "func";
    default:               return "?kind";
    }
}

#endif /* ITK_CTYPES_IMPLEMENTATION */

#endif /* ITK_CTYPES_H */
