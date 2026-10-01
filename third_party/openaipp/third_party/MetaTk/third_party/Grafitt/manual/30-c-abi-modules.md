# Chapter 30 — C ABI Modules

Include `Grafitt-Module.h`.

## Required Exports
- `GM_ModMetadata`
- `GM_ModStart`
- `GM_ModEnd`

## Metadata/Capability Model
- versioned records with explicit `size`
- capability declarations/tables
- optional dependency declarations
- strict C types only across boundary
