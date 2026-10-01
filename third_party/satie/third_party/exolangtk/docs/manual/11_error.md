# InteropTk: Error Handling {#manual_11_error}

Module: [itk_error.h](../include/InteropTk/itk_error.h) | Stability: stable

## Overview

`itk_error.h` provides a structured error type that every InteropTk module
uses as its primary error channel. It carries a domain, a numeric code, and a
human-readable message. OS errors are mapped to InteropTk status codes; raw
OS codes are preserved for caller inspection.

## The Status Struct

~~~c
typedef struct itk_status {
    int domain;
    int code;
    const char *message;
} itk_status;
~~~

- `domain` — one of the `ITK_DOMAIN_*` constants.
- `code` — a domain-specific error code.
- `message` — a borrowed, NUL-terminated diagnostic string.

## Domain Constants

~~~c
#define ITK_DOMAIN_NONE    0
#define ITK_DOMAIN_PLATFORM 1
#define ITK_DOMAIN_CTYPES   2
#define ITK_DOMAIN_LAYOUT   3
#define ITK_DOMAIN_MARSHAL  4
#define ITK_DOMAIN_CSTRING  5
#define ITK_DOMAIN_CDECL    6
#define ITK_DOMAIN_ALLOC    7
#define ITK_DOMAIN_OS       8
#define ITK_STATUS_MESSAGE_MAX 256
~~~

## Status Codes

~~~c
typedef enum itk_status_code {
    ITK_OK       = 0,
    ITK_EINVAL   = -1,
    ITK_ENOMEM   = -2,
    ITK_EOVERFLOW = -3,
    ITK_ENOTFOUND = -4,
    ITK_ETRUNC   = -5,
    ITK_EFAIL    = -6
} itk_status_code;
~~~

## Constructors and Queries

~~~c
itk_status itk_status_ok(void);
itk_bool itk_status_is_ok(itk_status st);
itk_status itk_status_set(int code, const char *message);
itk_status itk_status_from_errno(const char *fallback_message);
itk_status itk_status_from_lasterror(const char *fallback_message);
int itk_status_from_os(int domain, int code);
~~~

- `itk_status_ok()` returns the success sentinel.
- `itk_status_is_ok()` returns `ITK_TRUE` when `st.code == ITK_OK`.
- `itk_status_set()` constructs a status with the given code and message.
- `itk_status_from_errno()` reads `errno` and produces a status with a
  platform-specific message.
- `itk_status_from_lasterror()` reads the last OS error (via `GetLastError()`
  on Windows, `errno` on POSIX).
- `itk_status_from_os()` maps a raw OS error code to an InteropTk status
  code.

## Usage Example

~~~c
#define ITK_PLATFORM_IMPLEMENTATION
#define ITK_ERROR_IMPLEMENTATION
#include "InteropTk/itk_error.h"

itk_status st = itk_status_ok();

FILE *f = fopen("nonexistent", "r");
if (f == NULL) {
    st = itk_status_from_errno("could not open file");
    printf("error: domain=%d code=%d message=%s\n",
           st.domain, st.code, st.message);
}
~~~
