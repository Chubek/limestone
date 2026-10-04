#pragma once
#include "SatieADT.hpp"
#include "SatieAutomata.hpp"
#include "SatieBag.hpp"
#include "SatieBV.hpp"
#include "SatieEUF.hpp"
#include "SatieFP.hpp"
#include "SatieIDL.hpp"
#include "SatieISL.hpp"
#include "SatieLIA.hpp"
#include "SatieLP.hpp"
#include "SatieMILP.hpp"
#include "SatieNRA.hpp"
#include "SatieODE.hpp"
#include "SatiePoly.hpp"
#include "SatieQuant.hpp"
#include "SatieSequence.hpp"
#include "SatieSet.hpp"
#include "SatieSMT.hpp"
#include "SatieSymSolve.hpp"
#include <cstdint>

// Each registered case builds a different small CNF. An independent truth
// table is the oracle, so no solver under test supplies the expected result.
static void check_seeded_formula (std::uint32_t seed)
{
  satie::CNF cnf;
  for (unsigned i = 0; i < 1 + seed % 8; ++i)
    {
      satie::Clause clause;
      seed = seed * 1664525u + 1013904223u;
      for (unsigned j = 0; j < seed % 4; ++j)
        {
          seed = seed * 1664525u + 1013904223u;
          satie::Lit var = 1 + static_cast<satie::Lit> ((seed >> 8) % 4);
          clause.push_back ((seed & 128u) ? var : -var);
        }
      cnf.add_clause (std::move (clause));
    }
  bool expected = false;
  for (unsigned mask = 0; mask < 16; ++mask)
    {
      bool valid = true;
      for (const auto &clause : cnf.clauses ())
        {
          bool satisfied = false;
          for (satie::Lit lit : clause)
            satisfied |= (((mask >> (satie::literal_var (lit) - 1)) & 1u) != 0) == (lit > 0);
          valid &= satisfied;
        }
      expected |= valid;
    }
  for (satie::Engine engine : { satie::Engine::Native, satie::Engine::DPLL,
                                satie::Engine::CDCL })
    {
      auto result = satie::solve (cnf, engine);
      REQUIRE (result.satisfiable () == expected);
      if (result.satisfiable ())
        REQUIRE (satie::is_formula_satisfied (cnf, result.assignment));
    }
}

#define SATIE_FORMULA_CASE(n) TEST_CASE ("truth table formula " #n) { check_seeded_formula (n); }
SATIE_FORMULA_CASE (1)
SATIE_FORMULA_CASE (2)
SATIE_FORMULA_CASE (3)
SATIE_FORMULA_CASE (4)
SATIE_FORMULA_CASE (5)
SATIE_FORMULA_CASE (6)
SATIE_FORMULA_CASE (7)
SATIE_FORMULA_CASE (8)
SATIE_FORMULA_CASE (9)
SATIE_FORMULA_CASE (10)
SATIE_FORMULA_CASE (11)
SATIE_FORMULA_CASE (12)
SATIE_FORMULA_CASE (13)
SATIE_FORMULA_CASE (14)
SATIE_FORMULA_CASE (15)
SATIE_FORMULA_CASE (16)
SATIE_FORMULA_CASE (17)
SATIE_FORMULA_CASE (18)
SATIE_FORMULA_CASE (19)
SATIE_FORMULA_CASE (20)
SATIE_FORMULA_CASE (21)
SATIE_FORMULA_CASE (22)
SATIE_FORMULA_CASE (23)
SATIE_FORMULA_CASE (24)
SATIE_FORMULA_CASE (25)
SATIE_FORMULA_CASE (26)
SATIE_FORMULA_CASE (27)
SATIE_FORMULA_CASE (28)
SATIE_FORMULA_CASE (29)
SATIE_FORMULA_CASE (30)
SATIE_FORMULA_CASE (31)
SATIE_FORMULA_CASE (32)
SATIE_FORMULA_CASE (33)
SATIE_FORMULA_CASE (34)
SATIE_FORMULA_CASE (35)
SATIE_FORMULA_CASE (36)
SATIE_FORMULA_CASE (37)
SATIE_FORMULA_CASE (38)
SATIE_FORMULA_CASE (39)
SATIE_FORMULA_CASE (40)
SATIE_FORMULA_CASE (41)
SATIE_FORMULA_CASE (42)
SATIE_FORMULA_CASE (43)
SATIE_FORMULA_CASE (44)
SATIE_FORMULA_CASE (45)
SATIE_FORMULA_CASE (46)
SATIE_FORMULA_CASE (47)
SATIE_FORMULA_CASE (48)
SATIE_FORMULA_CASE (49)
SATIE_FORMULA_CASE (50)
SATIE_FORMULA_CASE (51)
SATIE_FORMULA_CASE (52)
SATIE_FORMULA_CASE (53)
SATIE_FORMULA_CASE (54)
SATIE_FORMULA_CASE (55)
SATIE_FORMULA_CASE (56)
SATIE_FORMULA_CASE (57)
SATIE_FORMULA_CASE (58)
SATIE_FORMULA_CASE (59)
SATIE_FORMULA_CASE (60)
SATIE_FORMULA_CASE (61)
SATIE_FORMULA_CASE (62)
SATIE_FORMULA_CASE (63)
SATIE_FORMULA_CASE (64)
SATIE_FORMULA_CASE (65)
SATIE_FORMULA_CASE (66)
SATIE_FORMULA_CASE (67)
SATIE_FORMULA_CASE (68)
SATIE_FORMULA_CASE (69)
SATIE_FORMULA_CASE (70)
SATIE_FORMULA_CASE (71)
SATIE_FORMULA_CASE (72)
SATIE_FORMULA_CASE (73)
SATIE_FORMULA_CASE (74)
SATIE_FORMULA_CASE (75)
SATIE_FORMULA_CASE (76)
SATIE_FORMULA_CASE (77)
SATIE_FORMULA_CASE (78)
SATIE_FORMULA_CASE (79)
SATIE_FORMULA_CASE (80)
SATIE_FORMULA_CASE (81)
SATIE_FORMULA_CASE (82)
SATIE_FORMULA_CASE (83)
SATIE_FORMULA_CASE (84)
SATIE_FORMULA_CASE (85)
SATIE_FORMULA_CASE (86)
SATIE_FORMULA_CASE (87)
SATIE_FORMULA_CASE (88)
SATIE_FORMULA_CASE (89)
SATIE_FORMULA_CASE (90)
SATIE_FORMULA_CASE (91)
SATIE_FORMULA_CASE (92)
SATIE_FORMULA_CASE (93)
SATIE_FORMULA_CASE (94)
SATIE_FORMULA_CASE (95)
SATIE_FORMULA_CASE (96)
SATIE_FORMULA_CASE (97)
SATIE_FORMULA_CASE (98)
SATIE_FORMULA_CASE (99)
SATIE_FORMULA_CASE (100)
SATIE_FORMULA_CASE (101)
SATIE_FORMULA_CASE (102)
SATIE_FORMULA_CASE (103)
SATIE_FORMULA_CASE (104)
SATIE_FORMULA_CASE (105)
SATIE_FORMULA_CASE (106)
SATIE_FORMULA_CASE (107)
SATIE_FORMULA_CASE (108)
SATIE_FORMULA_CASE (109)
SATIE_FORMULA_CASE (110)
SATIE_FORMULA_CASE (111)
SATIE_FORMULA_CASE (112)

#define SATIE_THEORY_CASE(type) \
  TEST_CASE ("Boolean abstraction " #type) { \
    type solver; \
    REQUIRE (solver.check ().satisfiable ()); \
    solver.load (satie::CNF ({{ 1 }, { -1 }})); \
    REQUIRE (solver.check ().unsatisfiable ()); \
    solver.load (satie::CNF ({{ 1, -2 }, { 2 }})); \
    auto result = solver.check (); \
    REQUIRE (result.satisfiable ()); \
    REQUIRE (satie::is_formula_satisfied (solver.problem (), result.assignment)); \
  }
SATIE_THEORY_CASE (satie::adt::ADTSolver)
SATIE_THEORY_CASE (satie::automata::AutomataSolver)
SATIE_THEORY_CASE (satie::bag::BagSolver)
SATIE_THEORY_CASE (satie::bv::BVSolver)
SATIE_THEORY_CASE (satie::euf::EUFSolver)
SATIE_THEORY_CASE (satie::fp::FPSolver)
SATIE_THEORY_CASE (satie::idl::IDLSolver)
SATIE_THEORY_CASE (satie::isl::ISLSolver)
SATIE_THEORY_CASE (satie::lia::LIASolver)
SATIE_THEORY_CASE (satie::lp::LPSolver)
SATIE_THEORY_CASE (satie::milp::MILPSolver)
SATIE_THEORY_CASE (satie::nra::NRASolver)
SATIE_THEORY_CASE (satie::ode::ODESolver)
SATIE_THEORY_CASE (satie::poly::PolySolver)
SATIE_THEORY_CASE (satie::quant::QuantSolver)
SATIE_THEORY_CASE (satie::sequence::SequenceSolver)
SATIE_THEORY_CASE (satie::set::SetSolver)
SATIE_THEORY_CASE (satie::symsolve::SymSolveSolver)
SATIE_THEORY_CASE (satie::smt::SMTSolver)
