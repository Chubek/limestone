# Chapter 04 — Imperative Graphs

`imperative_graph` mutates in place.

## Common Operations
- `add_vertex(v)`
- `add_edge(src, dst, label)`
- `remove_edge_e(e)`
- `iter_vertex(...)`, `iter_edges_e(...)`

## When to Use
- High-throughput incremental construction.
- Rewrite workflows that mutate directly.
