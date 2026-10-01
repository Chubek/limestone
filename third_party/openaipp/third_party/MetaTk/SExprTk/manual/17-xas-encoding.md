# Chapter 17 — XAS Encoding and Decoding

C++ callers can use the C API with fixed storage:

```cpp
std::array<unsigned char, SEXPRTK_XAS_MAX_DATAGRAM> bytes;
sexprtk_xas_frame frame;
sexprtk_xas_frame_init(&frame);
frame.bytes = bytes.data();
frame.capacity = bytes.size();
sexprtk_xas_frame_encode(&event, &frame);
```

Encoding rejects invalid kinds and payloads exceeding `SEXPRTK_XAS_MAX_PAYLOAD`. Null payload with nonzero length is not rejected by the reference encoder; it emits a header without payload bytes, so producers must maintain pointer/length consistency.

Validation returns `BAD_MAGIC`, `BAD_VERSION`, `TRUNCATED`, `BAD_KIND`, or `INVALID`. `TOO_LARGE` is an encode/storage error; frame validation does not reject a declared payload merely because it exceeds the protocol maximum.

Decoding initializes the event, reconstructs big-endian fields, and borrows payload bytes. Copy payload before releasing or reusing the frame.
