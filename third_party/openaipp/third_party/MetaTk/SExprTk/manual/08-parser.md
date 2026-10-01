# Chapter 8 — Parser State Machine

`SExprTk::parse` creates a private parser with:

- byte position;
- one-based line and column;
- monotonically increasing event sequence;
- dispatcher pointer;
- event and error vectors.

Parsing begins with `DOCUMENT_BEGIN`, repeatedly skips whitespace/comments, parses cells until end-of-input, then emits `DOCUMENT_END`.

`parse_cell` dispatches on the next byte:

- `(`: recursively parse a nested list;
- `)`: record an unexpected-close error and advance;
- `'` or `` ` ``: emit `QUOTE`, wrap the following cell as `quote` or `quasiquote`;
- otherwise: parse a token and convert it to an atom.

Whitespace includes all characters recognized by the current C locale through `std::isspace`. Semicolon comments terminate at newline or end-of-input. The parser is permissive after errors and can return partial structures; it is not a validating reader with transactional rollback.
