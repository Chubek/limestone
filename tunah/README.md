# Tunah

Tunah compiles caller-supplied equivalences and runs bounded equality saturation
through the repository's Equinox-NG engine. EkippX preprocesses rule specifications;
SExprTk events build source-located DSLtk ASTs before compilation. Vendor types stay
private to the library.

## Rule files

```scheme
(operator iadd 2)
(operator imul 2)
(operator ishl 2)
(rule add-zero (iadd ?x 0) ?x)
(rule multiply-four (imul ?x 4) (ishl ?x 2))
```

`Session::load_rules(text, source_name)` and `load_rules_file(path)` return the
number of rules added. Declarations apply throughout one load, including before
their textual position. Identical operator declarations are idempotent; conflicting
arities and duplicate rule names are errors. A failed load commits neither rules
nor declarations. Successful loads prepare immutable native patterns once, while
each saturation run owns a fresh e-graph.

Variables start with `?`. Replacement variables must be bound in the pattern.
Applications, including nullary applications such as `(nop)`, need operator
signatures. Bare symbols can represent opaque values and labels. The generic term
adapter supports signed 64-bit decimal literals and symbols, rejects overflowing
literals, and bounds source input to 4 MiB and S-expression nesting to 256 lists.

EkippX's native `@define(NAME, body)` and `@deflit(NAME, body)` directives define
per-load macros, invoked with `&NAME()`. For example:

```text
@define(ZERO, 0)
(operator iadd 2)
(rule add-zero (iadd ?x &ZERO()) ?x)
```

Comments cannot invoke macros. Definitions do not survive into another load, and
unknown directives/functions/expanders fail. Other EkippX facilities require a
future preprocessing adapter. Diagnostics retain the source file, line, column,
and byte offset. When preprocessing changes the stream, locations are explicitly
marked `expanded`, and diagnostic filenames have an `[expanded]` annotation;
these are expanded-stream coordinates rather than an invented original-source map.

## Analysis guards

```scheme
(rule widen-add
  (sext32to64 (iadd ?x ?y))
  (iadd (sext32to64 ?x) (sext32to64 ?y))
  :where (signed-add-fits32 ?x ?y))
```

Register each predicate before loading its rules:

```cpp
session.define_predicate("signed-add-fits32", 2,
  [](std::span<const limestone::tunah::Term> arguments)
    -> limestone::Result<bool> {
    // Consult the host's verified range/type analysis. Unknown is false.
    return limestone::Result<bool>::ok(prove_signed_sum_fits32(arguments));
  });
```

Predicates receive equivalent representatives of matched e-classes, extracted
using the default AST cost independently of target weights. Arguments may be bound
variables or integer literals. `(and (predicate ...) (predicate ...))` is supported;
conditions do not bind new variables. A predicate returning false skips that match.
A predicate returning an error aborts the run with its code and rule location.
An unregistered predicate fails rule compilation with `Unsupported`.

Host predicates must prove their claims under the IL's semantics, including width
and signedness. A selected symbolic representative alone is not an overflow proof.
The instruction-climbing and vectorization files require explicit scalar/vector
overflow analyses. They cannot be loaded into the standalone CLI, which supplies
no such host analyses.

## Extraction and structured terms

`saturate(expression, limits, costs)` and `saturate(term, limits, costs)` return
both a structured `Term` and its S-expression spelling, cost, e-node/e-class
counts, rewrite count, iteration count, saturation status, and limit status.
`parse_term` and `format_term` provide the generic ingress/egress boundary.

Default local costs are 1 for a literal and `1 + arity` for an operator, plus all
child costs. `CostModel::operators` overrides individual local operator costs;
`CostModel::literal` overrides the literal cost. Costs are non-negative, may be
zero, and exclude child costs. The maximum `size_t` value is reserved for infinity.
Additive overflow saturates to infinity; it cannot make an expensive alternative
appear cheap. Extraction fails with `ResourceLimit` only if no finite-cost
representative remains. Equal-cost alternatives follow Equinox-NG's stable node
order, and rules are applied in name order.

```cpp
limestone::tunah::CostModel costs;
costs.operators = {{"imul", 20}, {"ishl", 1}};
auto result = session.saturate("(imul x 4)", {}, costs);
```

Actual target weights belong to a host cost adapter consuming authoritative target
metadata. The numbers above illustrate explicit configuration.

Iteration, node, class, cancellation, and time bounds support extraction of an
equivalent result when saturation halts early. Node/class reservations conservatively
cover instantiation of both sides of a rewrite and may stop before a budget is
completely filled. Time/cancellation checks are cooperative between iterations and
matches; Equinox-NG matching, rebuilding, extraction, and host callbacks are not
preemptible. Host analyses should therefore be bounded.

`Limits::trace` controls trace collection. Entries identify admitted matches with
named rule-source locations and bindings sorted by variable name. Anonymous inline
rule locations remain available through `rules()`. Redundant matches may
appear in the trace; `rewrites` counts actual e-class unions, not trace entries.

## Instruction tuner corpus

Load `tuners/instr-level-tune/vocabulary.tuner` before selected rule files. It
declares the shared IL arities without introducing target latency, encoding, or
register facts. The 13 tuner files contain 239 rules.

The vocabulary is a syntax contract. The caller's IL adapter still establishes
types, purity, memory ordering, control flow, lane shapes, and reconstruction
legality. Generic term saturation does not itself implement those IL adapters.
Arithmetic corpus checks cover 8-, 16-, 32-, and 64-bit wrapping integers with
modulo-width shift counts and the explicitly wrapped signed-division overflow
model. These semantics must be supplied by the host, not inferred from an ISA.

Regression coverage includes:

- all 13 files loading through the shared vocabulary and registered analyses;
- 156 pure integer rules checked with boundary/random valuations at four widths,
  plus real Equinox-NG saturation/extraction;
- typed conversion compositions and rejection of overflowing arithmetic widening;
- byte/word store-load forwarding preserving both truncated values and memory;
- scalar broadcast/reduction shapes and vector overflow guards;
- malformed specifications, transactional failure, macros, reproducibility,
  cost overflow, resource fallback, and CLI integration.

## CLI

```sh
printf '(iadd (iadd 12 30) (imul x 0))\n' | build/limestone-cli \
  --optimize-term \
  --rules tunah/tuners/instr-level-tune/vocabulary.tuner \
  --rules tunah/tuners/instr-level-tune/alg-sim.tuner \
  --rules tunah/tuners/instr-level-tune/const-folding.tuner
# 42
```

Use repeatable `--op-cost OP COST`, `--literal-cost COST`, `--iterations N`,
`--node-limit N`, `--class-limit N`, and `--time-limit MS` for explicit configuration.
`--trace` sends matches/statistics to stderr; the extracted expression is written
to stdout or the file named by `-o`.

Before/after workload measurements are recorded in `BENCHMARKS.md`.
