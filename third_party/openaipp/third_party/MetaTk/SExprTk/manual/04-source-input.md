# Chapter 4 — Source Input

`Source` is the parser input unit:

```cpp
auto memory = sexprtk::Source::from_string("(alpha 42)", "unit.sx");
auto file = sexprtk::Source::from_file("program.sx");
```

Fields:

- `name`: diagnostic and metadata identity;
- `text`: complete source bytes.

`from_string` moves both arguments into the result and defaults the name to `<memory>`. `from_file` opens in binary mode, reads the complete stream, and throws `std::runtime_error` on failure. File paths are preserved as supplied.

The parser treats input as a byte string with character classification through `std::isspace`; no encoding validation is performed. XAS payloads are documented as UTF-8, but the parser does not enforce UTF-8.

A `Source` is reusable: `SExprTk::parse` constructs a fresh parser with position, line, column, sequence, event, and error state initialized to zero/one defaults.
