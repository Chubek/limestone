# grafitt

`grafitt` is a header-first C++20 graph toolkit inspired by OCamlGraph.

Current repository focus:
- explicit mutable (`imperative_graph`) and immutable (`persistent_graph`) APIs
- small builder helpers
- traversal/query utilities
- Queryfitt AST + textual query parsing/execution
- concrete rewrite and GBIN v1 support

## Status

This repository is in an incremental build-out phase.

Implemented now:
- core graph storage and operations
- mutable and immutable graph interfaces
- OCamlGraph-style iteration/fold helpers
- core algorithms (`reachable`, `shortest_path`, `bfs_order`)
- Queryfitt textual parsing for `match` / `traverse` / `path` / `reachable` / `aggregate`
- Queryfitt execution for traversal, shortest path, reachability (including `max_depth`), aggregation, and edge-oriented pattern matches
  - aggregation supports `count`, `degree`, `sum/avg/min/max degree`, with optional `where` and `by all|vertex_type` (`by vertex_type` returns per-group results)
- concrete rewrite `apply_once()` behavior for string-vertex graphs (`lhs->rhs`)
- GBIN v1 serializer/deserializer with schema lookup hooks (`GRAFITT_SCHEMA_DIR`, fallback `specs/`)

## Layout

- `Grafitt.hpp`: main single-header library
- `specs/Queryfitt.ebnf`: textual Queryfitt grammar skeleton
- `specs/PatternMatching.schema.json`: rewrite pattern schema
- `specs/GBIN.sktl`: GBIN schema descriptor
- `manual/`: concise implementation-aligned manual chapters
- `examples/`: usage snippets and query text files

## Requirements

- C++20 compiler
- optional local headers (auto-detected via `__has_include`):
  - `DSLtk.hpp` (or `MetaTk/DSLtk/DSLtk.hpp`) — MetaTk DSLtk PEG parser + pattern matching, powers Queryfitt syntax validation and DSL-tagged subgraph matching. Exposed as `GRAFITT_HAS_DSLTK` (and the legacy `GRAFITT_HAS_DSLUTILS` alias).
  - `SerdeTk.hpp` — SerdeTk serialization header. Exposed as `GRAFITT_HAS_SERDETK`.

The header compiles without optional integrations; corresponding features degrade to narrow stubs. (The previous `matcheroni`/`DSLUtils.hpp` dependency was replaced by MetaTk DSLtk.)

## Quick Start

```cpp
#include "Grafitt.hpp"

int main() {
    grafitt::imperative_graph<int> g;
    g.add_vertex(1);
    g.add_edge(1, 2);
    return g.mem_edge(1, 2) ? 0 : 1;
}
```

Build:

```bash
g++ -std=c++20 -I. your_file.cpp -o your_app
```

## Graph APIs

### Mutable graph (`imperative_graph`)

Use when in-place updates are desired.

```cpp
#include "Grafitt.hpp"
#include <string>

int main() {
    grafitt::imperative_graph<std::string> g;
    g.add_vertex("a");
    g.add_edge("a", "b");
    g.add_edge("b", "c");

    g.remove_edge("b", "c");

    bool has_ab = g.mem_edge("a", "b");
    bool has_bc = g.mem_edge("b", "c");
    return (has_ab && !has_bc) ? 0 : 1;
}
```

### Immutable graph (`persistent_graph`)

Use when updates must return new graph values without mutating earlier versions.

```cpp
#include "Grafitt.hpp"

int main() {
    grafitt::persistent_graph<int> g0;
    auto g1 = g0.add_vertex(10);
    auto g2 = g1.add_edge(10, 20);

    // g0 unchanged, g1 has only vertex 10, g2 has edge 10->20
    return (!g0.mem_vertex(10) && g1.mem_vertex(10) && g2.mem_edge(10, 20)) ? 0 : 1;
}
```

### Builders

```cpp
#include "Grafitt.hpp"

int main() {
    using G = grafitt::imperative_graph<int>;
    auto g = grafitt::builder::imperative_builder<G>()
        .vertex(1)
        .vertex(2)
        .edge(1, 2)
        .build();

    return g.nb_edges() == 1 ? 0 : 1;
}
```

## Algorithms

Available helpers in `grafitt::algo` include:
- `reachable(g, src, dst)`
- `shortest_path(g, src, dst)`
- `dijkstra_shortest_paths(g, source, weight_of[, zero])`
- `dijkstra_shortest_path(g, source, destination, weight_of[, zero])`
- `bellman_ford_shortest_paths(g, source, weight_of[, zero])`
- `bellman_ford_shortest_path(g, source, destination, weight_of[, zero])`
- `weighted_shortest_paths(g, source, weight_of[, strategy, zero])`
- `weighted_shortest_path(g, source, destination, weight_of[, strategy, zero])`
- `incremental_dijkstra_build(g, source, weight_of[, zero])`
- `incremental_dijkstra_relax_edge(g, src, dst, weight_of, cache)`
- `incremental_dijkstra_relax_from_vertex(g, touched, weight_of, cache)`
- `incremental_dijkstra_mark_stale(cache)`
- `incremental_dijkstra_rebuild(g, weight_of, cache)`
- `bfs_order(g, root)`
- `topological_sort(g)`
- `topological_sort_stable(g[, comp])`
- `cycle_witness(g)`
- `is_dag(g)`
- `weakly_connected_components(g)`
- `strongly_connected_components(g)`
- `condensation_graph(g)`
- `condensation_graph_from_scc(g, scc)`
- `validate_condensation_graph_detailed(g, c)`
- `validate_condensation_graph(g, c)`
- `condensation_dag_cover(g)`
- `condensation_dag_cover(g[, options])`
- `validate_condensation_dag_cover(g, cover)`
- `bfs_forest_cover(g[, options])`
- `validate_bfs_forest_cover(g, cover)`
- `score_cover(cover, objective)`
- `structurally_equal(lhs, rhs[, options])`
- `representationally_equal(lhs, rhs)`
- `graph_isomorphism(lhs, rhs[, options])`
- `subgraph_match(pattern, target[, options])`
- `subgraph_match_heuristic(pattern, target, options)`
- `degree(g, v)`
- `vertices(g)`, `edges(g)`

Example:

```cpp
#include "Grafitt.hpp"

int main() {
    grafitt::imperative_graph<int> g;
    g.add_edge(1, 2);
    g.add_edge(2, 3);

    auto ok = grafitt::algo::reachable(g, 1, 3);
    auto path = grafitt::algo::shortest_path(g, 1, 3);
    return (ok && path && path->size() == 3) ? 0 : 1;
}
```

Topological sorting returns a rich status object:
- `ok`: valid order in `result.order`
- `has_cycle`: cycle witness in `result.cycle_witness`
- `not_directed`: DAG ordering not applicable

Runnable example:
- `examples/topological-sort.cpp`

Weighted shortest paths:
- `dijkstra_shortest_paths` returns `dijkstra_result` with:
  - status (`ok`, `source_not_found`, `negative_edge`)
  - distance map
  - predecessor map
  - `distance_to(v)` and `path_to(v)` helpers
  - negative-edge witness metadata when rejected

Runnable example:
- `examples/weighted-shortest-path.cpp`

Bellman-Ford shortest paths:
- supports negative edges
- returns `bellman_ford_result` with:
  - status (`ok`, `source_not_found`, `negative_cycle`)
  - distance/predecessor maps
  - `distance_to(v)` and `path_to(v)` helpers
  - `negative_cycle_witness` for reachable negative cycles

Runnable example:
- `examples/bellman-ford.cpp`

Unified weighted shortest-path facade:
- `weighted_shortest_path_strategy`:
  - `auto_select` (Dijkstra for non-negative graphs, Bellman-Ford otherwise)
  - `dijkstra`
  - `bellman_ford`
- `weighted_shortest_path_result` exposes:
  - `strategy_used`
  - `ok()`, `reachable(v)`, `distance_to(v)`, `path_to(v)`
  - accessors `as_dijkstra()` / `as_bellman_ford()` for algorithm-specific diagnostics

Runnable example:
- `examples/weighted-shortest-path-unified.cpp`

Incremental shortest-path support (decrease-friendly Dijkstra cache):
- build cache: `incremental_dijkstra_build(...)`
- supported fast updates:
  - edge insertion or weight decrease via `incremental_dijkstra_relax_edge(...)`
  - frontier refresh from touched vertex via `incremental_dijkstra_relax_from_vertex(...)`
- unsupported updates (edge deletion / weight increase):
  - call `incremental_dijkstra_mark_stale(cache)`
  - then `incremental_dijkstra_rebuild(...)` before further incremental use
- result/status types:
  - `incremental_dijkstra_cache` with `ok()`, `stale()`, `distance_to()`, `path_to()`
  - `incremental_dijkstra_update_result` with diagnostics counters

Runnable example:
- `examples/incremental-dijkstra.cpp`

Graph comparison and isomorphism:
- `structurally_equal(lhs, rhs)` / `representationally_equal(lhs, rhs)`:
  exact identity-based equality (same vertex IDs and labeled edge identities).
- `graph_isomorphism(lhs, rhs[, options])`:
  exact isomorphism with:
  - fast rejects (directedness/size/degree profiles/edge-label profile)
  - witness maps (`forward_map`, `reverse_map`) on success
  - configurable labeled vs unlabeled matching via `isomorphism_options`

Runnable example:
- `examples/graph-isomorphism.cpp`

Subgraph matching (exact + constrained heuristic):
- `subgraph_match(pattern, target[, options])` returns `subgraph_match_result`:
  - status (`match_found`, `no_match`, `directed_mismatch`,
    `pattern_larger_than_target`, `search_budget_exhausted`)
  - one or more witness mappings (`vertex_bindings`, `reverse_vertex_bindings`)
  - edge witnesses mapping each pattern edge to a supporting target edge
  - search diagnostics (`search_order`, `search_steps`, `exhaustive`)
- `subgraph_match_options` supports:
  - labeled/unlabeled matching (`match_edge_labels`)
  - induced-subgraph constraints (`induced`)
  - exact vs constrained search (`search_mode`)
  - per-vertex candidate predicate (`vertex_compatible`)
  - explicit heuristic budgets (`max_search_steps`, `max_candidates_per_vertex`)
  - bounded witness collection (`max_witnesses`)
- rewrite-ready bindings:
  - each witness exposes `to_named_bindings(...)` to convert bindings into
    `std::map<std::string, std::string>` metadata style payloads.

Runnable examples:
- `examples/subgraph-matching.cpp`
- `examples/subgraph-matching-induced.cpp`

Match-driven rewriting bridge:
- `rewrite::apply_first_subgraph_match(g, pattern, transform[, options])`:
  finds the first witness and applies a user transform callback.
- `rewrite::apply_first_subgraph_match_copy(g, pattern, transform[, options])`:
  value-style variant returning `{graph_after, matched_rewrite_result}`.
- `rewrite::matched_rewrite_result` contains:
  - `applied` flag
  - match status
  - captured witnesses
  - rewrite-ready string bindings
- adapters:
  - `rewrite::to_query_match_result(witness)`
  - `rewrite::to_query_match_results(witnesses)`
  for interop with Queryfitt-style `match_result`.

Runnable example:
- `examples/rewrite-subgraph-match.cpp`

Strategy-driven rewrite scheduling:
- `rewrite::matched_rewrite_strategy`:
  - `first`, `all`, `bounded`, `until_fixpoint`
- `rewrite::matched_rewrite_run_options`:
  - `strategy`
  - `max_applications` (for bounded/global cap)
  - `max_rounds` (for fixpoint loop ceilings)
- `rewrite::apply_subgraph_matches(g, pattern, transform[, match_options, run_options])`
  applies according to strategy with deterministic witness ordering.
- `rewrite::apply_subgraph_matches_copy(...)` returns value-style pair.
- `rewrite::matched_rewrite_run_result` includes:
  - `changed`, `final_match_status`
  - per-application witness snapshots
  - `matched_rewrite_stats` (rounds, applications, observed matches, limits hit)

Runnable example:
- `examples/rewrite-subgraph-strategies.cpp`

Optional `DSLUtils` tag-constrained interop (`#if GRAFITT_HAS_DSLUTILS`):
- `rewrite::subgraph_match_with_dsl_tags<PatternTagPattern, TargetTagPattern>(...)`
  combines `dsl::pattern<S>::matches(...)` with `subgraph_match` vertex filtering.
- `rewrite::apply_first_subgraph_match_with_dsl_tags<...>(...)`
  combines DSL-tag filtering and first-match rewrite application.
- This remains lightweight string/tag gating (not full regex/query semantics).

Runnable example:
- `examples/rewrite-subgraph-dsl-tags.cpp`

Connectivity/decomposition helpers:
- `weakly_connected_components(g)` computes components in the underlying
  undirected sense (for directed graphs: uses both predecessor/successor
  adjacency).
- `strongly_connected_components(g)` computes SCCs (Tarjan-based).
- `condensation_graph(g)` builds an SCC condensation DAG with provenance:
  - `components`: SCC id -> original vertices
  - `component_of`: original vertex -> SCC id
  - `dag`: directed acyclic graph of SCC transitions
  - `edge_witnesses`: one original-edge witness per condensation edge
- `condensation_graph_from_scc(g, scc)` reuses a precomputed SCC result.
- `validate_condensation_graph(g, result)` checks mapping consistency,
  edge lifting, and DAG property.
- `validate_condensation_graph_detailed(g, result)` returns
  `condensation_validation_result` with failure category and witness metadata.

Runnable example:
- `examples/components-condensation.cpp`
- `examples/connected-components.cpp`
- `examples/condensation-graph.cpp`
- `examples/condensation-diagnostics.cpp`

Covering/decomposition helpers:
- `condensation_dag_cover(g[, options])`:
  partitions SCC-condensation DAG into weakly connected DAG pieces with
  component/original-vertex provenance and edge witnesses.
  - strategy options: `weak_component_partition`, `topo_chunked`
  - objective hinting: `cover_objective`
- `validate_condensation_dag_cover(g, cover)`:
  validates full component coverage, disjoint assignment, DAG-piece acyclicity,
  and edge coherence.
- `bfs_forest_cover(g[, options])`:
  builds weakly connected BFS forest pieces with parent map and tree-edge
  provenance.
  - strategy options: `weak_component_roots`, `degree_desc_root_priority`
  - optional piece-size cap for finer covers.
- `validate_bfs_forest_cover(g, cover)`:
  validates unique coverage, parent relations, and tree-edge cardinality.
- both covers expose `cover_quality_metrics`.
- `score_cover(cover, objective)` yields objective-aware numeric score for
  candidate comparison (`minimize_piece_count`, `balance_piece_sizes`,
  `maximize_locality`).

Runnable example:
- `examples/decomposition-covering.cpp`
- `examples/decomposition-cover-scoring.cpp`

Visualization/export (`grafitt::vizz`):
- `to_dot(g[, options])` for GraphViz DOT export
- `to_tikz(g[, options])` for LaTeX TikZ export
- `to_graphjs(g[, options])` for Graph.js-style JSON export
- `to_dot(condensation[, options])` for SCC-condensation DAG export
- `to_graphjs(condensation[, options])` for SCC-condensation Graph.js export
- `to_dot(condensation_cover[, options])` for DAG-cover clustered export
- `to_graphjs(bfs_forest_cover)` for forest-cover Graph.js export
- hooks:
  - custom vertex/edge IDs and labels
  - per-vertex/per-edge style callbacks
  - condensation node/edge style + component-contents labeling
  - class-oriented hooks for Graph.js consumers
- style presets:
  - `dot_vertex_highlight_style(...)` for selected vertices
  - `dot_edge_path_highlight_style<Graph>(...)` for path highlighting
  - `dot_edge_cycle_highlight_style<Graph>(...)` for cycle witness highlighting
  - `condensation_component_size_style<Vertex>(...)` for SCC size-based emphasis

Runnable example:
- `examples/export-dot-tikz.cpp`
- `examples/export-graphjs.cpp`

## Queryfitt

### C++ query construction

```cpp
#include "Grafitt.hpp"

int main() {
    auto q = grafitt::queryfitt::shortest_path_between("alice", "bob");
    (void)q;
    return 0;
}
```

### Text query parsing and execution

`parse_text()` accepts the textual style used in `examples/*.qfitt`:
- optional metadata block (`--- ... ---`)
- `match`, `traverse`, `path`, `reachable`, `aggregate`
- `in "..." { ... }` body with clause-specific fields

Aggregation-focused runnable examples:
- `examples/aggregation-count.cpp`
- `examples/aggregation-where.cpp`
- `examples/aggregation-by-vertex-type.cpp`
- `examples/aggregation-degree-stats.cpp`
- `examples/aggregation-degree-by-vertex-type.cpp`
- `examples/aggregation-parse-text.cpp`
- textual samples: `examples/aggregation-by-type.qfitt`, `examples/aggregation-where-by-type.qfitt`, `examples/aggregation-degree-by-type.qfitt`

```cpp
#include "Grafitt.hpp"
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;
    G g;
    g.add_edge("Alice", "Bob", "knows");
    g.add_edge("Bob", "Carol", "knows");

    auto parsed = grafitt::queryfitt::parse_text(
        "path in \"my-graphs/friendship-graph.gbin\" {\n"
        "  from \"Alice\"\n"
        "  to \"Carol\"\n"
        "  shortest\n"
        "}\n"
    );
    if (!parsed) return 1;

    auto result = grafitt::queryfitt::execute(
        g,
        parsed->value,
        [](const std::string& v) { return v; },
        [](const std::string&) { return true; },
        [](const auto&, const auto&) { return true; }
    );
    (void)result;
    return 0;
}
```

## Rewriting

```cpp
#include "Grafitt.hpp"
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;
    G g;
    g.add_edge("a", "b", "x");
    g.add_edge("a", "c", "y");

    grafitt::rewrite::rule r {
        .name = "redirect-a",
        .match = grafitt::queryfitt::shortest_path_between("a", "b"),
        .replacement = "a->z"
    };

    auto g2 = grafitt::rewrite::apply_once(g, r);
    return g2.mem_edge("a", "z") ? 0 : 1;
}
```

Equality saturation scaffold (`grafitt::eqsat`):
- core types:
  - `eqsat::term`, `eqsat::enode`, `eqsat::eclass`, `eqsat::egraph`
  - `eqsat::rewrite_rule`
- `eqsat::saturation_options`, `eqsat::saturation_summary`
  - scheduler-aware saturation via `rule_schedule_policy`
  - trace capture via `eqsat::saturation_trace`
- `eqsat::extraction_result`
- extraction customization:
  - `eqsat::extraction_options` (custom `node_cost`, tie-break policy,
    model naming, explanation capture)
  - `eqsat::extract_best(eg, root, options)`
  - `eqsat::extract_best_with_explanation(eg, root, options)`
  - explanation payload types:
    - `eqsat::extraction_explanation`
    - `eqsat::extraction_decision`
- workflow:
  - build e-graph with `egraph::add_term(seed)`
  - run `eqsat::saturate(eg, rules[, options])` or
    `eqsat::saturate(eg, rules, trace, options, schedule)`
  - extract representative with `eqsat::extract_best(eg, root)`
  - inspect explanations via:
    - `eqsat::explain_class(trace, cid)`
    - `eqsat::explain_rule(trace, "rule-name")`
- rewrite interop:
  - `rewrite::to_eqsat_rule(rule)`
  - `rewrite::run_eqsat(seed, eqsat_rules[, options])`
  - `rewrite::run_eqsat_from_rules(seed, rewrite_rules[, options])`

Runnable example:
- `examples/eqsat-basic.cpp`

## C++ Plugin/Pass Framework (`Grafitt-Plugin.hpp`)

`Grafitt-Plugin.hpp` provides a header-only in-process pass framework:
- pass base: `plugin::IGraphPass<Subject>`
- pass metadata/diagnostics:
  - `plugin::pass_metadata`, `plugin::pass_kind`
  - `plugin::pass_diagnostic`, `plugin::diagnostic_severity`
- orchestration:
  - `plugin::PassPipeline<Subject>`
  - `plugin::pipeline_result`
  - analysis cache lifecycle in `plugin::pass_context<Subject>`:
    - `put_analysis<T>(key, value)`, `get_analysis<T>(key)`, `has_analysis<T>(key)`
    - invalidation generation tracking

Analysis invalidation model:
- `plugin::analysis_invalidation_policy`:
  - `preserve_all`
  - `invalidate_all`
  - `invalidate_except_list`
- each `plugin::pass_result` can declare invalidation policy and preserved keys.
- `plugin::pass_metadata::declares_preserved_analyses` provides compile-time-ish
  preservation intent that is merged with runtime `pass_result::preserved_analyses`.
- pipeline reports cache outcomes:
  - `pipeline_result::final_analysis_generation`
  - `pipeline_result::final_cached_analyses`
- pipeline debug controls:
  - `plugin::pipeline_options{ .emit_invalidation_diagnostics = true }`
  - `PassPipeline::set_options(...)` emits explicit invalidation decisions into
    diagnostics stream.

Typed analysis helpers:
- `plugin::analysis_key<T>`
- `plugin::IAnalysisPass<Subject, AnalysisT>`
- `plugin::make_analysis_pass<Subject, AnalysisT>(meta, key, compute_fn)`
  for concise cached analysis pass construction.

Built-in eqsat pass wrapper:
- `plugin::eqsat_pass_options`
- `plugin::eqsat_pass_result`
- `plugin::EQSatTransformPass<Subject, ToTerm, ApplyExtracted>`
- `plugin::make_eqsat_transform_pass(...)`
- when eqsat rewrites change the subject, analysis cache invalidates by default.

Named pass registry/factory APIs:
- `plugin::PassRegistry<Subject>`
  - `register_factory(meta, factory)`
  - `make(name)`
  - `list()`
  - `append_to_pipeline(pipeline, {"pass-a", "pass-b", ...})`
- `plugin::pass_registry_status`
- `plugin::pipeline_build_result`
- eqsat convenience registration:
  - `plugin::register_eqsat_transform_pass(...)`
- category queries:
  - `list_by_kind(kind)`
  - `names_by_kind(kind)`
- declarative presets:
  - `plugin::pipeline_preset`
    - `analysis_first`
    - `normalize_rewrite_eqsat`
    - `validate_export`
  - `preset_names(preset)`
  - `append_preset_to_pipeline(pipeline, preset)`

Pipeline trace/report export:
- `plugin::pass_run_record` now captures:
  - pass outcome
  - invalidation policy
  - preserved-analysis count
  - cache/generation before/after
  - invalidation summary string
- exporters:
  - `plugin::to_text_report(pipeline_result)`
  - `plugin::to_json_report(pipeline_result)`

Runnable example:
- `examples/plugin-eqsat-pass.cpp`

## C ABI Module Interface (`Grafitt-Module.h`)

`Grafitt-Module.h` defines a strict C99-compatible, versioned module ABI:
- required exports:
  - `GM_ModMetadata`
  - `GM_ModStart`
  - `GM_ModEnd`
- core ABI records:
  - `GM_ModMetadata_Record`
  - `GM_HostAPI`
  - `GM_CapabilityDecl`
  - `GM_CapabilityTable`
- compatibility/validation helpers:
  - `GM_validate_metadata(...)`
  - `GM_validate_host_api(...)`
  - `GM_validate_capability_tables(...)`
- optional dependency metadata:
  - `GM_DependencyDecl`
  - `GM_ModMetadata_Record::dependency_count`
  - `GM_ModMetadata_Record::dependencies`

ABI guarantees in this header:
- fixed-width scalar fields and explicit `size` versioning fields
- no C++ types or exceptions across boundary
- capability-oriented registration via host callback table

Runnable C example:
- `examples/module-minimal.c`

Host-side lifecycle helper (`Grafitt-ModuleHost.hpp`):
- `modulehost::module_manager` supports:
  - metadata staging (`stage_module`)
  - start/stop lifecycle (`start_module`, `stop_module`)
  - dependency-aware global startup (`start_all`)
  - reverse startup-order teardown (`stop_all`)
  - dependency order preview (`startup_plan`)
  - active module snapshots (`active_modules`)
  - host diagnostics stream (`diagnostics`)
- host policies:
  - `host_options::reject_duplicate_capabilities`
    - reject or allow capability collisions (`capability_id` + `interface_version`)
    - emits explicit diagnostics in both modes
- reporting:
  - `to_text_report()`
  - `to_json_report()`
  - lifecycle events available via `lifecycle_events()`
- typed records:
  - `modulehost::module_runtime_record`
  - `modulehost::host_diagnostic`
  - `modulehost::host_result`, `modulehost::host_status`

Runnable host example:
- `examples/module-host-minimal.cpp`

## GBIN and Schema Lookup

`grafitt::gbin` provides:
- `schema_root()`
- `default_schema_path()`
- `serialize(graph)` (GBIN v1 bytes)
- `deserialize<Graph>(bytes)` (currently `std::string` vertices, label `std::string` or `unit`)
- `inspect_header(bytes)` (lightweight header probe)
- `deserialize_detailed<Graph>(bytes[, strict_trailing_bytes])` with rich status:
  - `decode_status` (`ok`, `bad_magic`, `unsupported_version`, `truncated`, etc.)
  - parsed `gbin_header_info`
  - decode offset and message diagnostics

GBIN v1 layout:
- magic: `GBIN`
- version: `1`
- directed flag: `0|1`
- vertex count (`u32`, little-endian)
- edge count (`u32`, little-endian)
- vertices and edges as length-prefixed strings

Environment variable:
- `GRAFITT_SCHEMA_DIR`: custom schema directory
- fallback: `specs/`

Example:

```cpp
#include "Grafitt.hpp"
#include <iostream>

int main() {
    std::cout << grafitt::gbin::default_schema_path().string() << "\n";
    return 0;
}
```

Runnable diagnostics example:
- `examples/gbin-diagnostics.cpp`

## Specs and Manual

- Query grammar: `specs/Queryfitt.ebnf`
- Pattern schema: `specs/PatternMatching.schema.json`
- GBIN schema descriptor: `specs/GBIN.sktl`
- manual index: `manual/README.md`

## Smoke Test Ideas

After editing `Grafitt.hpp` or parser-related code, run at least:
- header include/compile smoke
- one mutable + immutable behavior check
- one Queryfitt parse+execute check
- one rewrite check
- one GBIN round-trip check

A minimal compile smoke:

```bash
cat >/tmp/grafitt_smoke.cpp <<'CPP'
#include "Grafitt.hpp"
int main() { return 0; }
CPP

g++ -std=c++20 -I. /tmp/grafitt_smoke.cpp -o /tmp/grafitt_smoke
```

## Notes on Naming

Repository convention currently uses `Grafitt.hpp` (capital `G`) as the main include path.
