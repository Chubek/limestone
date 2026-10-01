# Chapter 03 — Core Graph Model

## Directionality
- `grafitt::direction::directed`
- `grafitt::direction::undirected`

## Core Types
- `edge<Vertex, EdgeLabel>`
- `imperative_graph<Vertex, EdgeLabel>`
- `persistent_graph<Vertex, EdgeLabel>`

## Invariants
- Vertex/edge membership checks are explicit.
- Undirected semantics preserve symmetric adjacency behavior.
- Algorithms assume standard Grafitt graph API contracts.
