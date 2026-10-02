# Limestone implementation status

This checkout contains Limestone's C/C++ foundation and subsystem implementations,
the D MachineIR package, the Infobank, and the supplied third-party integration
dependencies. Subsystem implementations cover focused vertical slices; full
architecture and IL-adapter contracts remain defined by their local `AGENTS.md`.

Implemented locally:

- Metacode ISA ingestion and architecture/register/operation normalization.
- Schedrow dependency-aware deterministic list scheduler.
- RegTL virtual/physical register model, linear-scan allocator, and verifier.
- Limeburg typed selection tree model and bottom-up dynamic-programming BURS selector.
- Unisel program/pattern/candidate and constraint models, Satie-backed selection,
  and an explicit greedy development selector.
- Bin2Bin generic decode/disassembly/translation-cache infrastructure with explicit unsupported cases.
- Tunah Equinox-NG-backed bounded saturation, source-aware transactional rule-file
  compilation through EkippX/SExprTk/DSLtk, host-analysis guards, structured term
  ingress/egress, configurable checked extraction costs, and optional traces.
- Tunah's shared instruction vocabulary and 13 tuner files (239 rules), with
  width/overflow, memory-forwarding, and scalar/vector-shape regressions. See
  `tunah/README.md` for semantics and adapter requirements.
- TraceML parsing, closed integer evaluation, and portable MachineIR-facing
  graph lowering.
- Exolayer stable C ABI for native symbol registration/calls.
- VMWeave deterministic Lua VM DSL and C skeleton generation.
- Top-level Limestone C/C++ orchestration API and CLI, including standalone
  Tunah term optimization with explicit rule files, costs, limits, and traces.
- CMake build, focused subsystem tests, tuner corpus checks, and CLI integration.
- All ten DParser grammars under `parsers/`, matching `.absyn` schemas, and the
  Perl `scripts/syngen.pl` AST/visitor generator. `limestone_parsers` exposes
  source-located owning C++ syntax trees, mutable/const visitors, and typed
  parsing entry points. See `parsers/README.md` for formats and generation.

The required Satie, Equinox-NG, MetaTk, ExolangTk, and DParser integrations are
supplied in this checkout and consumed through repository CMake targets.
Bin2Bin's optional persistent cache additionally requires the configured LMDB
headers/library.

The D MachineIR package remains independently buildable with D tooling. The
current environment provides that tooling; its native suite runs as the
`machine-ir` CTest test.

Remaining Tunah boundaries: concrete bi-directional IL adapters and their type,
effect, and target-legality analyses; complete preprocessing/source-map support;
preemptible time budgets inside vendor operations; and full e-node derivation
provenance. Generic rule loading rejects unavailable host predicates explicitly.

Validation: CMake configuration/build and all 17 CTest tests pass, including
parser/AST/visitor regressions, schema-generator checks, and the D MachineIR
suite. The parser runtime, all generated tables/ASTs, and the parser/corpus tests
also pass AddressSanitizer, UndefinedBehaviorSanitizer, and leak detection.
The earlier Tunah round passed focused sanitizer tests; its comparative
optimizer timings and output costs are recorded in `tunah/BENCHMARKS.md`.
