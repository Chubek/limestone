# Chapter 29 — Custom Passes and Interpreters

Custom analyzers should:

1. define a stable fact-key schema;
2. recurse through both list heads and cell tails;
3. avoid mutating the cartable;
4. distinguish parse errors from semantic findings.

Custom transformers should:

1. copy or rebuild the cartable;
2. preserve metadata intentionally;
3. decide whether old XAS events remain valid;
4. document invariants of the rewritten tree.

Custom semanticizers implement `semanticize`; custom kernels implement `evaluate`. Use `Semantics::errors` for recoverable evaluation failure and keep `rendered` meaningful only on success.

A domain interpreter can evaluate `Cell` directly, as shown by the PikoLisp example. Runtime ownership, environments, special forms, and coercion rules are entirely application-defined.

Do not infer lexical structure from `SymbolAnalyzer`; it is a byte-level symbol collector, not a scope analyzer.
