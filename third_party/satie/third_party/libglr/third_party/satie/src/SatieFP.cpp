#include "SatieFP.hpp"
#include "SatieModule.hpp"

#include <memory>

namespace
{
// Boolean-abstraction adapter: batch 1 exposes every theory through the
// uniform TheoryModule registry. Batch 2 replaces each `check` body with a
// real DPLL(T) decision procedure while keeping this registration intact.
struct RegisteredModule : satie::TheoryModule
{
  std::string_view name () const noexcept override
  {
    return satie::fp::FPSolver::theory_name ();
  }
  satie::SolveResult check (const satie::CNF &cnf) const override
  {
    return satie::fp::FPSolver (cnf).check ();
  }
};
} // namespace

namespace satie
{

// External-linkage registration hook. Referenced by
// `ensure_theories_registered()` so static-archive linking cannot discard
// this translation unit before its registrar runs.
void satie_register_fp_theory ()
{
  static const TheoryRegistrar kRegistrar{
      std::string (fp::FPSolver::theory_name ()),
      [] { return std::make_unique<RegisteredModule> (); }};
  (void)kRegistrar;
}

} // namespace satie
