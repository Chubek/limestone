# Chapter 27 — Analysis Cache & Invalidation

## Cache Surface
- typed `analysis_key<T>`
- `pass_context` analysis cache helpers
- `IAnalysisPass<Subject, T>` and `make_analysis_pass(...)`

## Invalidation Policies
- preserve all
- invalidate all
- invalidate except list

Pipelines track generation and cache size after execution.
