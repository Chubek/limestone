# Examples

Quick map of the sample programs and query files.

| File | Focus |
|---|---|
| `mutable-graph.cpp` | Mutable graph basics (insert, iterate, count) |
| `builder.cpp` | Builder-based graph construction |
| `write-graph.cpp` | Serialize a small graph to GBIN |
| `write-gbin.cpp` | GBIN serialization helper usage |
| `reachability.cpp` | Reachability and shortest-path algorithms |
| `queryfitt-exec.cpp` | Native Queryfitt query execution |
| `find-pattern.cpp` | Embedded pattern DSL construction |
| `graph-rewriting.cpp` | Rewrite rule application (`apply_once`) |
| `subgraph-matching.cpp` | Exact + constrained heuristic subgraph matching |
| `subgraph-matching-induced.cpp` | Induced vs non-induced subgraph matching behavior |
| `rewrite-subgraph-match.cpp` | Rewrite flow driven by subgraph match witnesses/bindings |
| `rewrite-subgraph-strategies.cpp` | Strategy-based subgraph rewrite scheduling and stats |
| `rewrite-subgraph-dsl-tags.cpp` | Optional DSLUtils pattern-tag constrained matching/rewrite |
| `decomposition-covering.cpp` | DAG/forest covering with validation and export hooks |
| `decomposition-cover-scoring.cpp` | Strategy/objective-driven cover comparison and scoring |
| `eqsat-basic.cpp` | Minimal equality-saturation build/saturate/extract workflow |
| `plugin-eqsat-pass.cpp` | C++ pass pipeline using built-in eqsat transform pass |
| `module-minimal.c` | Minimal C ABI module exporting metadata/start/end symbols |
| `module-host-minimal.cpp` | Host-side module staging/start/stop lifecycle management |
| `gbin-diagnostics.cpp` | GBIN header inspection and detailed decode diagnostics |
| `aggregation-count.cpp` | `count vertices` scalar aggregation |
| `aggregation-where.cpp` | Aggregation with `where` filtering |
| `aggregation-by-vertex-type.cpp` | Grouped `count vertices by vertex_type` |
| `aggregation-degree-stats.cpp` | Scalar degree stats (`sum degree`) |
| `aggregation-degree-by-vertex-type.cpp` | Grouped degree stats (`avg degree by vertex_type`) |
| `aggregation-parse-text.cpp` | Text query parse + execute for grouped aggregation |
| `aggregation-kitchen-sink.cpp` | Multiple aggregate forms in one runnable file |
| `aggregation.qfitt` | Minimal textual aggregate query |
| `aggregation-by-type.qfitt` | Textual grouped count query |
| `aggregation-where-by-type.qfitt` | Textual grouped query with `where` |
| `aggregation-degree-by-type.qfitt` | Textual grouped degree query |
| `path.qfitt` | Textual path query |
| `reachability.qfitt` | Textual reachability query |
| `traverse.qfitt` | Textual traversal query |
| `third-wheelers.qfitt` | Textual match-pattern sample |
