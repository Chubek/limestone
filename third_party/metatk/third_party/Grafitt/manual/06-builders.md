# Chapter 06 — Builders

Grafitt builders simplify staged graph construction.

## Available Builders
- `builder::imperative_builder<Graph>`
- `builder::persistent_builder<Graph>`

## Typical Flow
- Start builder with direction.
- Add vertices/edges fluently.
- Finish with `build()`.
