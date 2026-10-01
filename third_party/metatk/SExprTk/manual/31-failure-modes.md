# Chapter 31 — Failure Modes and Contracts

Primary hazards:

- dereferencing `Atom::as_*` with the wrong kind throws;
- `Cell::car()` on an empty nested list is invalid;
- iterator dereference after `done()` is invalid;
- decoded XAS payload becomes dangling when its frame storage dies;
- nonzero payload length with null payload is producer misuse;
- parser recovery can return a partial tree with `errors`;
- `SExprTk::run(source)` does not evaluate;
- disabled runtime backends report errors only through `Semantics`;
- `Kernel::run` hides those errors by returning rendered text;
- transformer-retained events may no longer match transformed roots;
- `PackageManifest::from_toml` is not a full TOML parser;
- signed integer folding can overflow.

Validation policy should be explicit at application boundaries. Check `Cartable::ok()` before evaluation, XAS status codes before consuming decoded fields, and `Semantics::ok()` before reading `value`.

For hostile network input, validate frame length and payload bounds before copying or interpreting payload bytes.
