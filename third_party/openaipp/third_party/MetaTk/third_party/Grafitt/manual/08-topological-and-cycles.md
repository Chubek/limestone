# Chapter 08 — Topological Order & Cycle Witnesses

## APIs
- `topological_sort(g)`
- `topological_sort_stable(g[, comp])`
- `is_dag(g)`
- `cycle_witness(g)`

## Result Contract
`topological_sort_result` exposes status + either:
- full order (`ok`), or
- cycle witness (`has_cycle`), or
- directedness rejection (`not_directed`).
