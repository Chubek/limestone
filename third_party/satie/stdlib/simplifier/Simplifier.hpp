#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Common.hpp"

namespace satie::stdlib::simplifier
{

struct PreprocessStats
{
  std::uint64_t tautologies_removed = 0;
  std::uint64_t duplicates_removed = 0;
  std::uint64_t subsumed_removed = 0;
  std::uint64_t units_propagated = 0;
  std::uint64_t pure_eliminated = 0;
};

struct PreprocessResult
{
  CNF cnf;
  Assignment partial; // Top-level assignments (units + pure literals).
  PreprocessStats stats;
  bool trivially_unsat = false;
};

/// SAT preprocessing: tautology/duplicate removal, subsumption (bounded
/// quadratic), unit propagation to fixpoint, and pure-literal elimination.
/// Semantics-preserving: SAT answers extend via `partial`, and
/// `trivially_unsat` (empty clause derived) is exact.
inline PreprocessResult simplify_cnf (CNF cnf)
{
  PreprocessResult result;
  result.partial = Assignment (cnf.variable_count ());
  ClauseList clauses = cnf.clauses ();

  // Tautologies and duplicates.
  ClauseList cleaned;
  for (const Clause &clause : clauses)
    {
      if (is_tautology (clause))
        {
          ++result.stats.tautologies_removed;
          continue;
        }
      if (std::find (cleaned.begin (), cleaned.end (), clause) != cleaned.end ())
        {
          ++result.stats.duplicates_removed;
          continue;
        }
      cleaned.push_back (clause);
    }

  // Bounded subsumption (skip very long clauses quadratically).
  ClauseList kept;
  for (const Clause &clause : cleaned)
    {
      bool subsumed = false;
      for (const Clause &other : cleaned)
        {
          if (&other == &clause || other.size () > clause.size () || other.size () > 64)
            continue;
          if (clause_subsumes (other, clause))
            {
              subsumed = true;
              break;
            }
        }
      if (subsumed)
        ++result.stats.subsumed_removed;
      else
        kept.push_back (clause);
    }

  // Unit propagation to fixpoint over the kept clauses.
  bool changed = true;
  while (changed)
    {
      changed = false;
      for (const Clause &clause : kept)
        {
          bool satisfied = false;
          Lit open = 0;
          std::size_t open_count = 0;
          for (Lit lit : clause)
            {
              const Value value = result.partial.get_literal (lit);
              if (value == Value::TRUE)
                {
                  satisfied = true;
                  break;
                }
              if (value == Value::UNKNOWN)
                {
                  open = lit;
                  ++open_count;
                }
            }
          if (satisfied)
            continue;
          if (open_count == 0)
            {
              result.trivially_unsat = true;
              result.cnf = CNF ({{}});
              return result;
            }
          if (open_count == 1 && result.partial.assign_literal (open))
            {
              ++result.stats.units_propagated;
              changed = true;
            }
        }
    }

  // Pure literals among the unsatisfied remainder.
  std::unordered_set<Var> positive, negative;
  for (const Clause &clause : kept)
    {
      if (is_clause_satisfied (clause, result.partial))
        continue;
      for (Lit lit : clause)
        {
          const Var var = literal_var (lit);
          if (result.partial.is_assigned (var))
            continue;
          if (lit > 0)
            positive.insert (var);
          else
            negative.insert (var);
        }
    }
  for (Var var : positive)
    if (negative.count (var) == 0 && result.partial.assign (var, true))
      ++result.stats.pure_eliminated;
  for (Var var : negative)
    if (positive.count (var) == 0 && result.partial.assign (var, false))
      ++result.stats.pure_eliminated;

  // Drop satisfied clauses; keep the remainder verbatim.
  ClauseList remainder;
  for (const Clause &clause : kept)
    if (!is_clause_satisfied (clause, result.partial))
      remainder.push_back (clause);
  result.cnf = CNF (std::move (remainder));
  return result;
}

} // namespace satie::stdlib::simplifier
