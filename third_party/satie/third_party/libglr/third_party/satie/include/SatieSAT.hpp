#pragma once

#include "Common.hpp"
#include "SatieNative.hpp"
#include "SatieDPLL.hpp"
#include "SatieCDCL.hpp"
#include "Satie.hpp"

namespace satie
{

/// Top-level SAT interface convenience functions and types.
using SolverType = Engine;

inline SolveResult solve_sat (const CNF &cnf, Engine engine = Engine::CDCL)
{
  return solve (cnf, engine);
}

inline SolveResult solve_sat (const std::string &dimacs_or_cnf, Engine engine = Engine::CDCL)
{
  return solve (dimacs_or_cnf, engine);
}

/// Version anchor defined in src/SatieSAT.cpp.
const char *sat_component_version () noexcept;

} // namespace satie
