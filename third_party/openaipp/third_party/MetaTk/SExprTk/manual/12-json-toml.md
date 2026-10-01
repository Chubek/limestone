# Chapter 12 — JSON and TOML Exchange

`Serializer::to_json` emits a structural JSON representation:

- nil → `null`;
- booleans, numbers → JSON primitives;
- strings → escaped JSON strings;
- symbols → escaped JSON strings;
- lists → JSON arrays;
- pair-like cells → `{"head":...,"tail":[...]}`.

This is a representation of the SExprTk tree, not a semantic conversion of symbols into JSON object keys.

`PackageManifest::to_toml` emits three standard fields—`name`, `version`, `entry`—followed by arbitrary string fields. `Package::to_toml` appends package metadata.

`PackageManifest::from_toml` is intentionally minimal: it scans lines for `=`, trims both sides, removes one surrounding pair of double quotes, recognizes the three standard keys, and stores all others in `fields`. It does not implement TOML tables, arrays, comments, escapes, or type checking.

Neither serializer validates output against an external grammar.
