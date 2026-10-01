# Chapter 28 — Packages and Manifests

`PackageManifest` contains:

- `name`, default `SExprTk`;
- `version`, default `0.1.0`;
- `entry`, default `main.sx`;
- arbitrary string `fields`.

`SExprTk::package` parses source and combines it with a manifest into `Package`, which also owns package metadata.

Manifest serialization is deterministic because fields use `std::map`. Package serialization emits manifest fields first, then package metadata. Duplicate keys are possible when metadata overlaps manifest keys; no conflict policy is applied.

The TOML parser is line-oriented and intentionally shallow. It is appropriate for generated metadata and controlled inputs, not general TOML. Values remain strings, and malformed lines without `=` are silently skipped.

Package assembly does not resolve the `entry` path, load dependencies, validate versions, or execute the cartable. Those concerns belong to a higher-level build or deployment system.
