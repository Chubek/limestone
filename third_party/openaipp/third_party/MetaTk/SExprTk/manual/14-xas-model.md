# Chapter 14 — XAS Event Model

XAS—eXchangeable Abstract Syntax—is a flat event stream for an s-expression document. Event order preserves nesting:

```text
DOCUMENT_BEGIN
LIST_BEGIN
ATOM ...
LIST_END
DOCUMENT_END
```

Comments and errors are interleaved. Quote prefixes receive explicit `QUOTE` events, while the tree stores quote wrappers.

Each event carries sequence, kind, source position, and optional payload. Parser sequences start at one and increase for every event, including comments and errors. Payload conventions:

- `ATOM`: source token text;
- `COMMENT`: comment text without `;`;
- `ERROR`: human-readable local diagnostic;
- structural events: normally empty, except list delimiters and quote spelling from the parser.

XAS decouples event consumers from tree construction. A dispatcher can observe the stream, a C source can generate events, and a network transport can frame them without exposing C++ tree internals.
