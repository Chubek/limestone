# InteropTk: Value Marshalling {#manual_09_marshal}

Module: [itk_marshal.h](../include/InteropTk/itk_marshal.h) | Stability: stable

## Overview

`itk_marshal.h` provides value marshalling primitives between a host
language's representation and raw C memory. It includes bounds-checked scalar
reads and writes, aggregate copy-in and copy-out, and endian-aware accessors
driven by `itk_type` descriptors.

## Scalar Carrier

~~~c
typedef union itk_scalar {
    uint64_t u;
    int64_t s;
    double d;
    long double ld;
} itk_scalar;
~~~

The `itk_scalar` union is wide enough to hold any C scalar type. Integers are
zero-extended (unsigned) or sign-extended (signed) on read. Floats are stored
in `d` (double) or `ld` (long double).

## Scalar Read and Write

~~~c
itk_bool itk_read_scalar(const void *buf, size_t len, const itk_type *t,
                         itk_scalar *out);
itk_bool itk_write_scalar(void *buf, size_t len, const itk_type *t,
                          itk_scalar value);
~~~

- `itk_read_scalar()` reads a scalar from `buf` in the target's native byte
  order. `len` is the bounds check — it must be at least `itk_type_size(t)`.
- `itk_write_scalar()` writes a scalar to `buf`. Narrow integers are
  truncated to the type's width; floats are narrowed on store.

Both return `ITK_TRUE` on success, `ITK_FALSE` on invalid arguments or
bounds violation.

## Endian-Aware Accessors

~~~c
itk_bool itk_read_scalar_le(const void *buf, size_t len, const itk_type *t,
                            itk_scalar *out);
itk_bool itk_read_scalar_be(const void *buf, size_t len, const itk_type *t,
                            itk_scalar *out);
itk_bool itk_write_scalar_le(void *buf, size_t len, const itk_type *t,
                             itk_scalar value);
itk_bool itk_write_scalar_be(void *buf, size_t len, const itk_type *t,
                             itk_scalar value);
~~~

These read and write scalars as explicit little-endian or big-endian bytes,
regardless of host byte order. Bytes are assembled LSB-first or MSB-first
respectively.

## Record Field Access

~~~c
itk_bool itk_record_read_field(const itk_record *r, size_t index,
                               itk_scalar *out);
itk_bool itk_record_write_field(const itk_record *r, size_t index,
                                itk_scalar value);
~~~

These read or write a single field of a record by index, handling bitfield
extraction and insertion internally.

## Aggregate Marshalling

~~~c
itk_bool itk_marshal_record(const itk_record *r, void *data,
                            const itk_scalar *values, size_t count);
itk_bool itk_unmarshal_record(const itk_record *r, const void *data,
                              itk_scalar *values, size_t count);
~~~

- `itk_marshal_record()` copies scalar values into a raw C record buffer.
- `itk_unmarshal_record()` extracts scalar values from a raw C record buffer.

Both accept a `values` array and a `count` (which must match the record's
field count). Bitfields are handled transparently.

## Usage Example

~~~c
#define ITK_PLATFORM_IMPLEMENTATION
#define ITK_CTYPES_IMPLEMENTATION
#define ITK_MARSHAL_IMPLEMENTATION
#include "InteropTk/itk_marshal.h"

itk_type t = itk_type_prim(ITK_KIND_INT);
char buf[4];
itk_scalar val = { .s = 42 };

itk_write_scalar(buf, sizeof(buf), &t, val);

itk_scalar out;
itk_read_scalar(buf, sizeof(buf), &t, &out);
printf("read back: %ld\n", (long)out.s);
~~~
