# Chapter 05 — Persistent Graphs

`persistent_graph` returns updated graph values for edits.

## Why It Matters
- Safer branching and rollback.
- Functional-style pipelines.
- Cleaner snapshot semantics for multi-pass analysis.

## Pattern
```cpp
g = g.add_vertex(v);
g = g.add_edge(a, b, label);
```
