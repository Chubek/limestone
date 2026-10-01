# Chapter 24 — GBIN Format

GBIN is Grafitt’s compact native binary graph format.

## Layout (v1)
- magic: `GBIN`
- version
- directed flag
- vertex/edge counts (`u32`, little-endian)
- length-prefixed strings for vertices and edges

## APIs
- `gbin::serialize(...)`
- `gbin::deserialize(...)`
