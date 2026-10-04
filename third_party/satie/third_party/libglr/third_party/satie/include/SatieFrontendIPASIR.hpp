#pragma once

#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Common.hpp"
#include "SatieCDCL.hpp"
#include "SatieFrontendCommon.hpp"

namespace satie::frontend
{

/// Incremental solving in the IPASIR style (the IPASIR frontend).
///
/// Literals are added with `add` (0 terminates a clause); `assume` sets a
/// temporary assumption for the next `solve`; `solve` reports 10 (SAT),
/// 20 (UNSAT), or 0 (unknown, only on conflict budgets); `val` follows the
/// IPASIR sign convention (literal, negated literal, or 0); `failed`
/// reports assumption-core membership after UNSAT. Clause addition is
/// persistent across solves; assumptions are cleared by each `solve`.
/// The C bindings in this header mirror IPASIR-2 (`satie_ipasir_*`).
class IpasirSolver
{
public:
  IpasirSolver () = default;

  void add (Lit lit)
  {
    if (lit == 0)
      {
        clauses_.push_back (std::move (pending_));
        pending_.clear ();
        solved_ = false;
        return;
      }
    pending_.push_back (lit);
  }

  void assume (Lit lit)
  {
    if (lit == 0)
      throw std::invalid_argument ("IPASIR assumptions must be non-zero");
    assumptions_.push_back (lit);
  }

  int solve ()
  {
    CNF cnf (clauses_);
    if (!pending_.empty ())
      {
        // A trailing unterminated clause is carried, not solved.
      }
    const std::vector<Lit> active = assumptions_;
    CDCLSolver solver (std::move (cnf));
    solver.set_conflict_budget (budget_);
    SolveResult result = solver.solve_under (active);
    assumptions_.clear ();
    solved_ = true;
    last_ = result;
    core_ = solver.last_unsat_core ();
    if (result.satisfiable ())
      return 10;
    if (result.unsatisfiable ())
      return 20;
    return 0;
  }

  int val (Var var) const
  {
    if (!solved_ || !last_.satisfiable ())
      return 0;
    const Value value = last_.assignment.get_var (var);
    if (value == Value::TRUE)
      return var;
    if (value == Value::FALSE)
      return -var;
    return 0;
  }

  int failed (Lit assumption) const
  {
    if (!solved_ || !last_.unsatisfiable ())
      return 0;
    for (Lit core : core_)
      if (core == assumption)
        return 1;
    return 0;
  }

  void set_conflict_budget (std::uint64_t conflicts) { budget_ = conflicts; }
  std::uint64_t conflict_budget () const { return budget_; }

  const CNF &clauses () const
  {
    in_clauses_ = CNF (clauses_);
    return in_clauses_;
  }

private:
  ClauseList clauses_;
  Clause pending_;
  std::vector<Lit> assumptions_;
  SolveResult last_{};
  std::vector<Lit> core_;
  bool solved_ = false;
  std::uint64_t budget_ = 0;
  mutable CNF in_clauses_;
};

/// Version anchor defined in src/SatieFrontendIPASIR.cpp.
const char *frontend_ipasir_component_version () noexcept;

} // namespace satie::frontend

extern "C"
{

typedef struct SatieIpasirHandle SatieIpasirHandle;

SatieIpasirHandle *satie_ipasir_init (void);
void satie_ipasir_release (SatieIpasirHandle *handle);
void satie_ipasir_add (SatieIpasirHandle *handle, int lit);
void satie_ipasir_assume (SatieIpasirHandle *handle, int lit);
int satie_ipasir_solve (SatieIpasirHandle *handle);
int satie_ipasir_val (SatieIpasirHandle *handle, int var);
int satie_ipasir_failed (SatieIpasirHandle *handle, int lit);

} // extern "C"
