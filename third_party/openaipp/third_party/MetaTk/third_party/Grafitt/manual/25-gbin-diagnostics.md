# Chapter 25 — GBIN Diagnostics

## New Diagnostic APIs
- `gbin::inspect_header(bytes)`
- `gbin::deserialize_detailed<Graph>(bytes, strict_trailing_bytes)`

## Rich Status
`decode_status` reports exact failure category:
- bad magic
- unsupported version
- truncation
- malformed directed flag
- decode failures
- trailing bytes
