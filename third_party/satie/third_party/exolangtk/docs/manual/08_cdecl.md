# InteropTk: C Declaration Parser {#manual_08_cdecl}

Module: [itk_cdecl.h](../include/InteropTk/itk_cdecl.h) | Stability: experimental

## Overview

`itk_cdecl.h` is a minimal, dependency-free parser for C declaration
snippets. Given a string like `"int (*)(const char *, size_t)"`, it produces
an `itk_type` descriptor. This lets interpreters accept C signatures as
strings without requiring a full C frontend.

## Grammar

~~~
decl    := base-type declarator? EOF
base    := qual* word+ qual*
dcl     := '*' qual* dcl | direct
direct  := '(' dcl ')' | IDENT? suffix*
suffix  := '[' NUM? ']' | '(' params ')'
params  := 'void' | (param (',' param)*)? ('...' after a ',')
param   := decl
~~~

Exact-width typedefs (`int8_t`..`uint64_t`, `size_t`, `intptr_t`, etc.) are
recognized as base types.

## Parse Context

~~~c
typedef struct itk_cdecl {
    itk_type types[ITK_CDECL_MAX_TYPES];
    size_t type_count;
    const itk_type *slots[ITK_CDECL_MAX_SLOTS];
    size_t slot_count;
    const char *name;
    size_t name_len;
    int err;
    size_t err_pos;
    char message[ITK_CDECL_MESSAGE_MAX];
} itk_cdecl;
~~~

Zero-initialize or call `itk_cdecl_reset()` before each parse. All types
produced by a parse borrow pointers to each other inside this struct; none
outlive it.

## Capacity Constants

~~~c
#define ITK_CDECL_MAX_TYPES   64
#define ITK_CDECL_MAX_PARAMS  16
#define ITK_CDECL_MAX_DEPTH    8
#define ITK_CDECL_MAX_SUFFIX   8
#define ITK_CDECL_MAX_SLOTS   96
#define ITK_CDECL_MESSAGE_MAX 96
~~~

## Parse and Query Functions

~~~c
void itk_cdecl_reset(itk_cdecl *cx);
itk_bool itk_cdecl_parse(const char *src, itk_cdecl *cx);
const itk_type *itk_cdecl_type(const itk_cdecl *cx);
const char *itk_cdecl_name(const itk_cdecl *cx, size_t *len);
~~~

## Formatting

~~~c
itk_bool itk_cdecl_format(const itk_type *t, const char *name,
                          char *buf, size_t cap);
~~~

Renders a type back to canonical C syntax. Pointer-to-function and
pointer-to-array types render with parenthesized abstract syntax, e.g.
`"int (*)(const char *, size_t)"`.

## Error Codes

~~~c
typedef enum itk_cdecl_error {
    ITK_CDECL_OK     = 0,
    ITK_CDECL_EARG   = 1,
    ITK_CDECL_ETYPE  = 2,
    ITK_CDECL_ESYNTAX = 3,
    ITK_CDECL_EDEPTH = 4,
    ITK_CDECL_EBUF   = 5,
    ITK_CDECL_ETRUNC = 6
} itk_cdecl_error;
~~~

On failure, `cx->err` is set to the error code, `cx->err_pos` to the byte
offset in the source, and `cx->message` to a human-readable description.

## Usage Example

~~~c
#define ITK_PLATFORM_IMPLEMENTATION
#define ITK_CTYPES_IMPLEMENTATION
#define ITK_CDECL_IMPLEMENTATION
#include "InteropTk/itk_cdecl.h"

itk_cdecl cx = {0};
if (itk_cdecl_parse("int (*)(const char *, size_t)", &cx)) {
    const itk_type *t = itk_cdecl_type(&cx);
    char buf[256];
    itk_cdecl_format(t, NULL, buf, sizeof(buf));
    printf("parsed: %s\n", buf);  /* "int (*)(const char *, size_t)" */
} else {
    printf("error at %zu: %s\n", cx.err_pos, cx.message);
}
~~~
