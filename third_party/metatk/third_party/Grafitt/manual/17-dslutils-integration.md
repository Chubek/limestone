# Chapter 17 — DSLtk Integration

When MetaTk `DSLtk.hpp` is available (detected via `__has_include`, exposed as
`GRAFITT_HAS_DSLTK` / the legacy `GRAFITT_HAS_DSLUTILS` alias):
- `subgraph_match_with_dsl_tags<...>(...)`
- `apply_first_subgraph_match_with_dsl_tags<...>(...)`
- the Queryfitt text parser gains PEG-based syntax validation
  (`queryfitt_structure_valid`) backed by `dsl::create_peg_definition()` /
  `dsl::PEGDefinition`, and the Queryfitt DSL struct is built on the
  `dsl::DSL<...>` mixin framework.

This integration is lightweight pattern/tag gating plus PEG grammar
validation, not full regex/query semantics. It supersedes the old
`DSLUtils.hpp`/matcheroni dependency.
