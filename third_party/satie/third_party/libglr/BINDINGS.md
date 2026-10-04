# Bindings Guide

LibGLR ships a SWIG interface in `bindings/libglr.i` for generating low-level,
mostly one-to-one foreign-function bindings across many SWIG targets. These
bindings are intentionally thinner than the higher-level language wrappers that
will live under `bindings/wrappers/`.

## Binding Philosophy

- The SWIG bindings expose the C API with minimal policy.
- Helper functions prefixed with `glr_binding_` smooth over raw C arrays,
  union-backed rewrite rules, and enum/string conversions.
- Higher-level wrappers should build on top of the generated artifacts instead
  of reaching directly into internal struct arrays.
- The generated bindings are meant to be portable across SWIG targets, so the
  interface prefers explicit helper accessors over target-specific magic.

## Covered Modules

The interface exports the public headers under `include/glr/`:

- grammar construction and inspection
- parse forests, graphs, stacks, forks, and reductions
- parser creation, parsing, and reader/token support
- lexer hooks and UTF-16 reader support
- disambiguation hooks
- rewrite programs and procedural rewrites

## SWIG-Friendly Helpers

`bindings/libglr.i` adds helpers that wrappers should prefer when possible:

- `glr_binding_grammar_symbol_count()` and `glr_binding_grammar_symbol_at()`
- `glr_binding_grammar_production_count()` and `glr_binding_grammar_production_at()`
- `glr_binding_production_body_length()` and `glr_binding_production_body_symbol_at()`
- `glr_binding_forest_child_count()` and `glr_binding_forest_child_at()`
- `glr_binding_graph_*()` helpers for nodes, edges, and degrees
- `glr_binding_item_set_count()` and `glr_binding_item_set_item_at()`
- `glr_binding_reader_token_name()`
- `glr_binding_parse_error_string()`, `glr_binding_rewrite_status_string()`,
  `glr_binding_disambig_result_string()`, and `glr_binding_reader_status_name()`
- `glr_binding_rewrite_program_add_*()` helpers for building rewrite programs
  without manually filling the `glr_rewrite_rule_t` union

These helpers are especially useful in languages where direct struct-array
field access is awkward or where unions are poorly represented.

## Generating Bindings

Use `bindings/generate-bindings.sh`.

Generate Python bindings in the repository root:

```bash
./bindings/generate-bindings.sh --language python
```

Generate Java and C# into another directory:

```bash
./bindings/generate-bindings.sh \
  --language java \
  --language csharp \
  --output-root /tmp/libglr-bindings
```

Generate every SWIG target reported by the local `swig` binary:

```bash
./bindings/generate-bindings.sh --all
```

Create archives as well, while keeping the generated directories:

```bash
./bindings/generate-bindings.sh \
  --language python \
  --archive \
  --directory \
  --package \
  --doc \
  --examples
```

## Output Layout

Each generated binding goes into a directory named like:

- `_bindings_libglr_python`
- `_bindings_libglr_ruby`
- `_bindings_libglr_java`

Inside each directory you will typically find:

- SWIG-generated source files
- language-specific proxy/module files
- copied runtime assets such as headers, documentation, `rewritelib/`, and `disambstd/`
- optional package metadata when `--package` is used
- optional docs/examples when `--doc` or `--examples` is used

## Packaging and Archiving

When `--archive` is used, the script delegates archive creation to
`scripts/archive-services.py`.

Supported archive options:

- `--gzip` (default)
- `--deb`
- `--rpm`
- `--zlib`
- `--bzip2`
- `--pacman`

When `--package` is used, the script prepares a language-oriented package
layout and, for non-DEB/RPM archive types, delegates extra package work to
`scripts/packaging-service.py`.

At packaging time the script also attempts to include:

- public headers from `include/glr/`
- the built `libglr` shared or static library when available
- `rewritelib/`
- `disambstd/`
- documentation from `doc/`

## Git Repositories

If `--git` is passed, the generator prepares each output directory as a Git
workspace by creating:

- `.gitignore`
- `.gitmodules`
- a Git repository when `git` is installed

This can be used even when `--archive` is not requested.

## Basic Usage Pattern

The exact import style depends on the target language, but the general workflow
is the same:

1. create core objects like `glr_grammar_t *` using constructors such as
   `glr_grammar_create()`
2. build symbols and productions with the grammar API
3. inspect arrays through `glr_binding_*()` helper functions
4. create `glr_parser_t *`, `glr_reader_t *`, or rewrite programs as needed
5. destroy owned objects explicitly with the matching destroy function

## Language Notes

- `python`: usually the fastest path for experimentation and wrapper prototyping.
- `ruby`: helper accessors avoid wrestling with pointer arrays in the generated layer.
- `java`: the interface injects a small library-load stub so wrappers can choose
  whether to load `libglr` automatically or manually.
- `csharp`: the interface adds standard interop imports to keep generated code cleaner.
- `go`, `lua`, `perl`, `php`, `tcl`, and others: generation depends on what the
  local `swig` binary reports via `swig -help`.

## Wrapper Authors

When you move on to `bindings/wrappers/`, prefer this layering:

- raw generated SWIG layer from `bindings/libglr.i`
- thin safety helpers in the wrapper package
- idiomatic language-facing classes/modules on top

That split keeps the generated surface stable while letting wrappers become
more natural in their host language.
