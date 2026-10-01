# Chapter 16 — XAS C API

`SExprTk-XASEvent.h` is C89/C90-compatible at the interface level, using `<stddef.h>` and `<stdint.h>`. Core types:

- `sexprtk_xas_event`;
- `sexprtk_xas_frame`;
- `sexprtk_xas_event_source`;
- `sexprtk_xas_event_sink`.

Helpers:

- `sexprtk_xas_event_kind_name`;
- `sexprtk_xas_event_kind_from_name`;
- `sexprtk_xas_event_kind_valid`;
- frame encode/decode/validate;
- payload-length query;
- source-to-sink pump;
- event/frame initialization.

The payload pointer is borrowed. Frame storage is caller-owned. `frame.capacity` must cover the complete encoded frame; the encoder sets `frame.length` on success.

The header declares functions but does not define transport, allocation, or event production. C++ inclusion of `SExprTk.hpp` supplies inline definitions with C linkage. A pure-C translation unit requires a host implementation linked separately.
