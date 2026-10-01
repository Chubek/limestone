# Chapter 10 — Errors and Source Locations

Parser locations are one-based. `line` starts at 1 and `column` at 1; newline advances to the next line and resets column to 1. Event locations describe the parser position at emission time, generally the next token or delimiter.

Malformed input generates:

1. an `XASEvent` of kind `ERROR`;
2. an error payload containing the local message;
3. a `Cartable::errors` entry with `source:line:column`.

Current diagnostics include `unterminated list` and `unexpected ')'`. The parser emits `DOCUMENT_END` even after recovery.

XAS uses `0` for unknown line/column, while parser-produced events normally carry nonzero values. Protocol fields are 16-bit; very large source positions narrow to `uint16_t`.

Runtime failures are not parser errors. They appear in `Semantics::errors`. File-open failures throw immediately from `Source::from_file`.
