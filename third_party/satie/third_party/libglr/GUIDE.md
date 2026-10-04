# LibGLR Guide

## Overview

LibGLR is layered, and each layer is usable on its own:

1. grammar data structures in `include/glr/grammar.h`
2. parser and ambiguity infrastructure in `include/glr/parser.h` and
   `include/glr/disambiguate.h`
3. grammar rewriting in `include/glr/rewrite.h`, with direct C calls or
   declarative S-expression programs
4. incremental editing in `include/glr/live-parsing.h`: report an edit, see
   what it invalidates, update the tree
5. queries over a parse forest in `include/glr/query.h`: patterns paired with
   actions, including AST building

Layers 4 and 5 build on 1 and 2 and are described below.

## Quick start

The grammar's terminal names *are* the token text: the tokenizer matches them
against the input, longest match first. A grammar therefore has to cover every
character it accepts, and anything optional (whitespace, comments) is better
handled as trivia than as a grammar rule.

```c
#include <glr/glr.h>

glr_grammar_t *grammar = glr_grammar_create ();
int expr = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Expr");
int num  = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "n");
int plus = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
glr_symbol_t *body[3];

body[0] = glr_grammar_get_symbol (grammar, num);
glr_grammar_add_production (grammar, expr, body, 1);
body[0] = glr_grammar_get_symbol (grammar, expr);
body[1] = glr_grammar_get_symbol (grammar, plus);
body[2] = glr_grammar_get_symbol (grammar, num);
glr_grammar_add_production (grammar, expr, body, 3);
glr_grammar_set_start_symbol (grammar, expr);

glr_parser_t *parser = glr_parser_create (grammar);
glr_parser_set_trivia (parser, " ");

glr_parse_result_t result = glr_parse (parser, "n + n", 5);
```

`glr_parse()` reports `GLR_PARSE_SUCCESS` only when the parse actually reached
an accepting state. Input outside the grammar's language fails with
`GLR_PARSE_ERROR_SYNTAX` and `result.position` pointing at the offending
byte, so there is no way to mistake a no-op parse for a successful one.

## Parse tables

`glr_parse()` builds an LR(1) table on first use and caches it on the parser.
Call `glr_grammar_build_parse_table()` to build one yourself — to check whether
a grammar is conflict free, to report conflicts, or to reuse a table across
parsers:

```c
char error[128];
glr_parse_table_t *table = glr_grammar_build_parse_table (grammar, error,
                                                           sizeof (error));
if (table == NULL)
  {
    fprintf (stderr, "no table: %s\n", error);
  }
else if (glr_parse_table_conflict_count (table) != 0)
  {
    printf ("%zu conflicting cells\n",
            glr_parse_table_conflict_count (table));
  }
glr_parser_set_parse_table (parser, table, false);   /* caller keeps ownership */
```

Generated tables carry one extra terminal column past the grammar's symbol
ids. Reductions justified by FOLLOW and the single accept action live there,
so end-of-input never collides with a real terminal; reach it with
`glr_parse_table_eof_column()`.

Operator precedence is encoded in the grammar shape, not in a separate table:
`Expr -> Expr '+' Term` with `Term -> Term '*' Factor` parses `n+n*n` the way
you expect and reports zero conflicts. Left recursion is not a conflict
either, and `n-n-n` reduces left-associatively.

## Ambiguity

When a grammar genuinely has no conflict-free LR(1) table, the parser keeps a
separate stack per reading instead of committing to one. After the parse,
`glr_parser_stack_count()` reports how many readings are still alive, and
`glr_forest_is_ambiguous()` reports whether the forest packed more than one
derivation. Register a disambiguator (see below) to prune them.

## The parse forest

The forest is a Shared Packed Parse Forest, a directed acyclic graph rather
than a tree, because sub-parses are shared. Node types:

- `GLR_NODE_TERMINAL`: a matched token, with `position`/`end_position`
  giving the bytes it covers.
- `GLR_NODE_CONSTRUCTOR`: one reduction, keyed by production. `symbol_id` is
  the production id; its children are the symbols the production matched.
- `GLR_NODE_NONTERMINAL`: one non-terminal occurrence over one span, with the
  constructor nodes that derive it as children.

Walk it with `glr_forest_visit()`, which visits each node once even though
several paths reach it. `forest->root` is the start-symbol node of the
accepted derivation, so starting the walk there skips the intermediate nodes
that were not part of the accepted parse.

## Editing text: live parsing

An editor knows two things about a change: where it starts and where it ends.
`live-parsing.h` takes exactly that and keeps a session that owns the text, the
forest, and the metadata tying them together.

```c
glr_live_parser_t *live
    = glr_live_parser_create (grammar, text, length, error, sizeof (error));

glr_live_edit_t edit = { 1, 2, 1, 3, "-", 1 };  /* line/col, line/col, text */
glr_live_parser_edit (live, &edit, error, sizeof (error));
glr_live_parser_update (live, error, sizeof (error));
```

The four calls map onto the four stages:

1. **Create** parses the initial text and keeps the tree.
2. **Edit** splices the text and marks the branches the edit can reach. The tree
   still describes the old text at this point, which is the point: an
   application can report several keystrokes and then update once.
3. **Update** re-parses and hands back a new tree.
4. **Forest** is what the rest of the program reads.

Positions are 1-based, and columns count bytes from the start of the line. The
end position is **exclusive**, which is what distinguishes the three cases: an
insertion is two equal positions with something to insert, a deletion is a
non-empty span with no replacement, and a replacement has both. A position
outside the text is rejected rather than clamped, because a clamped edit would
produce a tree that does not match the text.

### Marking

Marking walks the cached tree down to the nodes whose span the edited bytes fall
inside, and then up from each of those to their ancestors. Both halves matter:
the first says what has to be re-derived, the second says what has to be
re-derived *because of* it. `glr_live_parser_dirty_count()` and
`glr_live_parser_dirty_node()` report the result, with the overlapping nodes
first so the list also explains what the update is about to do.

The upwards half needs parent pointers, and the packed forest has none. The
session builds a parent index per edit instead of teaching the forest about
editing, which keeps the parser's data structure free of editor bookkeeping at
the cost of one walk per edit.

### What an update actually re-parses

An LR parse of a prefix does not depend on what follows it, so a parse can be
stopped at any point and resumed. The parser exposes that directly:

```c
glr_parser_set_snapshot_hook (parser, my_snapshot_sink, state);
/* ... after a parse, `state` holds one stack per position reached ... */
glr_parser_parse_from (parser, snapshot, forest, position, input, length,
                       &result);
```

A snapshot is the whole parse stack, not its top entry: a reduction pops as many
entries as its production is long and reads the state below them, so resuming
from one entry would silently lose the rest of the stack.

The session installs the hook, keeps the newest snapshot at or before each input
position, and on update resumes from the last snapshot the edit left valid.
Since a splice at offset *k* leaves every byte before *k* untouched, any
snapshot at or before *k* still describes the text it was taken from. The nodes
the edit invalidated are pruned from the forest first, and the parse continues
into the same forest, so the reused prefix is not rebuilt.

`glr_live_parser_stats_t` reports the split:

```c
stats.bytes_reused + stats.bytes_reparsed == stats.input_length;
```

Editing near the end of a document therefore costs a couple of bytes rather than
the whole file. `examples/live.c` prints that table for three edits.

When an edit makes the text unparseable the update reports it and drops the tree
rather than leaving one that describes text the session no longer holds;
`glr_live_parser_error()` and `stats.last_parse_succeeded` say so.

## Queries

A query walks a parse forest and offers every node it reaches to every rule, so
one traversal serves all of them. A rule pairs a pattern with one or more
actions:

```scheme
(query
  (name "operands")
  (rules
    (match (constructor _ (terminal @op) (nonterminal _))
           (extract "operator " "op"))
    (match (nonterminal Expr) (@node "Expr"))))
```

The `(query ...)` wrapper is optional, as is `(rules ...)`; a bare list of rules
compiles too.

### Patterns

| pattern | matches |
| --- | --- |
| `(terminal SYM)` | a terminal with that symbol |
| `(nonterminal SYM)` | a non-terminal node |
| `(constructor ...)` | a constructor node, keyed by production |
| `_` | any node of the expected kind |
| `(any SYM)` | a node of any kind with that symbol |
| `$NAME`, `@NAME` | binds the matched node to `NAME` |

Nested lists are patterns for the children and they match **in order**: each
nested pattern takes the next child that fits, and all of them have to be taken.
So `(constructor _ (terminal @op) (nonterminal _))` means "an operator with a
non-terminal after it", not "a node that happens to have both". Matching
children in order is what makes a pattern say something about structure.

### Actions

Actions resolve by name against a table, so an application can add its own
without changing the library. `glr_global_query_actions` holds the builtins; a
local table passed to `glr_query_compile()` is consulted first and shadows a
builtin of the same name.

| action | effect |
| --- | --- |
| `extract` | report the text of a bound node, optionally with a prefix |
| `print`, `print-symbol` | report the matched text or its symbol name |
| `count` | count matches as they fire |
| `@node` | build an AST node mirroring the matched subtree |
| `@leaf` | build an AST leaf, carrying matched text or a fixed value |
| `@xml` | stream the matched subtree as an XML event stream |
| `@rename` | rename the matched symbol: rewriting (needs `options.mutable_grammar`) |
| `@reject` | record the matched reading as rejected |
| `@signal` | deliver the matched text as a signal |
| `@ipc` | pass the matched text to another process |
| `@halt` | stop walking |
| `@exit` | ask the program to exit, with a code |

Everything an action wants to say goes through one reporting sink, so the
application decides what "report" means: print, log, or push onto a pipe.

```c
glr_query_options_t options = { 0 };
options.report = my_sink;      /* where extraction, printing and IPC land */
options.user_data = my_state;  /* handed back to the sink */
options.ast = ast;            /* what @node() and @leaf() fill */
glr_query_run_ex (query, grammar, forest, forest->root, input, length,
                  &options, &stats);
```

`glr_query_run_ex()` takes the grammar as `const`, so rewriting actions only
run when the caller offers `options.mutable_grammar`. A query that silently cast
the const away would be changing the caller's grammar behind its back.

An application action receives the same context plus the arguments written in the
query, which is what lets one registered name do different things in different
rules:

```c
static glr_query_action_result_t
count_action (glr_query_context_t *context, const glr_query_match_t *match,
              const char *const *args, size_t arg_count, void *user_data);
```

`@reject` records the rejected node in the query context rather than reaching
into the parser, so `glr_query_was_rejected()` is available afterwards and the
application decides what to do with the list.

### ASTs

An AST is a plain tree rather than a DAG: a node either has children or carries
a value, never both. It is described in S-expression form, inline in a query with
`(ast (node KIND PATTERN) ...)` or loaded from a file with
`glr_query_ast_load_file()`. `@node()` mirrors the matched subtree into it, and
because several matches can fire, sibling matches attach under a synthetic
`list` node so a flat result stays flat.

ASTs serialize both ways: `glr_ast_to_sexp()` renders one and
`glr_ast_from_sexp()` reads it back, and `glr_ast_write()` streams one through a
callback so the renderer does not need stdio.

## The parse forest as XML

`glr_forest_write_xml_events()` walks the forest and reports one event per node:

```
<?xml version="1.0" encoding="UTF-8"?>
<parse-forest nodes="7">
  <node type="nonterminal" symbol="Expr" start="0" end="3">
    <node type="constructor" production="1" start="0" end="3">
      <node type="nonterminal" symbol="Expr" start="0" end="1">
        <node type="constructor" production="0" start="0" end="1">
          <leaf type="terminal" symbol="n" start="0" end="1">n</leaf>
```

`glr_forest_node_write_xml_events()` does the same for one subtree, which is
what the `@xml` action uses. `glr_forest_to_xml()` renders the whole stream into
one string. Shared nodes are emitted once, so the document mirrors the packed
structure rather than expanding the ambiguity into repeated subtrees.

## The GLR Rewrite Language

GRL is a small S-expression language. A program is a `rewrite` form that
contains a name and either inline rules or a `(rules ...)` block.

### Program shape

```lisp
(rewrite
  (name "pipeline-name")
  (rules
    (rule-one ...)
    (rule-two ...)))
```

The parser also accepts a shorter form where rules are listed directly under
`rewrite`.

### Built-in rules

GRL currently supports these built-ins:

- `(add-symbol terminal NAME)`
- `(add-symbol nonterminal NAME)`
- `(drop-symbol NAME)`
- `(rename-symbol OLD NEW)`
- `(set-start NAME)`
- `(add-production HEAD (SYM1 SYM2 ...))`
- `(drop-production HEAD (SYM1 SYM2 ...))`
- `(remove-epsilon-productions)`
- `(remove-unit-productions)`
- `(eliminate-useless-symbols)`
- `(remove-left-recursion)`
- `(left-factor)`
- `(make-lr-compatible)`
- `(eliminate-ambiguity)`

### Semantics

- `add-symbol` is idempotent when a symbol with the same name and kind already exists.
- `drop-symbol` also removes productions that reference the symbol.
- `add-production` resolves body symbols by name against the current grammar.
- `remove-epsilon-productions` preserves nullable derivations and keeps only start-symbol epsilon productions.
- `remove-unit-productions` computes the nonterminal unit-closure and materializes the non-unit productions.
- `remove-left-recursion` uses ordered substitution followed by direct left-recursion elimination.
- `left-factor` performs repeated one-symbol prefix factoring and introduces helper nonterminals.
- `make-lr-compatible` runs epsilon removal, unit removal, left-recursion elimination, left factoring, and useless-symbol cleanup.
- `eliminate-ambiguity` runs the LR-compatibility pipeline, removes useless symbols, and then **verifies** the result by building a parse table. It returns `GLR_REWRITE_STATUS_OK` only when the rewritten grammar really is conflict free, and `GLR_REWRITE_STATUS_CONFLICT` when ambiguity survives — which is the case for a grammar that is ambiguous by construction, such as `expr -> expr '+' expr | expr '*' expr | expr -> n`.

### Example programs

Remove nullable productions and cleanup:

```lisp
(rewrite
  (name "nullable-cleanup")
  (rules
    (remove-epsilon-productions)
    (eliminate-useless-symbols)))
```

Perform a full normalization pipeline:

```lisp
(rewrite
  (name "normalize")
  (rules
    (make-lr-compatible)))
```

Edit a grammar structurally before normalization:

```lisp
(rewrite
  (name "rename-and-normalize")
  (rules
    (rename-symbol Expr Expression)
    (set-start Expression)
    (make-lr-compatible)))
```

## Procedural rewrite API

The procedural API mirrors the GRL runtime and is useful when rewrites depend
on application logic.

### Core types

- `glr_rewrite_rule_kind_t` enumerates each supported opcode.
- `glr_rewrite_rule_t` stores one compiled rule.
- `glr_rewrite_program_t` stores an ordered list of rules.
- `glr_rewrite_report_t` reports how many rules ran and where execution failed.
- `glr_rewrite_status_t` distinguishes parse errors, missing symbols, conflicts, and memory failures.

### Loading and compiling GRL

```c
char error[256];
glr_rewrite_program_t *program =
    glr_rewrite_program_load_file ("rewritelib/remove-left-recursion.grl",
                                   error, sizeof (error));
if (program == NULL)
  {
    fprintf (stderr, "rewrite load failed: %s\n", error);
  }
```

### Applying a compiled program

```c
glr_rewrite_report_t report;
glr_rewrite_status_t status =
    glr_rewrite_program_apply (grammar, program, &report);
if (status != GLR_REWRITE_STATUS_OK)
  {
    fprintf (stderr, "rewrite failed after %zu rules: %s\n",
             report.rules_attempted, report.message);
  }
```

### Building a program procedurally

```c
glr_rewrite_program_t *program = glr_rewrite_program_create ("custom");
glr_rewrite_rule_t rule = {0};

rule.kind = GLR_REWRITE_RULE_RENAME_SYMBOL;
rule.data.rename_symbol.old_name = "Expr";
rule.data.rename_symbol.new_name = "Expression";
glr_rewrite_program_add_rule (program, &rule);

rule.kind = GLR_REWRITE_RULE_MAKE_LR_COMPATIBLE;
glr_rewrite_program_add_rule (program, &rule);
```

`glr_rewrite_program_add_rule` copies owned strings and production templates,
so stack-allocated rule values are fine.

### One-shot helpers

Each major transform is also exposed directly:

```c
glr_rewrite_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "ExprTail");
glr_rewrite_add_production (grammar, "Expr", body_names, body_length);
glr_rewrite_remove_epsilon_productions (grammar);
glr_rewrite_remove_unit_productions (grammar);
glr_rewrite_remove_left_recursion (grammar);
glr_rewrite_left_factor (grammar);
glr_rewrite_remove_useless_symbols (grammar);
```

### Error handling guidance

- use `GLR_REWRITE_STATUS_NOT_FOUND` to detect misspelled symbol names in GRL or procedural edits
- use `GLR_REWRITE_STATUS_CONFLICT` for name collisions and invalid start-symbol updates
- treat `GLR_REWRITE_STATUS_PARSE_ERROR` as an invalid GRL source file
- treat `GLR_REWRITE_STATUS_MEMORY_ERROR` as a hard failure and keep the current grammar snapshot only if your application copied it first

## Standard rewrite library

`rewritelib/` installs reusable GRL files:

- `eliminate-epsilon-production.grl`
- `remove-unit-productions.grl`
- `remove-left-recursion.grl`
- `left-factor.grl`
- `eliminate-useless-symbols.grl`
- `make-lr-compat.grl`
- `eiminate-ambguity.grl`

The last filename keeps the historic typo already present in the tree so the
repository layout stays stable.

## Disambiguation hooks

Disambiguation remains procedural. Register hooks on `glr_parser_t` with:

```c
glr_parser_add_disambiguator (
    parser,
    glr_disambig_precedence_hook_create ("precedence", 100,
                                         resolve_precedence,
                                         my_state, destroy_state));
```

The standard helpers in `disambstd/` cover precedence, associativity,
predicates, semantic filtering, dynamic programming, and probabilistic
selection.

## Testing

The C test suites are registered with CTest and carry labels, so a subset can be
run on its own:

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -L stratified          # data structures and parsing
ctest --test-dir build -L forest              # parse forest and live parsing
ctest --test-dir build -L query               # queries and AST building
```

The suites worth knowing about:

| suite | what it covers |
| --- | --- |
| `test_stratified` | grammar, stack, forest, table, disambiguation, rewrite |
| `test_stratified_sppf` | LR(1) tables, the driver, the forest, trivia, left recursion |
| `test_live_parsing` | editing, marking, incremental updates, XML events |
| `test_query` | patterns, the action table, builtin actions, AST round trips |

`test_live_parsing` and `test_query` cover the two newest layers end to end:
the live suite drives real edits through a session and checks that an update
reuses the prefix it is entitled to, and the query suite compiles patterns,
registers an application action, and round-trips an AST through S-expressions.

The C++ bindings in `GLRpp/` have a Catch2 suite as well. It is not wired into
CMake, so build it by hand:

```bash
g++ -std=c++20 -I. -Iinclude -c GLRpp/tests/unittest/test_glrpp.cpp \
    -o /tmp/test_glrpp.o
g++ /tmp/test_glrpp.o build/liblibglr.a build/libsfsexp.a \
    -lCatch2Main -lCatch2 -o /tmp/test_glrpp
/tmp/test_glrpp
```

`tests/fuzz/` holds AFL harnesses for the core data structures and, with
`tests/fuzz/run_fuzzing.sh`, for the cache-backed incremental paths. Each
harness also runs standalone, reading one case from standard input, which is how
a finding gets reproduced.

## Generated documentation

When Doxygen is enabled, the generated HTML and manpages cover both the parser
API and the rewrite API. The removed `man/` directory is not required anymore.
