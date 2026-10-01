# Chapter 11 — Incremental Dijkstra Cache

Grafitt provides decrease-friendly incremental maintenance.

## APIs
- `incremental_dijkstra_build(...)`
- `incremental_dijkstra_relax_edge(...)`
- `incremental_dijkstra_relax_from_vertex(...)`
- `incremental_dijkstra_mark_stale(...)`
- `incremental_dijkstra_rebuild(...)`

## Guarantee
- Fast updates for insertions/decreases.
- Deletions/increases require stale marking and rebuild.
