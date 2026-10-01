# Chapter 26 — C++ Plugin Framework

Include `Grafitt-Plugin.hpp`.

## Core Concepts
- `IGraphPass<Subject>`
- pass metadata and diagnostics
- sequential `PassPipeline<Subject>`

## Built-In Eqsat Pass
- `EQSatTransformPass`
- `make_eqsat_transform_pass(...)`
