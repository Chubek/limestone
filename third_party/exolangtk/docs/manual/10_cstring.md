# InteropTk: String Bridging {#manual_10_cstring}

Module: [itk_cstring.h](../include/InteropTk/itk_cstring.h) | Stability: stable

## Overview

`itk_cstring.h` provides ownership-aware bridging of strings across the
interop boundary. It defines borrowed views, owned buffers, NUL-termination
guarantees, length-prefixed conversion, and UTF-8 validation for hosts with
non-C string representations.

## Borrowed String View

~~~c
typedef struct itk_str_view {
    const char *data;
    size_t len;
} itk_str_view;

#define ITK_STR_VIEW_EMPTY { NULL, (size_t)0 }
~~~

An `itk_str_view` is a borrowed reference to a string slice. It does not own
the memory and the caller must ensure the underlying data outlives the view.

## Owned String Buffer

~~~c
typedef struct itk_str_buf {
    char *data;
    size_t len;
    size_t cap;
    const itk_allocator *allocator;
} itk_str_buf;
~~~

An `itk_str_buf` owns its memory. It is allocated through a caller-supplied
allocator. The buffer is always NUL-terminated (the terminator is not counted
in `len`).

## View Functions

~~~c
itk_str_view itk_str_from_c(const char *s);
itk_str_view itk_str_view_from_len(const char *data, size_t len);
itk_bool itk_str_to_c(itk_str_view v, char *dst, size_t cap);
itk_bool itk_str_view_equals(itk_str_view a, itk_str_view b);
itk_bool itk_str_view_index(itk_str_view v, size_t index, char *out);
~~~

- `itk_str_from_c()` wraps a NUL-terminated C string.
- `itk_str_view_from_len()` wraps a counted byte range.
- `itk_str_to_c()` copies a view into a caller-supplied buffer with NUL
  termination.
- `itk_str_view_equals()` performs byte-for-byte comparison.
- `itk_str_view_index()` returns the byte at the given index.

## Buffer Functions

~~~c
itk_bool itk_str_buf_init(itk_str_buf *buf, const itk_allocator *a);
void itk_str_buf_free(itk_str_buf *buf);
itk_bool itk_str_buf_reserve(itk_str_buf *buf, size_t extra);
itk_bool itk_str_buf_append(itk_str_buf *buf, const char *data, size_t len);
itk_bool itk_str_buf_append_c(itk_str_buf *buf, const char *s);
~~~

- `itk_str_buf_init()` initializes an empty buffer. The allocator is borrowed.
- `itk_str_buf_free()` releases all memory and zeros the struct.
- `itk_str_buf_reserve()` ensures at least `extra` additional bytes of
  capacity.
- `itk_str_buf_append()` copies `len` bytes from `data` into the buffer.
- `itk_str_buf_append_c()` appends a NUL-terminated C string.

## UTF-8 Validation

~~~c
typedef enum itk_utf8_status {
    ITK_UTF8_OK,
    ITK_UTF8_TRUNCATED,
    ITK_UTF8_OVERLONG,
    ITK_UTF8_SURROGATE,
    ITK_UTF8_TOO_LARGE,
    ITK_UTF8_INVALID
} itk_utf8_status;

itk_utf8_status itk_utf8_validate(itk_str_view v, size_t *offset);
~~~

`itk_utf8_validate()` checks whether a string view contains valid UTF-8. On
failure, `*offset` is set to the byte position of the first error.

## Usage Example

~~~c
#define ITK_PLATFORM_IMPLEMENTATION
#define ITK_CSTRING_IMPLEMENTATION
#include "InteropTk/itk_cstring.h"

itk_str_view v = itk_str_from_c("hello");
itk_str_buf buf;
itk_str_buf_init(&buf, itk_libc_allocator());
itk_str_buf_append_c(&buf, " world");

size_t err_off = 0;
itk_utf8_status st = itk_utf8_validate(v, &err_off);

itk_str_buf_free(&buf);
~~~
