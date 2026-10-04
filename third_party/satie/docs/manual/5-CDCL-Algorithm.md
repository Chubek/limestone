# CDCL Solver

## Core model

`CDCLSolver` extends DPLL with:

- implication trail with decision levels;
- conflict analysis by clause resolution;
- learned clause insertion;
- non-chronological backjumping.

## State representation

Primary internal state:

- clause database (`original + learned`);
- per-variable assignment value;
- per-variable decision level;
- per-variable reason clause index;
- trail entries `(literal, level, antecedent)`;
- variable activity scores.

## Propagation

`propagate()` consumes the assignment trail using two watched literals
per non-unit clause. A watch list is visited when its watched literal
becomes false:

- a true blocking literal allows the clause to be skipped;
- otherwise the solver tries to move the watch to a non-false literal;
- if no replacement exists, the other watch is either an implication
  or an actual conflicting clause.

Implications are appended to the trail with their reason clause and
decision level. Propagation stops when the queue is drained or a conflict
is found; it does not rescan every clause at each step.

## Conflict analysis

On conflict:

1. initialize learned clause from conflicting clause;
2. resolve against antecedent clauses of current-level literals;
3. reduce current-level literal count to the first unique implication point;
4. compute backjump level as max non-current level in learned clause;
5. bump activity of variables in learned clause.

## Learning and backjump

- learned clause appended to clause database;
- backjump unassigns trail entries above target level;
- search resumes from reduced level with stronger clause set.

Effect:

- avoids rediscovering equivalent conflicts;
- compresses search depth via non-chronological rewind.

## Branching

Current branching selects highest activity unassigned variable.

- conflict-driven activity growth biases toward recently conflicting variables.
- tie behavior is deterministic by scan order.
- saved phases select the polarity for later decisions.

## Minimization, restarts, and database reduction

Recursive minimization checks whether a learned literal follows from
reason clauses using literals already present in the analysis or at the
root level. It is enabled by default and can be disabled with
`set_enable_minimization(false)`.

Restarts follow the Luby sequence `1, 1, 2, 1, 1, 2, 4, ...` multiplied
by `set_luby_scale` (default 128 conflicts). The solver cancels decisions,
keeps the current clause database, and reasserts dropped assumptions.
Restarts can be disabled for an individual engine instance.

At restart points, a learned database beyond `set_max_learned_clauses`
(default 4000) is reduced using clause activity and length ordering and
the watch lists are rebuilt. The threshold is a reduction trigger rather
than a hard bound enforced immediately after every learned clause.

## Assumptions and stopping conditions

`solve_under` assigns assumption literals at initial decision levels.
Backtracking or restarting reasserts assumptions that were dropped.
UNSAT under assumptions triggers deletion-based core minimization;
`last_unsat_core` reports a subset of those assumption literals.

`set_conflict_budget` supports an UNKNOWN result on an unfinished search.
Every solve starts a fresh search and resets statistics. See
[Solver Controls and Diagnostics](16-Solver-Controls-and-Diagnostics.md)
for defaults, lifecycle details, and runnable examples.

## Statistics semantics

- `decisions`: decision assignments.
- `propagations`: implied assignments.
- `conflicts`: encountered conflicts.
- `learned_clauses`: total inserted learned clauses.
- `restarts`: actual Luby-triggered restarts.
- `backjumps`: non-chronological backtracks.

Additional counters record root propagation, analyzed/minimized literals,
deleted clauses, reductions, assumptions, and activity rescaling. A
learned unit counts toward `learned_clauses`, even though it is not stored
in the non-unit learned-clause index list.

## Operational recommendations

- use as default production engine;
- persist learned-clause and conflict metrics for benchmark regression;
- pair with high-quality CNF encoding to maximize pruning efficiency.
