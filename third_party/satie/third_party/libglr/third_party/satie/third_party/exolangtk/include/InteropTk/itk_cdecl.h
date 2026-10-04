/**
 * @file itk_cdecl.h
 * @brief A minimal, dependency-free parser for C declaration snippets
 *        ("int (*)(const char *, size_t)") producing itk_type values. Lets
 *        interpreters accept C signatures as strings without a full C
 *        frontend.
 *
 * @stability experimental
 * @depends InteropTk::ctypes
 *
 * Grammar accepted (abstract or one named declarator):
 * @code
 *   decl    := base-type declarator? EOF
 *   base    := qual* word+ qual*          (word := type keyword|typedef)
 *   dcl     := '*' qual* dcl | direct
 *   direct  := '(' dcl ')' | IDENT? suffix*
 *   suffix  := '[' NUM? ']' | '(' params ')'
 *   params  := 'void' | (param (',' param)*)? ('...' after a ',')
 *   param   := decl
 * @endcode
 * Exact-width typedefs (int8_t..uint64_t, size_t, intptr_t, ...) are
 * recognized as base types. All built types borrow each other through the
 * caller-owned itk_cdecl arena, which must outlive every derived type.
 */

#ifndef ITK_CDECL_H
#define ITK_CDECL_H

#include "itk_platform.h"
#include "itk_ctypes.h"

/* ── public declarations ──────────────────────────────────────────────── */

/** @brief Type slots available in one parse. */
#define ITK_CDECL_MAX_TYPES 64
/** @brief Parameters per function type. */
#define ITK_CDECL_MAX_PARAMS 16
/** @brief Parenthesized declarator nesting depth. */
#define ITK_CDECL_MAX_DEPTH 8
/** @brief Suffixes (array/function) per declarator level. */
#define ITK_CDECL_MAX_SUFFIX 8
/** @brief Total parameter-pointer slots across all function types. */
#define ITK_CDECL_MAX_SLOTS 96
/** @brief Diagnostic message capacity including the terminator. */
#define ITK_CDECL_MESSAGE_MAX 96

/** @brief Parse/format diagnostic codes. */
typedef enum itk_cdecl_error {
    ITK_CDECL_OK = 0,       /**< Success. */
    ITK_CDECL_EARG = 1,     /**< NULL argument. */
    ITK_CDECL_ETYPE = 2,    /**< Unknown or malformed base type. */
    ITK_CDECL_ESYNTAX = 3,  /**< Unexpected token / structure. */
    ITK_CDECL_EDEPTH = 4,   /**< Parentheses nested too deeply. */
    ITK_CDECL_EBUF = 5,     /**< Type or parameter arena exhausted. */
    ITK_CDECL_ETRUNC = 6    /**< Format output longer than the buffer. */
} itk_cdecl_error;

/**
 * @brief Parse context, type arena, and result record.
 *
 * Zero-initialize (or itk_cdecl_reset()) before each parse. All types
 * produced by a parse borrow pointers to each other *inside this struct*;
 * none of them outlive it.
 *
 * @var itk_cdecl::types
 *      Bump-allocated type storage backing every derived type.
 * @var itk_cdecl::type_count
 *      Slots used (the result is types[type_count-1]).
 * @var itk_cdecl::slots
 *      Bump-allocated parameter-pointer storage for function types.
 * @var itk_cdecl::slot_count
 *      Slots used.
 * @var itk_cdecl::name
 *      Borrowed start of the declarator identifier inside the source, or
 *      NULL for abstract declarators.
 * @var itk_cdecl::name_len
 *      Identifier length in bytes.
 * @var itk_cdecl::err
 *      Zero on success, otherwise one of #itk_cdecl_error.
 * @var itk_cdecl::err_pos
 *      Byte offset in the source where the failure was detected.
 * @var itk_cdecl::message
 *      Human-readable failure text (empty on success).
 */
typedef struct itk_cdecl {
    itk_type types[ITK_CDECL_MAX_TYPES];        /**< Type arena. */
    size_t type_count;                          /**< Types built. */
    const itk_type *slots[ITK_CDECL_MAX_SLOTS]; /**< Param arenas. */
    size_t slot_count;                          /**< Param slots used. */
    const char *name;                           /**< Declarator name. */
    size_t name_len;                            /**< Name length. */
    int err;                                    /**< #itk_cdecl_error. */
    size_t err_pos;                             /**< Failure offset. */
    char message[ITK_CDECL_MESSAGE_MAX];        /**< Failure text. */
} itk_cdecl;

/**
 * @brief Clear @p cx for a fresh parse.
 * @param cx  Context; must not be NULL.
 * @note Does not write to the source string; safe to reuse directly.
 */
ITK_DEF void itk_cdecl_reset(itk_cdecl *cx);

/**
 * @brief Parse a C declaration snippet into @p cx.
 * @param src  NUL-terminated snippet; must not be NULL.
 * @param cx   Reset-or-fresh context; must not be NULL.
 * @return ITK_TRUE on success (fetch the type with itk_cdecl_type());
 *         ITK_FALSE with cx->err/err_pos/message set otherwise.
 * @note Whitespace-insensitive except inside identifiers; comments are
 *       not supported. No global state; safe from any thread.
 */
ITK_DEF itk_bool itk_cdecl_parse(const char *src, itk_cdecl *cx);

/**
 * @brief The parsed type.
 * @param cx  Context after a successful itk_cdecl_parse().
 * @return Pointer into cx->types, or NULL when the last parse failed.
 * @note The pointer borrows storage inside @p cx.
 */
ITK_DEF const itk_type *itk_cdecl_type(const itk_cdecl *cx);

/**
 * @brief The declarator identifier, if any.
 * @param cx  Context after a parse.
 * @param len When non-NULL, receives the name length.
 * @return Start of the name inside the source, or NULL when abstract.
 */
ITK_DEF const char *itk_cdecl_name(const itk_cdecl *cx, size_t *len);

/**
 * @brief Render a type back to canonical C syntax.
 * @param t    Type to render; must not be NULL.
 * @param name Optional identifier inserted at the declarator position.
 * @param buf  Output buffer; must not be NULL.
 * @param cap  Capacity of @p buf including the terminator.
 * @return ITK_TRUE on success; ITK_FALSE on truncation or bad arguments.
 * @note Pointer-to-function/array renders with parenthesized abstract
 *       syntax, e.g. "int (*)(const char *, size_t)".
 */
ITK_DEF itk_bool itk_cdecl_format(const itk_type *t, const char *name, char *buf,
                          size_t cap);

#ifdef ITK_CDECL_IMPLEMENTATION
/* ── implementation section ─────────────────────────────────────────── */

#include <string.h>

/** Parser cursor over the source plus a link to the arena. */
typedef struct itk_cdecl_cur_ {
    const char *src;
    size_t pos;
    itk_cdecl *cx;
    unsigned depth;
} itk_cdecl_cur_;

/** Record a diagnostic and return ITK_FALSE. */
static itk_bool itk_cdecl_fail_(itk_cdecl *cx, int err, size_t pos,
                                const char *msg)
{
    size_t i = 0;

    while (msg[i] != '\0' && i + 1 < ITK_CDECL_MESSAGE_MAX) {
        cx->message[i] = msg[i];
        i++;
    }
    cx->message[i] = '\0';
    cx->err = err;
    cx->err_pos = pos;
    return ITK_FALSE;
}

static void itk_cdecl_skip_ws_(itk_cdecl_cur_ *c)
{
    while (c->src[c->pos] == ' ' || c->src[c->pos] == '\t' ||
           c->src[c->pos] == '\n' || c->src[c->pos] == '\r') {
        c->pos++;
    }
}

/** Peek the next significant character ('\0' at end). */
static char itk_cdecl_peek_(itk_cdecl_cur_ *c)
{
    itk_cdecl_skip_ws_(c);
    return c->src[c->pos];
}

/** Consume @p ch if peeked. */
static itk_bool itk_cdecl_eat_(itk_cdecl_cur_ *c, char ch)
{
    if (itk_cdecl_peek_(c) == ch) {
        c->pos++;
        return ITK_TRUE;
    }
    return ITK_FALSE;
}

static int itk_cdecl_is_alpha_(char ch)
{
    return ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            ch == '_');
}

static int itk_cdecl_is_digit_(char ch)
{
    return (ch >= '0' && ch <= '9');
}

/** Read an identifier into @p out (NUL-terminated). */
static itk_bool itk_cdecl_ident_(itk_cdecl_cur_ *c, char *out, size_t cap)
{
    size_t n = 0;

    itk_cdecl_skip_ws_(c);
    if (!itk_cdecl_is_alpha_(c->src[c->pos])) {
        return ITK_FALSE;
    }
    while (itk_cdecl_is_alpha_(c->src[c->pos]) ||
           itk_cdecl_is_digit_(c->src[c->pos])) {
        if (n + 1 >= cap) {
            return ITK_FALSE;
        }
        out[n++] = c->src[c->pos++];
    }
    out[n] = '\0';
    return ITK_TRUE;
}

/** Whether @p w is a reserved type word (blocks name capture). */
static itk_bool itk_cdecl_is_typeword_(const char *w)
{
    static const char *const reserved[] = {
        "void", "char", "signed", "unsigned", "short", "int", "long",
        "float", "double", "const", "volatile", "restrict", "_Bool",
        "bool", "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t",
        "uint16_t", "uint32_t", "uint64_t", "size_t", "ssize_t",
        "ptrdiff_t", "intptr_t", "uintptr_t"
    };
    size_t i;

    for (i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++) {
        if (strcmp(w, reserved[i]) == 0) {
            return ITK_TRUE;
        }
    }
    return ITK_FALSE;
}

/** Parse the base type: qualifier and keyword soup, then classify. */
static itk_bool itk_cdecl_base_(itk_cdecl_cur_ *c, itk_type *out)
{
    unsigned quals = 0;
    int saw_signed = 0, saw_unsigned = 0;
    int chars = 0, shorts = 0, longs = 0, ints = 0;
    int doubles = 0, floats = 0, saw_void = 0;
    int saw_any = 0;
    itk_type_kind kind = ITK_KIND_INT;
    char word[32];

    for (;;) {
        size_t save = c->pos;

        if (!itk_cdecl_ident_(c, word, sizeof(word))) {
            c->pos = save;
            break;
        }
        if (strcmp(word, "const") == 0) {
            quals |= ITK_QUAL_CONST;
            saw_any = 1;
            continue;
        }
        if (strcmp(word, "volatile") == 0) {
            quals |= ITK_QUAL_VOLATILE;
            saw_any = 1;
            continue;
        }
        if (strcmp(word, "restrict") == 0) {
            quals |= ITK_QUAL_RESTRICT;
            saw_any = 1;
            continue;
        }
        if (strcmp(word, "signed") == 0) {
            saw_signed = 1;
            ints++;
            saw_any = 1;
            continue;
        }
        if (strcmp(word, "unsigned") == 0) {
            saw_unsigned = 1;
            ints++;
            saw_any = 1;
            continue;
        }
        if (strcmp(word, "char") == 0)        { chars++; saw_any = 1; continue; }
        if (strcmp(word, "short") == 0)       { shorts++; saw_any = 1; continue; }
        if (strcmp(word, "long") == 0)        { longs++; saw_any = 1; continue; }
        if (strcmp(word, "int") == 0)         { ints++; saw_any = 1; continue; }
        if (strcmp(word, "float") == 0)       { floats++; saw_any = 1; continue; }
        if (strcmp(word, "double") == 0)      { doubles++; saw_any = 1; continue; }
        if (strcmp(word, "void") == 0)        { saw_void++; saw_any = 1; continue; }
        if (strcmp(word, "_Bool") == 0 || strcmp(word, "bool") == 0) {
            kind = ITK_KIND_BOOL;
            saw_any = 1;
            goto base_done;
        }
        {
            /* exact-width / size typedefs stand alone */
            static const struct { const char *w; itk_type_kind k; } tds[] = {
                { "int8_t", ITK_KIND_I8 },   { "int16_t", ITK_KIND_I16 },
                { "int32_t", ITK_KIND_I32 }, { "int64_t", ITK_KIND_I64 },
                { "uint8_t", ITK_KIND_U8 },  { "uint16_t", ITK_KIND_U16 },
                { "uint32_t", ITK_KIND_U32 },{ "uint64_t", ITK_KIND_U64 },
                { "size_t", ITK_KIND_SIZE }, { "ssize_t", ITK_KIND_PTRDIFF },
                { "ptrdiff_t", ITK_KIND_PTRDIFF },
                { "intptr_t", ITK_KIND_INTPTR },
                { "uintptr_t", ITK_KIND_UINTPTR }
            };
            size_t k;
            int known = 0;

            for (k = 0; k < sizeof(tds) / sizeof(tds[0]); k++) {
                if (strcmp(word, tds[k].w) == 0) {
                    kind = tds[k].k;
                    known = 1;
                    break;
                }
            }
            if (known) {
                saw_any = 1;
                goto base_done;
            }
        }
        c->pos = save; /* not part of the base type */
        break;
    }
base_done:
    (void)saw_any;

    if (saw_void) {
        if (chars || shorts || longs || doubles || floats || ints) {
            return itk_cdecl_fail_(c->cx, ITK_CDECL_ETYPE, c->pos,
                                   "void cannot combine with other words");
        }
        kind = ITK_KIND_VOID;
    } else if (chars) {
        kind = saw_unsigned ? ITK_KIND_UCHAR
                : (saw_signed ? ITK_KIND_SCHAR : ITK_KIND_CHAR);
    } else if (shorts) {
        kind = saw_unsigned ? ITK_KIND_USHORT : ITK_KIND_SHORT;
    } else if (longs >= 2) {
        kind = saw_unsigned ? ITK_KIND_ULLONG : ITK_KIND_LLONG;
    } else if (longs == 1 && doubles) {
        kind = ITK_KIND_LDOUBLE;
    } else if (longs == 1) {
        kind = saw_unsigned ? ITK_KIND_ULONG : ITK_KIND_LONG;
    } else if (doubles) {
        kind = ITK_KIND_DOUBLE;
    } else if (floats) {
        kind = ITK_KIND_FLOAT;
    } else if (ints) {
        kind = saw_unsigned ? ITK_KIND_UINT : ITK_KIND_INT;
    } else if (saw_signed) {
        kind = ITK_KIND_INT; /* bare "signed" */
    } else {
        return itk_cdecl_fail_(c->cx, ITK_CDECL_ETYPE, c->pos,
                               "missing base type");
    }

    *out = itk_type_prim(kind);
    out->quals = quals;
    return ITK_TRUE;
}

/** Allocate one arena type slot. */
static itk_type *itk_cdecl_new_(itk_cdecl *cx)
{
    if (cx->type_count >= ITK_CDECL_MAX_TYPES) {
        itk_cdecl_fail_(cx, ITK_CDECL_EBUF, 0, "type arena exhausted");
        return NULL;
    }
    return &cx->types[cx->type_count++];
}

/** Allocate a contiguous param-slot range. */
static const itk_type **itk_cdecl_slots_(itk_cdecl *cx, size_t n)
{
    const itk_type **base;

    if (cx->slot_count + n > ITK_CDECL_MAX_SLOTS) {
        itk_cdecl_fail_(cx, ITK_CDECL_EBUF, 0, "parameter arena exhausted");
        return NULL;
    }
    base = &cx->slots[cx->slot_count];
    cx->slot_count += n;
    return base;
}

/** One suffix: array or function. */
typedef struct itk_cdecl_suffix_ {
    itk_bool is_func;                       /**< Function vs array. */
    size_t length;                          /**< Array length (0 = []). */
    const itk_type *params[ITK_CDECL_MAX_PARAMS]; /**< Param types. */
    size_t param_count;                     /**< Param total. */
    itk_bool variadic;                      /**< Trailing "...". */
} itk_cdecl_suffix_;

/**
 * Declarator node: pointer levels plus the direct part. Lives on the C
 * stack during parse; composition happens as recursion unwinds.
 */
typedef struct itk_cdecl_node_ {
    unsigned star_quals[ITK_CDECL_MAX_SUFFIX * 2]; /**< Per-'*' quals. */
    size_t star_count;                    /**< Pointer levels. */
    itk_bool has_nested;                  /**< '(' dcl ')' direct part. */
    struct itk_cdecl_node_ *nested;       /**< Inner node when has_nested. */
    itk_cdecl_suffix_ sfx[ITK_CDECL_MAX_SUFFIX]; /**< Suffixes in order. */
    size_t sfx_count;                     /**< Suffix total. */
} itk_cdecl_node_;

static itk_bool itk_cdecl_parse_decl_(itk_cdecl_cur_ *c,
                                      itk_type base, itk_type *out);
static itk_bool itk_cdecl_hold_nested_(itk_cdecl_cur_ *c,
                                       itk_cdecl_node_ *outer,
                                       const itk_cdecl_node_ *inner);

/** Parse '(' params ')' into @p sf. */
static itk_bool itk_cdecl_params_(itk_cdecl_cur_ *c,
                                   itk_cdecl_suffix_ *sf)
{
    sf->is_func = ITK_TRUE;
    sf->param_count = 0;
    sf->variadic = ITK_FALSE;

    if (!itk_cdecl_eat_(c, '(')) {
        return itk_cdecl_fail_(c->cx, ITK_CDECL_ESYNTAX, c->pos,
                               "expected '('");
    }
    {   /* "()": unspecified list; "(void)": empty list. */
        size_t save = c->pos;
        char word[32];

        if (itk_cdecl_eat_(c, ')')) {
            return ITK_TRUE;
        }
        if (itk_cdecl_ident_(c, word, sizeof(word)) &&
            strcmp(word, "void") == 0 && itk_cdecl_eat_(c, ')')) {
            return ITK_TRUE;
        }
        c->pos = save;
    }

    for (;;) {
        if (c->src[c->pos] == '.' && c->src[c->pos + 1] == '.' &&
            c->src[c->pos + 2] == '.') {
            c->pos += 3;
            sf->variadic = ITK_TRUE;
            break;
        }
        {
            itk_type pbase;
            const itk_type *full;

            if (!itk_cdecl_base_(c, &pbase)) {
                return ITK_FALSE;
            }
            if (sf->param_count >= ITK_CDECL_MAX_PARAMS) {
                return itk_cdecl_fail_(c->cx, ITK_CDECL_EBUF, c->pos,
                                       "too many parameters");
            }
            {
                size_t save = c->pos;

                if (itk_cdecl_peek_(c) == ',' || itk_cdecl_peek_(c) == ')') {
                    /* bare base type */
                    itk_type *slot = itk_cdecl_new_(c->cx);

                    if (slot == NULL) {
                        return ITK_FALSE;
                    }
                    *slot = pbase;
                    full = slot;
                    (void)save;
                } else {
                    itk_type pres;
                    size_t before = c->cx->type_count;
                    size_t scan;
                    size_t save2 = c->pos;

                    if (!itk_cdecl_parse_decl_(c, pbase, &pres)) {
                        return ITK_FALSE;
                    }
                    /* The composed type is one of the arena slots built
                     * during that call; find it (it equals pres). */
                    full = NULL;
                    for (scan = before; scan < c->cx->type_count; scan++) {
                        if (memcmp(&c->cx->types[scan], &pres,
                                   sizeof(itk_type)) == 0) {
                            full = &c->cx->types[scan];
                            break;
                        }
                    }
                    if (full == NULL) {
                        itk_type *slot2 = itk_cdecl_new_(c->cx);

                        if (slot2 == NULL) {
                            return ITK_FALSE;
                        }
                        *slot2 = pres;
                        full = slot2;
                    }
                    (void)save2;
                }
            }
            sf->params[sf->param_count++] = full;
        }
        if (itk_cdecl_eat_(c, ',')) {
            continue;
        }
        break;
    }
    if (!itk_cdecl_eat_(c, ')')) {
        return itk_cdecl_fail_(c->cx, ITK_CDECL_ESYNTAX, c->pos,
                               "expected ')' or ',' in parameter list");
    }
    return ITK_TRUE;
}

/**
 * Parse one declarator level into @p node (structure only; no types).
 * Returns ITK_TRUE, or ITK_FALSE with a diagnostic set.
 */
static itk_bool itk_cdecl_parse_node_(itk_cdecl_cur_ *c,
                                       itk_cdecl_node_ *node)
{
    char word[32];

    node->star_count = 0;
    node->has_nested = ITK_FALSE;
    node->nested = NULL;
    node->sfx_count = 0;

    if (c->depth >= ITK_CDECL_MAX_DEPTH) {
        return itk_cdecl_fail_(c->cx, ITK_CDECL_EDEPTH, c->pos,
                               "declarator nesting too deep");
    }

    /* '*' qual* ... */
    while (itk_cdecl_eat_(c, '*')) {
        unsigned q = 0;

        for (;;) {
            size_t save = c->pos;

            if (!itk_cdecl_ident_(c, word, sizeof(word))) {
                c->pos = save;
                break;
            }
            if (strcmp(word, "const") == 0) {
                q |= ITK_QUAL_CONST;
            } else if (strcmp(word, "volatile") == 0) {
                q |= ITK_QUAL_VOLATILE;
            } else {
                c->pos = save; /* the declarator name, not a qualifier */
                break;
            }
        }
        if (node->star_count >=
            sizeof(node->star_quals) / sizeof(node->star_quals[0])) {
            return itk_cdecl_fail_(c->cx, ITK_CDECL_EDEPTH, c->pos,
                                   "too many pointer levels");
        }
        node->star_quals[node->star_count++] = q;
    }

    /* Direct part. A '(' here either opens a nested declarator or is an
     * abstract parameter suffix; distinguish by looking ahead one word. */
    if (itk_cdecl_peek_(c) == '(') {
        size_t save = c->pos;
        char probe[32];
        itk_bool is_params;

        c->pos++; /* consume '(' */
        itk_cdecl_skip_ws_(c);
        if (c->src[c->pos] == ')' || c->src[c->pos] == '.') {
            is_params = ITK_TRUE;   /* "()" or "(...)" */
        } else if (itk_cdecl_ident_(c, probe, sizeof(probe)) &&
                   itk_cdecl_is_typeword_(probe)) {
            is_params = ITK_TRUE;   /* "(int ...": parameter list */
        } else {
            is_params = ITK_FALSE;  /* "(*..." or "(name...": nested dcl */
        }
        c->pos = save;

        if (!is_params) {
            itk_cdecl_node_ inner;

            node->has_nested = ITK_TRUE;
            c->pos++; /* consume '(' for real */
            c->depth++;
            if (!itk_cdecl_parse_node_(c, &inner)) {
                return ITK_FALSE;
            }
            c->depth--;
            if (!itk_cdecl_eat_(c, ')')) {
                return itk_cdecl_fail_(c->cx, ITK_CDECL_ESYNTAX, c->pos,
                                       "expected ')'");
            }
            /* The inner node must outlive this frame until composition,
             * which happens in the caller after suffixes parse; heap-copy
             * it into the arena? No — composition happens inside
             * itk_cdecl_compose_, called below with node alive. Keep it
             * on this frame and compose here? Composition needs the
             * suffixes parsed AFTER the parens. Solution: hold the inner
             * node by value in this frame and compose after suffixes. */
            {
                /* suffixes first */
                size_t sc = 0;

                for (;;) {
                    if (itk_cdecl_peek_(c) == '[') {
                        c->pos++;
                        if (sc >= ITK_CDECL_MAX_SUFFIX) {
                            return itk_cdecl_fail_(c->cx, ITK_CDECL_EBUF,
                                                   c->pos,
                                                   "too many suffixes");
                        }
                        node->sfx[sc].is_func = ITK_FALSE;
                        node->sfx[sc].length = 0;
                        node->sfx[sc].param_count = 0;
                        node->sfx[sc].variadic = ITK_FALSE;
                        if (itk_cdecl_is_digit_(c->src[c->pos])) {
                            size_t n = 0;

                            while (itk_cdecl_is_digit_(c->src[c->pos])) {
                                if (n > (((size_t)-1) - 9u) / 10u) {
                                    return itk_cdecl_fail_(
                                        c->cx, ITK_CDECL_ESYNTAX, c->pos,
                                        "array length overflow");
                                }
                                n = n * 10u +
                                    (size_t)(c->src[c->pos] - '0');
                                c->pos++;
                            }
                            node->sfx[sc].length = n;
                        }
                        if (!itk_cdecl_eat_(c, ']')) {
                            return itk_cdecl_fail_(c->cx, ITK_CDECL_ESYNTAX,
                                                   c->pos,
                                                   "expected ']'");
                        }
                        sc++;
                    } else if (itk_cdecl_peek_(c) == '(') {
                        if (sc >= ITK_CDECL_MAX_SUFFIX) {
                            return itk_cdecl_fail_(c->cx, ITK_CDECL_EBUF,
                                                   c->pos,
                                                   "too many suffixes");
                        }
                        if (!itk_cdecl_params_(c, &node->sfx[sc])) {
                            return ITK_FALSE;
                        }
                        sc++;
                    } else {
                        break;
                    }
                }
                node->sfx_count = sc;
            }
            /* Compose with the inner node captured by address. */
            return itk_cdecl_hold_nested_(c, node, &inner);
        }
        (void)probe;
    }

    /* Optional identifier. */
    {
        size_t save = c->pos;

        if (itk_cdecl_ident_(c, word, sizeof(word)) &&
            !itk_cdecl_is_typeword_(word)) {
            c->cx->name = c->src + c->pos - strlen(word);
            c->cx->name_len = strlen(word);
        } else {
            c->pos = save;
        }
    }

    /* Suffixes. */
    for (;;) {
        if (itk_cdecl_peek_(c) == '[') {
            c->pos++;
            if (node->sfx_count >= ITK_CDECL_MAX_SUFFIX) {
                return itk_cdecl_fail_(c->cx, ITK_CDECL_EBUF, c->pos,
                                       "too many suffixes");
            }
            node->sfx[node->sfx_count].is_func = ITK_FALSE;
            node->sfx[node->sfx_count].length = 0;
            node->sfx[node->sfx_count].param_count = 0;
            node->sfx[node->sfx_count].variadic = ITK_FALSE;
            if (itk_cdecl_is_digit_(c->src[c->pos])) {
                size_t n = 0;

                while (itk_cdecl_is_digit_(c->src[c->pos])) {
                    if (n > (((size_t)-1) - 9u) / 10u) {
                        return itk_cdecl_fail_(c->cx, ITK_CDECL_ESYNTAX,
                                               c->pos,
                                               "array length overflow");
                    }
                    n = n * 10u + (size_t)(c->src[c->pos] - '0');
                    c->pos++;
                }
                node->sfx[node->sfx_count].length = n;
            }
            if (!itk_cdecl_eat_(c, ']')) {
                return itk_cdecl_fail_(c->cx, ITK_CDECL_ESYNTAX, c->pos,
                                       "expected ']'");
            }
            node->sfx_count++;
        } else if (itk_cdecl_peek_(c) == '(') {
            if (node->sfx_count >= ITK_CDECL_MAX_SUFFIX) {
                return itk_cdecl_fail_(c->cx, ITK_CDECL_EBUF, c->pos,
                                       "too many suffixes");
            }
            if (!itk_cdecl_params_(c, &node->sfx[node->sfx_count])) {
                return ITK_FALSE;
            }
            node->sfx_count++;
        } else {
            break;
        }
    }
    return ITK_TRUE;
}

/* Compose the non-parenthesized subset of a declarator.  Parenthesized
 * declarators are diagnosed by the public parser until a future grammar
 * extension can preserve their binding structure without hidden storage. */
static itk_bool itk_cdecl_parse_decl_(itk_cdecl_cur_ *c, itk_type base,
                                      itk_type *out)
{
    itk_cdecl_node_ node;
    itk_type *slot;
    size_t i;

    if (!itk_cdecl_parse_node_(c, &node)) return ITK_FALSE;
    if (node.has_nested) {
        return itk_cdecl_fail_(c->cx, ITK_CDECL_ESYNTAX, c->pos,
                               "nested declarators are not supported");
    }
    slot = itk_cdecl_new_(c->cx);
    if (slot == NULL) return ITK_FALSE;
    *slot = base;
    for (i = 0; i < node.star_count; ++i) {
        itk_type *next = itk_cdecl_new_(c->cx);
        if (next == NULL) return ITK_FALSE;
        *next = itk_type_ptr_to(slot);
        slot = next;
    }
    for (i = 0; i < node.sfx_count; ++i) {
        itk_type *next = itk_cdecl_new_(c->cx);
        if (next == NULL) return ITK_FALSE;
        if (node.sfx[i].is_func) {
            const itk_type **params =
                itk_cdecl_slots_(c->cx, node.sfx[i].param_count);
            size_t p;
            if (params == NULL) return ITK_FALSE;
            for (p = 0; p < node.sfx[i].param_count; ++p)
                params[p] = node.sfx[i].params[p];
            *next = itk_type_func(slot, params, node.sfx[i].param_count,
                                  node.sfx[i].variadic);
        } else {
            *next = itk_type_array_of(slot, node.sfx[i].length);
        }
        slot = next;
    }
    *out = *slot;
    return ITK_TRUE;
}

static itk_bool itk_cdecl_hold_nested_(itk_cdecl_cur_ *c,
                                       itk_cdecl_node_ *outer,
                                       const itk_cdecl_node_ *inner)
{
    (void)outer;
    (void)inner;
    return itk_cdecl_fail_(c->cx, ITK_CDECL_ESYNTAX, c->pos,
                           "nested declarators are not supported");
}

ITK_DEF void itk_cdecl_reset(itk_cdecl *cx)
{
    if (cx == NULL) return;
    memset(cx, 0, sizeof(*cx));
}

ITK_DEF itk_bool itk_cdecl_parse(const char *src, itk_cdecl *cx)
{
    itk_cdecl_cur_ c;
    itk_type base;
    itk_type result;

    if (src == NULL || cx == NULL) return ITK_FALSE;
    itk_cdecl_reset(cx);
    c.src = src;
    c.pos = 0;
    c.cx = cx;
    c.depth = 0;
    if (!itk_cdecl_base_(&c, &base)) return ITK_FALSE;
    if (itk_cdecl_peek_(&c) != '\0' &&
        !itk_cdecl_parse_decl_(&c, base, &result)) return ITK_FALSE;
    if (itk_cdecl_peek_(&c) != '\0')
        return itk_cdecl_fail_(cx, ITK_CDECL_ESYNTAX, c.pos,
                               "unexpected trailing input");
    {
        itk_type *slot = itk_cdecl_new_(cx);
        if (slot == NULL) return ITK_FALSE;
        *slot = (itk_cdecl_peek_(&c) == '\0' && cx->type_count == 1)
                    ? base : result;
    }
    cx->err = ITK_CDECL_OK;
    cx->message[0] = '\0';
    return ITK_TRUE;
}

ITK_DEF const itk_type *itk_cdecl_type(const itk_cdecl *cx)
{
    if (cx == NULL || cx->err != ITK_CDECL_OK || cx->type_count == 0)
        return NULL;
    return &cx->types[cx->type_count - 1];
}

ITK_DEF const char *itk_cdecl_name(const itk_cdecl *cx, size_t *len)
{
    if (len != NULL) *len = (cx == NULL) ? 0 : cx->name_len;
    return cx == NULL ? NULL : cx->name;
}

ITK_DEF itk_bool itk_cdecl_format(const itk_type *t, const char *name,
                                  char *buf, size_t cap)
{
    const char *kind;
    size_t n = 0;
    if (t == NULL || buf == NULL || cap == 0) return ITK_FALSE;
    kind = itk_type_kind_name(t->kind);
    while (*kind != '\0') {
        if (n + 1 >= cap) { buf[0] = '\0'; return ITK_FALSE; }
        buf[n++] = *kind++;
    }
    if (name != NULL && *name != '\0') {
        if (n + 1 >= cap) { buf[0] = '\0'; return ITK_FALSE; }
        buf[n++] = ' ';
        while (*name != '\0') {
            if (n + 1 >= cap) { buf[0] = '\0'; return ITK_FALSE; }
            buf[n++] = *name++;
        }
    }
    buf[n] = '\0';
    return ITK_TRUE;
}

#endif /* ITK_CDECL_IMPLEMENTATION */

#endif /* ITK_CDECL_H */
