# Chapter 15 — XAS Wire Format

Every datagram frame uses a 24-byte big-endian header:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | magic `XA` |
| 2 | 1 | version `1` |
| 3 | 1 | flags |
| 4 | 1 | event kind |
| 5 | 1 | reserved |
| 6 | 2 | source id |
| 8 | 8 | sequence |
| 16 | 2 | line |
| 18 | 2 | column |
| 20 | 4 | payload length |
| 24 | n | payload |

`SEXPRTK_XAS_FLAG_EOS` is bit zero. Other flag bits are reserved. Payload maximum is 65,507 bytes; maximum frame size is 65,531 bytes.

The decoder validates magic, version, kind, and declared payload extent. It permits trailing bytes after the declared payload. It ignores the reserved header byte and does not enforce reserved flag bits.

Decoded payload points into the caller’s frame buffer. Buffer lifetime therefore dominates event lifetime.
