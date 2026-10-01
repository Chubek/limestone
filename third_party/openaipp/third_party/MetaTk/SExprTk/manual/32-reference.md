# Chapter 32 — API Reference

## Core types

- `NodeKind`: typed node discriminator.
- `Atom`: scalar or list value with checked accessors.
- `Cell`: head plus tail representation.
- `List`: ordered cell vector.
- `Source`: named source text or file content.
- `Cartable`: root tree, metadata, events, errors.
- `Serializer`: s-expression, JSON, and TOML emitters.

## Processing interfaces

- `SExprTk::parse`, `run`, `semanticize`, `analyze`, `transform`, `package`.
- `Analyzer`, `ShapeAnalyzer`, `SymbolAnalyzer`.
- `Transformer`, `FlattenTransformer`, `ConstantFoldTransformer`, `MapTransformer`.
- `Semanticizer`, `LuaKernelSemanticizer`, `S7KernelSemanticizer`.
- `Kernel`, `LuaKernel`, `S7Kernel`.

## XAS

- C constants, event/status enums, frame/event structs.
- kind-name conversion and validation.
- frame encode/decode/validate.
- event/frame initialization.
- source/sink pump.
- C++ `XASEvent`, `XASEventDispatcher`, `CartableDispatcher`.

## Stability constraints

XAS event numeric values and wire offsets are protocol ABI. Append new event kinds; do not renumber existing kinds. Treat borrowed payload pointers and parser-retained event streams as explicit lifetime/version boundaries.
