# Chapter 09 — Unweighted Paths

Use unweighted BFS path helpers for topology-based routing.

## APIs
- `shortest_path(g, src, dst)`
- `reachable(g, src, dst)`

## Behavior
- Returns `std::optional<std::vector<Vertex>>` for paths.
- Missing vertices or disconnected nodes return no path.
