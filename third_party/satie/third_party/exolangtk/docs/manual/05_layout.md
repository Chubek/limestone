# InteropTk: Record Layout {#manual_05_layout}

Module: [itk_layout.h](../include/InteropTk/itk_layout.h) | Stability: stable

## Overview

`itk_layout.h` computes struct, union, and bitfield layout matching the
target ABI. Given a set of fields with their types, it calculates field
offsets, padding, tail padding, and overall size and alignment. This is
essential when a managed language must mirror C records byte-for-byte.

## Record Kinds and ABI

~~~c
typedef enum itk_record_kind {
    ITK_RECORD_STRUCT,
    ITK_RECORD_UNION
} itk_record_kind;

typedef enum itk_abi_kind {
    ITK_ABI_DEFAULT,
    ITK_ABI_SYSV,
    ITK_ABI_WIN64,
    ITK_ABI_AAPCS
} itk_abi_kind;
~~~

`itk_layout_default_abi()` returns the default ABI for the target platform.

## The Record Builder

~~~c
typedef struct itk_record_builder {
    /* internal state — caller allocates, zero-initialize before use */
} itk_record_builder;

typedef struct itk_record {
    itk_record_kind kind;
    size_t field_count;
    size_t size;
    size_t alignment;
    /* internal field data */
} itk_record;

typedef struct itk_field_info {
    size_t offset;
    size_t size;
    unsigned bit_offset;
    unsigned bit_width;
} itk_field_info;
~~~

## Builder API

~~~c
void itk_record_builder_init(itk_record_builder *b, itk_record_kind kind,
                             itk_abi_kind abi);
itk_bool itk_record_field(itk_record_builder *b, const itk_type *type);
itk_bool itk_record_bitfield(itk_record_builder *b, const itk_type *type,
                             unsigned width);
itk_bool itk_record_finish(itk_record_builder *b, itk_record *out);
~~~

- `itk_record_field()` adds a regular field.
- `itk_record_bitfield()` adds a bitfield of the given width (in bits).
  The type must be an integer kind.
- `itk_record_finish()` finalizes the builder and produces the layout.

## Query Functions

~~~c
size_t itk_field_offset(const itk_record *r, size_t index);
size_t itk_record_size(const itk_record *r);
size_t itk_record_align(const itk_record *r);
itk_bool itk_field_bitfield_info(const itk_record *r, size_t index,
                                 itk_field_info *out);
~~~

## Bitfield Allocation Rules

Bitfields are packed according to the target ABI:

- SysV: consecutive bitfields pack into the same allocation unit when the
  types match and there is room.
- Win64: bitfields never cross allocation-unit boundaries.
- AAPCS: follows the ARM Procedure Call Standard.

## Usage Example

~~~c
#define ITK_PLATFORM_IMPLEMENTATION
#define ITK_CTYPES_IMPLEMENTATION
#define ITK_LAYOUT_IMPLEMENTATION
#include "InteropTk/itk_layout.h"

itk_record_builder b;
itk_record r;
itk_record_builder_init(&b, ITK_RECORD_STRUCT, itk_layout_default_abi());

itk_type t_int = itk_type_prim(ITK_KIND_INT);
itk_type t_char = itk_type_prim(ITK_KIND_CHAR);
itk_record_field(&b, &t_char);
itk_record_field(&b, &t_int);
itk_record_finish(&b, &r);

printf("struct size: %zu, align: %zu\n",
       itk_record_size(&r), itk_record_align(&r));
printf("field 0 offset: %zu, field 1 offset: %zu\n",
       itk_field_offset(&r, 0), itk_field_offset(&r, 1));
~~~
