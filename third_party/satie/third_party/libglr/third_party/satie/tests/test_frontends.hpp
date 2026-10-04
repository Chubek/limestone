#pragma once

#include "catch_shim.hpp"

#include "Satie.hpp"
#include "SatieCDCL.hpp"
#include "SatieFrontendCNF.hpp"
#include "SatieFrontendCommon.hpp"
#include "SatieFrontendIPASIR.hpp"
#include "SatieFrontendOPB.hpp"
#include "SatieFrontendSMTLIB2.hpp"
#include "SatieFrontendSatieLisp.hpp"
#include "SatieFrontendWCNF.hpp"

namespace fe = satie::frontend;

// ---- Bool formulas (CNF frontend) ------------------------------------------

TEST_CASE ("frontend parses full Boolean formulas") {
  fe::BoolFormula formula = fe::parse_bool_formula ("(a | ~b) & (b -> c)");
  REQUIRE (formula.cnf.clause_count () > 2);
  REQUIRE (formula.symbols.lookup ("a").has_value ());
  REQUIRE (satie::solve (formula.cnf).satisfiable ());
}

TEST_CASE ("frontend xor chain is equisatisfiable") {
  fe::BoolFormula formula = fe::parse_bool_formula ("(a ^ b) & (a <-> b)");
  REQUIRE (satie::solve (formula.cnf).unsatisfiable ());
}

TEST_CASE ("frontend implication and equivalence") {
  fe::BoolFormula sat = fe::parse_bool_formula ("(a -> b) & a & b");
  REQUIRE (satie::solve (sat.cnf).satisfiable ());
  fe::BoolFormula unsat = fe::parse_bool_formula ("(a -> b) & a & ~b");
  REQUIRE (satie::solve (unsat.cnf).unsatisfiable ());
}

TEST_CASE ("frontend rejects malformed formulas") {
  REQUIRE_THROWS_AS (fe::parse_bool_formula ("(a & )"), satie::ParseError);
  REQUIRE_THROWS_AS (fe::parse_bool_formula ("a b"), satie::ParseError);
}

TEST_CASE ("frontend component versions are non-empty") {
  REQUIRE (fe::frontend_common_component_version () != nullptr);
  REQUIRE (fe::frontend_cnf_component_version () != nullptr);
  REQUIRE (fe::frontend_wcnf_component_version () != nullptr);
  REQUIRE (fe::frontend_opb_component_version () != nullptr);
  REQUIRE (fe::frontend_smtlib2_component_version () != nullptr);
  REQUIRE (fe::frontend_ipasir_component_version () != nullptr);
  REQUIRE (fe::frontend_satielisp_component_version () != nullptr);
}

// ---- WCNF / MaxSAT ----------------------------------------------------------

TEST_CASE ("frontend parses WCNF hard and soft") {
  fe::WeightedCNF problem =
      fe::parse_wcnf_text ("p wcnf 2 3 10\n10 1 0\n10 2 0\n1 -1 -2 0\n");
  REQUIRE (problem.hard.size () == 2);
  REQUIRE (problem.soft.size () == 1);
  REQUIRE (problem.top == 10);
}

TEST_CASE ("frontend MaxSAT finds the optimum") {
  fe::WeightedCNF problem =
      fe::parse_wcnf_text ("p wcnf 2 3 10\n10 1 0\n10 2 0\n1 -1 -2 0\n");
  fe::MaxSATResult result = fe::solve_maxsat (problem);
  REQUIRE (result.status == fe::MaxSATStatus::Optimal);
  REQUIRE (result.cost == 1);
}

TEST_CASE ("frontend MaxSAT reports hard UNSAT") {
  fe::WeightedCNF problem =
      fe::parse_wcnf_text ("p wcnf 1 2 10\n10 1 0\n10 -1 0\n");
  fe::MaxSATResult result = fe::solve_maxsat (problem);
  REQUIRE (result.status == fe::MaxSATStatus::UnsatHard);
}

TEST_CASE ("frontend WCNF rejects bad input") {
  REQUIRE_THROWS_AS (fe::parse_wcnf_text ("p wcnf 1 1\n1\n"), satie::ParseError);
}

// ---- OPB --------------------------------------------------------------------

TEST_CASE ("frontend parses and solves OPB") {
  fe::PBProblem problem =
      fe::parse_opb_text ("* t\nmin: 1 x1 +1 x2;\n1 x1 +1 x2 >= 1;");
  REQUIRE (problem.variables == 2);
  REQUIRE (problem.constraints.size () == 1);
  REQUIRE (fe::solve_pb (problem).satisfiable ());
}

TEST_CASE ("frontend OPB detects UNSAT") {
  fe::PBProblem problem = fe::parse_opb_text ("1 x1 >= 1;\n1 x1 <= 0;");
  REQUIRE (fe::solve_pb (problem).unsatisfiable ());
}

TEST_CASE ("frontend OPB optimizes the objective") {
  fe::PBProblem problem =
      fe::parse_opb_text ("min: 1 x1 +2 x2;\n1 x1 +1 x2 >= 1;");
  fe::PBOptimum optimum = fe::optimize_pb_min (problem);
  REQUIRE (optimum.status == satie::SolveStatus::SAT);
  REQUIRE (optimum.value == 1);
}

TEST_CASE ("frontend OPB rejects bad variables") {
  REQUIRE_THROWS_AS (fe::parse_opb_text ("1 y1 >= 1;"), satie::ParseError);
}

// ---- SMT-LIB2 ---------------------------------------------------------------

TEST_CASE ("frontend SMT-LIB2 solves Boolean scripts") {
  fe::SMTCheckResult result = fe::solve_smtlib2_text (
      "(declare-const a Bool)\n(declare-const b Bool)\n"
      "(assert (and a (not b)))\n(check-sat)\n");
  REQUIRE (result.status == satie::SolveStatus::SAT);
  REQUIRE (result.model.bools["a"] == true);
  REQUIRE (result.model.bools["b"] == false);
}

TEST_CASE ("frontend SMT-LIB2 solves LIA scripts") {
  fe::SMTCheckResult result = fe::solve_smtlib2_text (
      "(set-logic QF_LIA)\n(declare-const x Int)\n"
      "(assert (>= x 2))\n(assert (<= x 5))\n(check-sat)\n");
  REQUIRE (result.status == satie::SolveStatus::SAT);
  REQUIRE (result.model.ints["x"] >= 2);
  REQUIRE (result.model.ints["x"] <= 5);
}

TEST_CASE ("frontend SMT-LIB2 reports LIA UNSAT") {
  fe::SMTCheckResult result = fe::solve_smtlib2_text (
      "(declare-const x Int)\n(assert (>= x 2))\n(assert (<= x 1))\n(check-sat)\n");
  REQUIRE (result.status == satie::SolveStatus::UNSAT);
}

TEST_CASE ("frontend SMT-LIB2 mixed fragments are UNKNOWN") {
  fe::SMTCheckResult result = fe::solve_smtlib2_text (
      "(declare-const a Bool)\n(declare-const x Int)\n"
      "(assert (and a (>= x 1)))\n(check-sat)\n");
  REQUIRE (result.status == satie::SolveStatus::UNKNOWN);
}

TEST_CASE ("frontend SMT-LIB2 requires check-sat") {
  REQUIRE_THROWS_AS (fe::solve_smtlib2_text ("(declare-const a Bool)\n"),
                     satie::ParseError);
}

// ---- SatieLisp ---------------------------------------------------------------

TEST_CASE ("frontend SatieLisp solves formulas") {
  fe::LispResult result = fe::solve_satielisp ("(and a (or b (not a)))");
  REQUIRE (result.status == satie::SolveStatus::SAT);
  REQUIRE (result.model["a"] == true);
}

TEST_CASE ("frontend SatieLisp detects UNSAT") {
  fe::LispResult result = fe::solve_satielisp ("(and a (not a))");
  REQUIRE (result.status == satie::SolveStatus::UNSAT);
}

TEST_CASE ("frontend SatieLisp rejects unknown heads") {
  REQUIRE_THROWS_AS (fe::solve_satielisp ("(frob a b)"), satie::ParseError);
}

// ---- IPASIR ------------------------------------------------------------------

TEST_CASE ("frontend IPASIR solves incrementally") {
  fe::IpasirSolver solver;
  solver.add (1);
  solver.add (0);
  REQUIRE (solver.solve () == 10);
  REQUIRE (solver.val (1) == 1);
}

TEST_CASE ("frontend IPASIR assumptions give cores") {
  fe::IpasirSolver solver;
  solver.add (1);
  solver.add (0);
  solver.assume (-1);
  REQUIRE (solver.solve () == 20);
  REQUIRE (solver.failed (-1) == 1);
  REQUIRE (solver.failed (2) == 0);
}

TEST_CASE ("frontend IPASIR keeps clauses across solves") {
  fe::IpasirSolver solver;
  solver.add (1);
  solver.add (2);
  solver.add (0);
  REQUIRE (solver.solve () == 10);
  solver.add (-1);
  solver.add (0);
  solver.assume (-2);
  REQUIRE (solver.solve () == 20);
}

TEST_CASE ("frontend IPASIR C API round-trips") {
  SatieIpasirHandle *handle = satie_ipasir_init ();
  REQUIRE (handle != nullptr);
  satie_ipasir_add (handle, 1);
  satie_ipasir_add (handle, 0);
  REQUIRE (satie_ipasir_solve (handle) == 10);
  REQUIRE (satie_ipasir_val (handle, 1) == 1);
  satie_ipasir_release (handle);
}

// ---- Assumption soundness (CDCL regression) --------------------------------
// Backtracking once cancelled assumptions and asserted their negation,
// reporting SAT under contradictory assumptions.

TEST_CASE ("frontend assumptions are never dropped") {
  satie::CNF cnf ({{ 1, 2 }, { -1 }});
  satie::CDCLSolver solver (cnf);
  SolveResult result = solver.solve_under ({ -2 });
  REQUIRE (result.unsatisfiable ());
  bool mentions = false;
  for (satie::Lit lit : solver.last_unsat_core ())
    if (lit == -2)
      mentions = true;
  REQUIRE (mentions);
}

TEST_CASE ("frontend assumption cores are minimal") {
  satie::CNF cnf ({{ 1 }});
  satie::CDCLSolver solver (cnf);
  SolveResult result = solver.solve_under ({ 1, 2 });
  REQUIRE (result.satisfiable ());
  satie::CDCLSolver solver2 (satie::CNF ({{ 1 }}));
  SolveResult result2 = solver2.solve_under ({ -1, 2 });
  REQUIRE (result2.unsatisfiable ());
  bool has_neg1 = false, has_2 = false;
  for (satie::Lit lit : solver2.last_unsat_core ())
    {
      has_neg1 |= lit == -1;
      has_2 |= lit == 2;
    }
  REQUIRE (has_neg1);
  REQUIRE (!has_2); // 2 is irrelevant to the contradiction.
}

TEST_CASE ("frontend assumptions survive restarts") {
  satie::CNF cnf ({{ 1, 2 }, { 1, -2 }, { -1, 2 }, { -1, -2 }, { 3 }});
  satie::CDCLSolver solver (cnf);
  solver.set_restarts_enabled (true);
  solver.set_luby_scale (1);
  SolveResult result = solver.solve_under ({ -3 });
  REQUIRE (result.unsatisfiable ());
}
