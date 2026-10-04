#pragma once

#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Common.hpp"
#include "Satie.hpp"
#include "SatieCDCL.hpp"
#include "SatieFrontendCommon.hpp"
#include "SatieTheoryUtils.hpp"

namespace satie::frontend
{

/// DIMACS-WCNF parsing and MaxSAT solving (the WCNF frontend).
///
/// Format: `c` comments, one `p wcnf <vars> <clauses> [top]` header line,
/// then `<weight> <lits> 0` rows; weight `>= top` (or `== top` when the
/// header carries it) marks a hard clause. Without a header, `top` defaults
/// to one plus the total weight. `solve_maxsat` runs an unweighted
/// relaxation loop: each soft clause gets a blocker, and the number of true
/// blockers is bounded by binary search using a sequential counter, so the
/// reported cost is optimal whenever the hard clauses are satisfiable.
inline WeightedCNF parse_wcnf (std::istream &input)
{
  WeightedCNF problem;
  std::string line;
  std::size_t line_no = 0;
  bool has_header = false;
  std::size_t declared_clauses = 0;
  std::vector<std::pair<long long, Clause>> rows;
  long long weight_sum = 0;

  while (std::getline (input, line))
    {
      ++line_no;
      std::size_t first = 0;
      while (first < line.size () &&
             std::isspace (static_cast<unsigned char> (line[first])) != 0)
        ++first;
      if (first >= line.size () || line[first] == 'c')
        continue;
      if (line[first] == '%')
        break;
      if (line[first] == 'p')
        {
          if (has_header)
            throw ParseError (line_no, first + 1, "duplicate problem line");
          std::stringstream header (line.substr (first));
          std::string p, kind;
          std::size_t vars = 0, count = 0;
          long long top = 0;
          if (!(header >> p >> kind >> vars >> count) || p != "p" || kind != "wcnf")
            throw ParseError (line_no, first + 1, "expected 'p wcnf <vars> <clauses> [top]'");
          has_header = true;
          problem.variables = vars;
          declared_clauses = count;
          if (header >> top)
            {
              if (top <= 0)
                throw ParseError (line_no, first + 1, "top weight must be positive");
              problem.top = top;
            }
          continue;
        }
      std::stringstream row (line.substr (first));
      long long weight = 0;
      if (!(row >> weight) || weight <= 0)
        throw ParseError (line_no, first + 1, "expected a positive clause weight");
      Clause clause;
      long long raw = 0;
      bool terminated = false;
      while (row >> raw)
        {
          if (raw < std::numeric_limits<Lit>::min () ||
              raw > std::numeric_limits<Lit>::max ())
            throw ParseError (line_no, first + 1, "literal exceeds int32 range");
          Lit lit = static_cast<Lit> (raw);
          if (lit == 0)
            {
              terminated = true;
              break;
            }
          if (problem.variables != 0 &&
              static_cast<std::size_t> (literal_var (lit)) > problem.variables)
            throw ParseError (line_no, first + 1, "literal exceeds declared variable count");
          clause.push_back (lit);
        }
      if (!terminated)
        throw ParseError (line_no, first + 1, "unterminated clause; missing 0");
      weight_sum += weight;
      rows.push_back ({ weight, std::move (clause) });
    }

  if (!has_header)
    problem.top = weight_sum + 1;
  else if (problem.top == 0)
    problem.top = weight_sum + 1;
  for (auto &[weight, clause] : rows)
    {
      if (weight >= problem.top)
        problem.hard.push_back (std::move (clause));
      else
        problem.soft.push_back ({ std::move (clause), weight });
    }
  if (has_header && rows.size () != declared_clauses)
    throw ParseError (line_no, 1, "clause count does not match problem line");
  CNF probe (problem.hard);
  problem.variables = std::max (problem.variables, probe.variable_count ());
  return problem;
}

inline WeightedCNF parse_wcnf_text (const std::string &text)
{
  std::istringstream input (text);
  return parse_wcnf (input);
}

enum class MaxSATStatus
{
  Optimal,
  UnsatHard,
  Unknown
};

struct MaxSATResult
{
  MaxSATStatus status = MaxSATStatus::Unknown;
  long long cost = 0;
  Assignment assignment{};
};

/// Optimal MaxSAT by binary search over the falsified-weight bound. Each
/// soft clause `c` with weight `w` is relaxed `k` times (unit-weight
/// copies); at-most-`k` over the blockers is enforced with a sequential
/// counter. Only satisfiable hard parts reach the loop, so UNSAT answers
/// are exact.
inline MaxSATResult solve_maxsat (const WeightedCNF &problem,
                                  Engine engine = Engine::CDCL)
{
  (void)engine; // The search below drives CDCL directly.
  CNF hard (problem.hard);
  SolveResult hard_check = solve_cdcl (hard);
  if (hard_check.unsatisfiable ())
    return { MaxSATStatus::UnsatHard, -1, Assignment (problem.variables) };
  if (problem.soft.empty ())
    return { MaxSATStatus::Optimal, 0, hard_check.assignment };

  // Expand weighted soft clauses into unit-weight copies.
  std::vector<Clause> copies;
  for (const WeightedClause &soft : problem.soft)
    for (long long i = 0; i < soft.weight; ++i)
      copies.push_back (soft.clause);
  const long long total = static_cast<long long> (copies.size ());
  if (total > 2000000000)
    throw std::invalid_argument ("MaxSAT total weight exceeds the supported bound");

  Var next = static_cast<Var> (problem.variables) + 1;
  std::vector<Lit> blockers;
  ClauseList relaxed;
  for (const Clause &copy : copies)
    {
      const Lit blocker = next++;
      blockers.push_back (blocker);
      Clause relaxed_clause = copy;
      relaxed_clause.push_back (blocker);
      relaxed.push_back (std::move (relaxed_clause));
    }

  long long lo = 0, hi = total;
  Assignment best = hard_check.assignment;
  // Feasibility at `hi = total` always holds (block every soft clause).
  while (lo < hi)
    {
      const long long mid = lo + (hi - lo) / 2;
      theory::Encoder enc (next);
      for (const Clause &clause : relaxed)
        enc.add_clause (clause);
      enc.at_most_k (blockers, static_cast<int> (mid));
      CNF candidate = hard;
      for (Clause &clause : enc.take_clauses ())
        candidate.add_clause (std::move (clause));
      SolveResult attempt = solve_cdcl (candidate);
      if (attempt.satisfiable ())
        {
          hi = mid;
          best = attempt.assignment;
        }
      else
        lo = mid + 1;
    }
  return { MaxSATStatus::Optimal, lo, best };
}

/// Version anchor defined in src/SatieFrontendWCNF.cpp.
const char *frontend_wcnf_component_version () noexcept;

} // namespace satie::frontend
