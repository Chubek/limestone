# Chapter 4: CLI Usage (`eqvsg`)

## Purpose

This chapter documents the exact behavior of the `eqvsg` executable from `EQVSG.cpp`.

## Command synopsis

```text
eqvsg <expr-definition.eq> <rewrite-rules.eq> <input.sexpr>
```

`eqvsg` expects exactly **3 arguments** after the program name.

If argument count is wrong, it prints usage text and exits with status `1`.

## What `eqvsg` does internally

1. Reads `<input.sexpr>` as raw text.
2. Creates `qamrpp::Context`.
3. Loads:
   - `libqamrpp-sexpr.so`
   - `libqamrpp-leqvsg.so`
4. Calls native functions through a short QaMRpp script:
   - `eqvsg_load_expr_definition(path)`
   - `eqvsg_load_rewrite_rules(path)`
   - `eqvsg_rewrite(input)`
5. Prints rewrite result to stdout.

## Library loading behavior

For each library, load order is:

1. `$HOME/.qamrpp/eqvsglib/<libname>`
2. fallback `bin/<libname>`

If neither path works, `eqvsg` prints a load error and exits nonzero.

## Inputs

### Expression definition file (`*.eqdef`)

Example: `examples/expr-definitions/01-arith-core.eqdef`

```text
name:arith-core
sort:Expr
ops:add,mul,sub,div,neg,zero,one,x,y
```

### Rewrite rule file (`*.rwr`)

Example: `examples/rewrite-rules/01-add-zero.rwr`

```text
(add x zero) => x
(add zero x) => x
```

### Input S-expression file (`*.sexpr`)

Example: `examples/tests/input-01-add-zero.sexpr`

```text
(add x zero)
```

## Output

A single rewritten expression is printed to stdout followed by newline.

Example run:

```bash
eqvsg examples/expr-definitions/01-arith-core.eqdef \
      examples/rewrite-rules/01-add-zero.rwr \
      examples/tests/input-01-add-zero.sexpr
```

Expected output:

```text
x
```

## More practical runs

### Boolean simplification

```bash
eqvsg examples/expr-definitions/02-bool-core.eqdef \
      examples/rewrite-rules/11-not-not.rwr \
      examples/tests/input-11-not-not.sexpr
```

### Control simplification

```bash
eqvsg examples/expr-definitions/03-control.eqdef \
      examples/rewrite-rules/12-if-true.rwr \
      examples/tests/input-12-if-true.sexpr
```

### Logical expansion

```bash
eqvsg examples/expr-definitions/12-logic.eqdef \
      examples/rewrite-rules/20-iff-expand.rwr \
      examples/tests/input-20-iff-expand.sexpr
```

## Error handling and exit behavior

From `EQVSG.cpp` behavior:

- Missing/unreadable input files: throws and prints `EQVSG error: ...`
- Failed native library load: explicit message with attempted paths
- Null script result: `EQVSG error: QaMRpp script returned null result`
- Any runtime exception: printed as `EQVSG error: <message>`

All error paths return exit code `1`.

## Batch/matrix usage hint

Use `examples/tests/run_matrix.sh` to run multiple predefined cases. This script is useful for quick regression checks of the CLI data path.
