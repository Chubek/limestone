# Chapter 20 — Built-in Analyzers

`ShapeAnalyzer` emits:

- `atoms`;
- `lists`;
- `depth`.

It recursively traverses list-valued heads and cell tails. `depth` starts at one for an empty list and adds nested levels according to the current cell representation.

`SymbolAnalyzer` emits:

- `unique-symbols`;
- `symbols`, a space-separated sorted set.

It also exposes `has_symbol(const List&, string_view)` for recursive membership testing. Symbols are identified by `NodeKind::Symbol`; strings containing the same bytes are excluded.

Both analyzers recurse through nested list heads and tails. Neither interprets binding, lexical scope, quote semantics, or runtime conventions. A symbol in a quoted form is counted exactly like any other symbol.

Fact values are strings even when numerically meaningful. Consumers should use `get_count` only for facts known to be decimal counts.
