# Chapter 9 — Concrete Syntax

Supported lexical forms:

- lists delimited by `(` and `)`;
- semicolon comments;
- quoted strings;
- symbols;
- integers;
- floating-point values containing `.`, `e`, or `E`;
- `nan` and `inf`;
- `nil`, `#t`, `true`, `#f`, and `false`;
- quote prefixes `'` and `` ` ``.

Strings use backslash escapes for `n`, `r`, `t`, backslash, and double quote. Unknown escapes discard the backslash and retain the escaped character. An unterminated string consumes to end-of-input and returns the accumulated text; no explicit unterminated-string diagnostic is generated.

Tokens stop at whitespace, parentheses, or semicolon. A leading colon does not create a separate keyword kind; it creates a `Symbol` whose text includes the colon.

Comma and comma-at are not handled as quote syntax despite being listed in the XAS protocol vocabulary. They are ordinary token text.
