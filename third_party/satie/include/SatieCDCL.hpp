#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

#include "Common.hpp"
#include "SatieMemory.hpp"

namespace satie
{

// ============================================================
// Public API
// ============================================================

/// Conflict-Driven Clause Learning engine.
///
/// * two watched literals per clause with a propagation queue,
/// * first-UIP conflict analysis with recursive clause minimization,
/// * VSIDS branching with an activity decay and phase saving,
/// * Luby-scheduled restarts and a bounded learned-clause database,
/// * assumption support with unsat-core extraction.
///
/// `solve`, `solve_under`, and every counter are reset at the start of each
/// search, so an instance is reusable across `load`/`solve` cycles.
class CDCLSolver
{
public:
  struct Statistics
  {
    std::uint64_t decisions = 0;
    std::uint64_t propagations = 0;
    std::uint64_t conflicts = 0;
    std::uint64_t learned_clauses = 0;
    std::uint64_t restarts = 0;
    std::uint64_t backjumps = 0;
    // --- extended counters (appended; source compatible with older code) ---
    std::uint64_t root_propagations = 0;
    std::uint64_t analysed_literals = 0;
    std::uint64_t deleted_clauses = 0;
    std::uint64_t reduced_databases = 0;
    std::uint64_t assumptions = 0;
    std::uint64_t tautologies_removed = 0;
    std::uint64_t max_learned_clause_length = 0;
    std::uint64_t activity_rescales = 0;
    std::uint64_t minimized_literals = 0;
    std::uint64_t conflicts_since_restart = 0;
  };

  CDCLSolver () = default;
  explicit CDCLSolver (CNF cnf) { load (std::move (cnf)); }

  void load (CNF cnf)
  {
    original_ = dimacs_to_cnf (cnf_to_dimacs (std::move (cnf)));
    initialize ();
  }

  const CNF &problem () const noexcept { return original_; }

  const Statistics &stats () const { return stats_; }
  const Statistics &statistics () const { return stats_; }

  /// Aborts the search with `SolveStatus::UNKNOWN` after `conflicts`
  /// conflicts; `0` means "no limit".
  void set_conflict_budget (std::uint64_t conflicts) noexcept
  {
    conflict_limit_ = conflicts;
  }
  std::uint64_t conflict_budget () const noexcept { return conflict_limit_; }

  void set_restarts_enabled (bool enabled) noexcept { restarts_enabled_ = enabled; }
  void set_max_learned_clauses (std::size_t limit) noexcept { max_learned_clauses_ = limit; }
  void set_luby_scale (std::uint64_t scale) noexcept { luby_scale_ = scale == 0 ? 1 : scale; }
  void set_enable_minimization (bool enabled) noexcept { minimize_ = enabled; }

  SolveResult solve () { return solve_under ({}); }

  /// Solves under assumptions.  Assumption literals occupy the first
  /// decision levels; `last_unsat_core()` then reports a minimized subset of
  /// the assumptions responsible for an UNSAT answer (empty when the formula
  /// is unsatisfiable on its own). Assumptions are never silently dropped:
  /// backtracking below them re-asserts them, and a learnt clause falsified
  /// by them proves UNSAT.
  SolveResult solve_under (const std::vector<Lit> &assumptions)
  {
    for (Lit lit : assumptions)
      if (lit == 0 || lit == std::numeric_limits<Lit>::min () ||
          static_cast<std::size_t> (literal_var (lit)) > original_.variable_count ())
        throw std::invalid_argument ("assumption is outside the problem variable range");
    initialize ();
    assumptions_ = assumptions;
    stats_.assumptions = assumptions.size ();
    assumption_core_.clear ();
    if (unsatisfiable_at_root_)
      return unsat_result ();
    if (!assume_all ())
      return unsat_with_core ();

    luby_index_ = 0;
    restart_budget_ = luby (luby_index_) * luby_scale_;
    stats_.conflicts_since_restart = 0;

    while (true)
      {
        const int conflict = propagate ();
        if (conflict >= 0)
          {
            ++stats_.conflicts;
            ++stats_.conflicts_since_restart;
            if (decision_level_ == 0)
              return unsat_with_core ();

            Clause learned;
            int backtrack_level = 0;
            analyze (conflict, learned, backtrack_level);
            cancel_until (backtrack_level);
            if (!reassume_dropped ())
              return unsat_with_core ();
            if (learned.size () == 1)
              {
                // A unit learned clause is still a learned clause: count it so
                // statistics stay meaningful, then assert it at level 0.
                ++stats_.learned_clauses;
                stats_.max_learned_clause_length =
                    std::max<std::uint64_t> (stats_.max_learned_clause_length, 1);
                if (literal_false (learned.front ()))
                  return unsat_with_core ();
                if (!literal_true (learned.front ()))
                  unchecked_enqueue (learned.front ());
              }
            else
              {
                if (learned.empty () || literal_false (learned.front ()))
                  return unsat_with_core ();
                attach_and_assert (std::move (learned));
              }
            bump_decay ();

            if (restarts_enabled_ && stats_.conflicts_since_restart >= restart_budget_)
              {
                cancel_until (0);
                if (!reassume_dropped ())
                  return unsat_with_core ();
                ++stats_.restarts;
                ++luby_index_;
                restart_budget_ = luby (luby_index_) * luby_scale_;
                stats_.conflicts_since_restart = 0;
                reduce_database ();
              }
            if (conflict_limit_ != 0 && stats_.conflicts >= conflict_limit_)
              return { SolveStatus::UNKNOWN, Assignment (original_.variable_count ()) };
            continue;
          }

        if (all_variables_assigned ())
          return { SolveStatus::SAT, build_assignment () };

        const std::optional<Lit> next = pick_branch_literal ();
        if (!next)
          return { SolveStatus::SAT, build_assignment () };
        ++stats_.decisions;
        new_decision_level ();
        unchecked_enqueue (*next);
      }
  }

  /// Literals (negated) of the assumption subset that caused the last UNSAT
  /// answer.  Empty when UNSAT does not depend on the assumptions.
  const std::vector<Lit> &last_unsat_core () const noexcept { return assumption_core_; }

  const Clause &last_learned_clause () const noexcept { return last_learned_clause_; }

  std::size_t learned_clause_count () const noexcept { return learned_clause_indices_.size (); }

private:
  /// Enqueues every assumption at a fresh decision level. Returns false when
  /// one is already falsified (the caller then reports UNSAT with a core).
  bool assume_all ()
  {
    for (Lit lit : assumptions_)
      {
        if (literal_true (lit))
          {
            new_decision_level ();
            continue;
          }
        if (literal_false (lit))
          return false;
        new_decision_level ();
        unchecked_enqueue (lit);
      }
    return true;
  }

  /// Re-asserts assumptions dropped by backtracking below their levels.
  /// Returns false when one is falsified (UNSAT with a core).
  bool reassume_dropped ()
  {
    for (Lit lit : assumptions_)
      {
        if (literal_true (lit) || literal_false (lit))
          {
            if (literal_false (lit))
              return false;
            continue;
          }
        new_decision_level ();
        unchecked_enqueue (lit);
      }
    return true;
  }

  /// UNSAT verdict with a deletion-minimized assumption core (empty when the
  /// formula is unsatisfiable on its own).
  SolveResult unsat_with_core ()
  {
    assumption_core_.clear ();
    if (!assumptions_.empty ())
      {
        std::vector<Lit> core = assumptions_;
        for (std::size_t i = 0; i < core.size ();)
          {
            std::vector<Lit> trial;
            for (std::size_t j = 0; j < core.size (); ++j)
              if (j != i)
                trial.push_back (core[j]);
            CDCLSolver probe (original_);
            probe.set_conflict_budget (0);
            probe.set_restarts_enabled (false);
            if (probe.solve_under_no_core (trial).unsatisfiable ())
              core = std::move (trial);
            else
              ++i;
          }
        assumption_core_ = std::move (core);
      }
    return unsat_result ();
  }

  /// Core-free entry point for minimization probes (avoids recursion).
  SolveResult solve_under_no_core (const std::vector<Lit> &assumptions)
  {
    initialize ();
    assumptions_ = assumptions;
    assumption_core_.clear ();
    if (unsatisfiable_at_root_)
      return unsat_result ();
    if (!assume_all ())
      return unsat_result ();

    while (true)
      {
        const int conflict = propagate ();
        if (conflict >= 0)
          {
            if (decision_level_ == 0)
              return unsat_result ();
            Clause learned;
            int backtrack_level = 0;
            analyze (conflict, learned, backtrack_level);
            cancel_until (backtrack_level);
            if (!reassume_dropped ())
              return unsat_result ();
            if (learned.empty ())
              return unsat_result ();
            if (literal_false (learned.front ()))
              return unsat_result ();
            if (learned.size () == 1)
              {
                if (!literal_true (learned.front ()))
                  unchecked_enqueue (learned.front ());
              }
            else
              attach_and_assert (std::move (learned));
            continue;
          }
        if (all_variables_assigned ())
          return { SolveStatus::SAT, build_assignment () };
        const std::optional<Lit> next = pick_branch_literal ();
        if (!next)
          return { SolveStatus::SAT, build_assignment () };
        new_decision_level ();
        unchecked_enqueue (*next);
      }
  }

  /// Attaches a learnt clause and asserts its first literal unless already
  /// satisfied. Callers check falsification beforehand.
  void attach_and_assert (Clause clause)
  {
    const int index = static_cast<int> (clauses_.size ());
    clauses_.push_back (clause);
    learned_clause_indices_.push_back (static_cast<std::size_t> (index));
    clause_activity_.push_back (1.0);
    attach_clause (index, clauses_.back ());
    const Lit first = clauses_.back ().front ();
    if (!literal_true (first) && !literal_false (first))
      {
        unchecked_enqueue (first);
        reasons_[static_cast<std::size_t> (literal_var (first))] =
            static_cast<std::size_t> (index);
      }
  }
  struct Watcher
  {
    int clause = -1;
    Lit blocker = 0;
  };

  static constexpr std::size_t no_clause = static_cast<std::size_t> (-1);

  // ---- setup -------------------------------------------------------------

  void initialize ()
  {
    const std::size_t variables = original_.variable_count ();
    clauses_ = original_.clauses_without_tautologies ();
    unsatisfiable_at_root_ = original_.has_empty_clause ();
    decision_level_ = 0;
    luby_index_ = 0;
    var_increment_ = 1.0;
    trail_head_ = 0;
    assumption_core_.clear ();
    last_learned_clause_.clear ();
    learned_clause_indices_.clear ();
    trail_.clear ();
    trail_limits_.clear ();
    stats_ = {};
    stats_.tautologies_removed =
        original_.clause_count () - std::min (original_.clause_count (), clauses_.size ());
    if (unsatisfiable_at_root_)
      return;

    watches_.assign (2 * (variables + 1), {});
    original_clause_count_ = clauses_.size ();
    values_.assign (variables + 1, Value::UNKNOWN);
    levels_.assign (variables + 1, 0);
    reasons_.assign (variables + 1, no_clause);
    activity_.assign (variables + 1, 0.0);
    clause_activity_.assign (clauses_.size (), 0.0);
    phase_.assign (variables + 1, 1);
    seen_.assign (variables + 1, 0);

    for (std::size_t index = 0; index < clauses_.size (); ++index)
      {
        const Clause &clause = clauses_[index];
        attach_clause (static_cast<int> (index), clause);
        // Unit clauses are asserted at level 0 so they act before any decision.
        if (clause.size () == 1)
          unchecked_enqueue (clause.front ());
      }
  }

  SolveResult unsat_result () const
  {
    return { SolveStatus::UNSAT, Assignment (original_.variable_count ()) };
  }

  // ---- literal helpers ---------------------------------------------------

  static int literal_index (Lit lit) noexcept
  {
    return 2 * literal_var (lit) + (lit > 0 ? 1 : 0);
  }

  bool literal_true (Lit lit) const noexcept
  {
    const Value value = values_[static_cast<std::size_t> (literal_var (lit))];
    return value == (lit > 0 ? Value::TRUE : Value::FALSE);
  }
  bool literal_false (Lit lit) const noexcept
  {
    const Value value = values_[static_cast<std::size_t> (literal_var (lit))];
    return value == (lit > 0 ? Value::FALSE : Value::TRUE);
  }

  void unchecked_enqueue (Lit lit)
  {
    const std::size_t variable = static_cast<std::size_t> (literal_var (lit));
    const Value next = lit > 0 ? Value::TRUE : Value::FALSE;
    // Never overwrite an existing decision.  A contradicting assignment means
    // the clause database already contains a falsified clause, which
    // `propagate` reports as a conflict; skipping keeps the trail consistent.
    if (values_[variable] != Value::UNKNOWN)
      return;
    values_[variable] = next;
    phase_[variable] = lit > 0 ? 1 : 0;
    levels_[variable] = decision_level_;
    reasons_[variable] = no_clause;
    trail_.push_back (lit);
  }

  // ---- clause database ---------------------------------------------------

  void attach_clause (int index, const Clause &clause)
  {
    if (clause.size () >= 2)
      {
        watches_[static_cast<std::size_t> (literal_index (negate (clause[0])))].push_back (
            Watcher{ index, clause[1] });
        watches_[static_cast<std::size_t> (literal_index (negate (clause[1])))].push_back (
            Watcher{ index, clause[0] });
        return;
      }
    // Unit clause: watch its single literal twice so that falsifying it is
    // reported as a conflict (this is how opposite units are detected).
    const Lit unit = clause[0];
    watches_[static_cast<std::size_t> (literal_index (negate (unit)))].push_back (
        Watcher{ index, unit });
    watches_[static_cast<std::size_t> (literal_index (negate (unit)))].push_back (
        Watcher{ index, unit });
  }

  void attach_learned (Clause clause)
  {
    ++stats_.learned_clauses;
    stats_.max_learned_clause_length =
        std::max<std::uint64_t> (stats_.max_learned_clause_length, clause.size ());
    const int index = static_cast<int> (clauses_.size ());
    clauses_.push_back (std::move (clause));
    learned_clause_indices_.push_back (static_cast<std::size_t> (index));
    clause_activity_.push_back (1.0);
    attach_clause (index, clauses_.back ());
  }

  /// Unit propagation to fixpoint.  Returns the index of a falsified clause, or
  /// -1 when the current partial assignment is conflict free.
  int propagate ()
  {
    while (trail_head_ < trail_.size ())
      {
        const Lit propagated = trail_[trail_head_++];
        std::vector<Watcher> &list =
            watches_[static_cast<std::size_t> (literal_index (propagated))];
        const Lit false_lit = negate (propagated);
        std::size_t read = 0;
        std::size_t write = 0;
        while (read < list.size ())
          {
            const Watcher watcher = list[read];
            Clause &clause = clauses_[static_cast<std::size_t> (watcher.clause)];
            if (clause.size () >= 2 && clause[0] == false_lit)
              std::swap (clause[0], clause[1]);
            const Lit first = clause[0];

            if (literal_true (first))
              {
                list[write++] = Watcher{ watcher.clause, first };
                ++read;
                continue;
              }

            bool moved = false;
            for (std::size_t position = 2; position < clause.size (); ++position)
              {
                if (literal_false (clause[position]))
                  continue;
                clause[1] = clause[position];
                clause[position] = false_lit;
                // The new watch must fire when `clause[1]` becomes false, so
                // it is keyed by the negation (as in `attach_clause`).
                watches_[static_cast<std::size_t> (literal_index (negate (clause[1])))].push_back (
                    Watcher{ watcher.clause, first });
                moved = true;
                break;
              }
            if (moved)
              {
                ++read;
                continue;
              }

            list[write++] = Watcher{ watcher.clause, first };
            ++read;
            if (literal_false (first))
              {
                // Conflict: preserve the untouched tail of the watcher list.
                while (read < list.size ())
                  list[write++] = list[read++];
                list.resize (write);
                trail_head_ = trail_.size ();
                return watcher.clause;
              }
            unchecked_enqueue (first);
            reasons_[static_cast<std::size_t> (literal_var (first))] =
                static_cast<std::size_t> (watcher.clause);
            ++stats_.propagations;
            if (decision_level_ == 0)
              ++stats_.root_propagations;
          }
        list.resize (write);
      }
    return -1;
  }

  // ---- conflict analysis -------------------------------------------------

  void bump_variable (std::size_t variable)
  {
    activity_[variable] += var_increment_;
    if (activity_[variable] > 1e100)
      {
        ++stats_.activity_rescales;
        for (double &value : activity_)
          value *= 1e-100;
        var_increment_ *= 1e-100;
      }
  }

  void bump_decay () noexcept { var_increment_ *= (1.0 / 0.95); }

  void rescale_activities ()
  {
    ++stats_.activity_rescales;
    for (double &value : activity_)
      value *= 1e-100;
    for (double &value : clause_activity_)
      value *= 1e-100;
    var_increment_ *= 1e-100;
  }


  /// First-UIP analysis.  `learned` receives the asserting clause whose first
  /// literal is the negation of the UIP; `backtrack_level` is the level of the
  /// remaining literals.
  void analyze (int conflict, Clause &learned, int &backtrack_level)
  {
    learned.clear ();
    learned.push_back (0); // asserting literal placeholder
    int open = 0;
    Lit pivot = 0;
    std::size_t index = trail_.size ();

    do
      {
        const Clause &reason_clause = clauses_[static_cast<std::size_t> (conflict)];
        clause_activity_[static_cast<std::size_t> (conflict)] += 1.0;
        for (std::size_t position = (pivot == 0) ? 0 : 1; position < reason_clause.size ();
             ++position)
          {
            const Lit lit = reason_clause[position];
            const std::size_t variable = static_cast<std::size_t> (literal_var (lit));
            if (seen_[variable] || levels_[variable] == 0)
              continue;
            seen_[variable] = 1;
            bump_variable (variable);
            ++stats_.analysed_literals;
            if (levels_[variable] >= decision_level_)
              ++open;
            else
              learned.push_back (lit);
          }
        while (!seen_[static_cast<std::size_t> (literal_var (trail_[--index]))])
          ;
        pivot = trail_[index];
        const std::size_t reason = reasons_[static_cast<std::size_t> (literal_var (pivot))];
        seen_[static_cast<std::size_t> (literal_var (pivot))] = 0;
        --open;
        conflict = reason == no_clause ? -1 : static_cast<int> (reason);
      }
    while (open > 0 && conflict >= 0);

    learned[0] = negate (pivot);

    if (minimize_)
      minimize_clause (learned);

    backtrack_level = 0;
    for (std::size_t position = 1; position < learned.size (); ++position)
      backtrack_level =
          std::max (backtrack_level, levels_[static_cast<std::size_t> (literal_var (learned[position]))]);

    for (const Lit lit : learned)
      seen_[static_cast<std::size_t> (literal_var (lit))] = 0;
    last_learned_clause_ = learned;
  }

  /// Recursive clause minimization: a literal of the learned clause is implied
  /// by the formula when its own reason is built from literals that are either
  /// at level 0 or themselves redundant.
  void minimize_clause (Clause &learned)
  {
    Clause kept;
    kept.push_back (learned[0]);
    for (std::size_t position = 1; position < learned.size (); ++position)
      if (!is_redundant (learned[position]))
        kept.push_back (learned[position]);
      else
        ++stats_.minimized_literals;
    learned = std::move (kept);
  }

  bool is_redundant (Lit lit)
  {
    MemoryResource memory;
    std::pmr::vector<Lit> stack (&memory);
    stack.push_back (lit);
    while (!stack.empty ())
      {
        const Lit current = stack.back ();
        stack.pop_back ();
        const std::size_t variable = static_cast<std::size_t> (literal_var (current));
        if (!seen_[variable])
          continue;
        const std::size_t reason = reasons_[variable];
        if (reason == no_clause)
          return false;
        const Clause &reason_clause = clauses_[reason];
        for (std::size_t position = 1; position < reason_clause.size (); ++position)
          {
            const Lit other = reason_clause[position];
            const std::size_t other_variable = static_cast<std::size_t> (literal_var (other));
            if (other_variable == variable || levels_[other_variable] == 0)
              continue;
            if (!seen_[other_variable])
              return false;
            stack.push_back (other);
          }
      }
    return true;
  }


  // ---- trail management --------------------------------------------------

  void new_decision_level ()
  {
    trail_limits_.push_back (static_cast<std::size_t> (trail_.size ()));
    ++decision_level_;
  }

  void cancel_until (int level)
  {
    if (decision_level_ <= level)
      return;
    ++stats_.backjumps;
    const std::size_t target =
        level < static_cast<int> (trail_limits_.size ())
            ? trail_limits_[static_cast<std::size_t> (level)]
            : 0;
    for (std::size_t index = trail_.size (); index-- > target;)
      {
        const std::size_t variable = static_cast<std::size_t> (literal_var (trail_[index]));
        values_[variable] = Value::UNKNOWN;
        levels_[variable] = 0;
        reasons_[variable] = no_clause;
      }
    trail_.resize (target);
    trail_limits_.resize (static_cast<std::size_t> (std::max (level, 0)));
    trail_head_ = std::min<std::size_t> (trail_head_, trail_.size ());
    decision_level_ = std::max (level, 0);
  }

  bool all_variables_assigned () const noexcept
  {
    for (Var v = first_variable; v <= static_cast<Var> (original_.variable_count ()); ++v)
      if (values_[static_cast<std::size_t> (v)] == Value::UNKNOWN)
        return false;
    return true;
  }

  std::optional<Lit> pick_branch_literal () const noexcept
  {
    Var best = 0;
    double best_activity = -1.0;
    for (Var v = first_variable; v <= static_cast<Var> (original_.variable_count ()); ++v)
      {
        const std::size_t variable = static_cast<std::size_t> (v);
        if (values_[variable] != Value::UNKNOWN)
          continue;
        if (activity_[variable] > best_activity)
          {
            best_activity = activity_[variable];
            best = v;
          }
      }
    if (best == 0)
      return std::nullopt;
    return make_literal (best, phase_[static_cast<std::size_t> (best)] == 0);
  }

  // ---- learned clause database ------------------------------------------

  void reduce_database ()
  {
    if (learned_clause_indices_.size () <= max_learned_clauses_)
      return;
    ++stats_.reduced_databases;
    MemoryResource memory (MemoryLifetime::Transient);
    std::pmr::vector<std::size_t> order (learned_clause_indices_.begin (),
                                        learned_clause_indices_.end (), &memory);
    std::sort (order.begin (), order.end (),
               [this] (std::size_t lhs, std::size_t rhs) {
                 if (clause_activity_[lhs] != clause_activity_[rhs])
                   return clause_activity_[lhs] < clause_activity_[rhs];
                 return clauses_[lhs].size () > clauses_[rhs].size ();
               });
    const std::size_t keep = learned_clause_indices_.size () / 2;
    std::vector<std::size_t> retained (order.begin () + static_cast<std::ptrdiff_t> (keep),
                                       order.end ());
    std::vector<char> retained_flag (clauses_.size (), 0);
    for (const std::size_t index : retained)
      retained_flag[index] = 1;

    // Rebuild every watch list: original clauses are always kept, only half of
    // the learned clauses survive the reduction.
    std::vector<std::vector<Watcher>> rebuilt (watches_.size ());
    for (std::size_t index = 0; index < original_clause_count_; ++index)
      {
        const Clause &clause = clauses_[index];
        if (clause.size () >= 2)
          {
            rebuilt[static_cast<std::size_t> (literal_index (negate (clause[0])))].push_back (
                Watcher{ static_cast<int> (index), clause[1] });
            rebuilt[static_cast<std::size_t> (literal_index (negate (clause[1])))].push_back (
                Watcher{ static_cast<int> (index), clause[0] });
          }
        else
          {
            rebuilt[static_cast<std::size_t> (literal_index (negate (clause[0])))].push_back (
                Watcher{ static_cast<int> (index), clause[0] });
            rebuilt[static_cast<std::size_t> (literal_index (negate (clause[0])))].push_back (
                Watcher{ static_cast<int> (index), clause[0] });
          }
      }
    for (const std::size_t index : learned_clause_indices_)
      {
        if (!retained_flag[index])
          continue;
        const Clause &clause = clauses_[index];
        rebuilt[static_cast<std::size_t> (literal_index (negate (clause[0])))].push_back (
            Watcher{ static_cast<int> (index), clause[1] });
        rebuilt[static_cast<std::size_t> (literal_index (negate (clause[1])))].push_back (
            Watcher{ static_cast<int> (index), clause[0] });
      }
    stats_.deleted_clauses +=
        static_cast<std::uint64_t> (learned_clause_indices_.size () - retained.size ());
    watches_.swap (rebuilt);
    learned_clause_indices_ = std::move (retained);
    for (double &value : clause_activity_)
      value = 0.0;
  }

  /// Luby restart sequence, zero based: 1, 1, 2, 1, 1, 2, 4, 1, ...
  static std::uint64_t luby (std::uint64_t index) noexcept
  {
    std::uint64_t position = index + 1;
    std::uint64_t exponent = 1;
    while (((std::uint64_t{1} << (exponent + 1)) - 1) <= position)
      ++exponent;
    const std::uint64_t peak = (std::uint64_t{1} << exponent) - 1;
    if (position == peak)
      return std::uint64_t{1} << (exponent - 1);
    return luby (position - (std::uint64_t{1} << (exponent - 1)) - 1);
  }

  Assignment build_assignment () const
  {
    Assignment out (original_.variable_count ());
    for (Var v = first_variable; v <= static_cast<Var> (original_.variable_count ()); ++v)
      {
        const Value value = values_[static_cast<std::size_t> (v)];
        if (value == Value::TRUE)
          out.assign (v, true);
        else if (value == Value::FALSE)
          out.assign (v, false);
      }
    return out;
  }

  // ---- state -------------------------------------------------------------

  CNF original_{};
  ClauseList clauses_{};
  std::size_t original_clause_count_ = 0;
  std::vector<std::vector<Watcher>> watches_;
  std::vector<Value> values_;
  std::vector<int> levels_;
  std::vector<std::size_t> reasons_;
  std::vector<double> activity_;
  std::vector<double> clause_activity_;
  std::vector<char> phase_;
  std::vector<char> seen_;
  std::vector<Lit> trail_;
  std::vector<std::size_t> trail_limits_;
  std::size_t trail_head_ = 0;
  std::vector<std::size_t> learned_clause_indices_;
  std::vector<Lit> assumptions_;
  std::vector<Lit> assumption_core_;
  Clause last_learned_clause_{};
  int decision_level_ = 0;
  double var_increment_ = 1.0;
  std::uint64_t luby_index_ = 0;
  std::uint64_t luby_scale_ = 128;
  std::uint64_t restart_budget_ = 128;
  std::uint64_t conflict_limit_ = 0;
  std::size_t max_learned_clauses_ = 4000;
  bool restarts_enabled_ = true;
  bool minimize_ = true;
  bool unsatisfiable_at_root_ = false;
  Statistics stats_{};
};

inline SolveResult solve_cdcl (const CNF &cnf)
{
  CDCLSolver solver (cnf);
  return solver.solve ();
}
inline SolveResult solve_cdcl (const CNF &cnf, const std::vector<Lit> &assumptions)
{
  CDCLSolver solver (cnf);
  return solver.solve_under (assumptions);
}
inline bool is_satisfiable_cdcl (const CNF &cnf) { return solve_cdcl (cnf).satisfiable (); }

/// Version anchor defined in src/SatieCDCL.cpp; keeps the translation unit
/// non-empty and lets tooling query the linked CDCL component.
const char *cdcl_component_version () noexcept;

} // namespace satie
