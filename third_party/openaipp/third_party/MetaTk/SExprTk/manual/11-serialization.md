# Chapter 11 — S-Expression Serialization

`Serializer::to_string` maps the typed tree back to canonical s-expression text:

- nil → `nil`;
- booleans → `#t` or `#f`;
- integers and floats → stream-formatted numerals;
- strings → escaped double-quoted text;
- symbols → raw symbol text;
- lists → parenthesized forms.

For a `Cell` with a list head, only that nested list is rendered. For a cell with a non-list head and nonempty tail, the serializer synthesizes `(head tail...)`.

`Serializer::escape` handles backslash, quote, newline, carriage return, and tab. `unescape` handles the same escapes and removes the backslash from unknown escapes.

Serialization is structural, not source-preserving. Comments, original whitespace, quote spelling, and token lexemes such as `true` versus `#t` are normalized or discarded. Use `Cartable::events` when source-level event fidelity is required.
