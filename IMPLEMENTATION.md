# Limestone implementation status

This checkout contains the implemented C/C++ foundation and subsystem scaffolding for Limestone plus the existing D MachineIR package and Infobank.

Implemented locally:

- Metacode ISA ingestion and architecture/register/operation normalization.
- Schedrow dependency-aware deterministic list scheduler.
- RegTL virtual/physical register model, linear-scan allocator, and verifier.
- Limeburg typed selection tree model and bottom-up dynamic-programming BURS selector.
- Unisel program/pattern/candidate model and explicit greedy development selector. The public solver entry remains gated on the required Satie backend.
- Bin2Bin generic decode/disassembly/translation-cache infrastructure with explicit unsupported cases.
- Tunah rule/session model. Equality saturation is gated on the required Equinox-NG backend.
- TraceML parser/compilation scaffold and MachineIR-facing lowering placeholder.
- Exolayer stable C ABI for native symbol registration/calls.
- VMWeave deterministic Lua VM DSL and C skeleton generation.
- Top-level Limestone C/C++ orchestration API and CLI.
- CMake build and smoke-test integration.

The archive does not contain `third_party/satie`, `third_party/equinox-ng`, `third_party/metatk`, or `third_party/exolangtk`. Components whose contracts require those dependencies therefore expose explicit adapter boundaries instead of silently replacing the mandated dependencies.

The D MachineIR package remains independently buildable with D tooling; this environment did not provide a D compiler/package manager, so its native test suite could not be run here.
