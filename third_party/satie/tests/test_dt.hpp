#pragma once

#include "catch_shim.hpp"

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
#include "SatieSymSolve.hpp"

// ---- BV ------------------------------------------------------------------

TEST_CASE ("BV equality is SAT with a model") {
  satie::bv::BVSolver solver;
  solver.add_var ("x", 4);
  solver.add_var ("y", 4);
  solver.add_eq ("x", "y");
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (solver.value ("x") == solver.value ("y"));
}

TEST_CASE ("BV opposing orders are UNSAT") {
  satie::bv::BVSolver solver;
  solver.add_var ("x", 4);
  solver.add_var ("y", 4);
  solver.add_ult ("x", "y");
  solver.add_ult ("y", "x");
  REQUIRE (solver.check ().unsatisfiable ());
}

TEST_CASE ("BV modular addition wraps") {
  satie::bv::BVSolver solver;
  solver.add_var ("x", 4);
  solver.add_var ("y", 4);
  solver.add_var ("z", 4);
  solver.add_eq_const ("x", 15);
  solver.add_eq_const ("y", 1);
  solver.add_add ("x", "y", "z");
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (solver.value ("z").value () == 0); // 15 + 1 == 0 mod 16
}

TEST_CASE ("BV wrong sum is UNSAT") {
  satie::bv::BVSolver solver;
  solver.add_var ("x", 4);
  solver.add_var ("y", 4);
  solver.add_var ("z", 4);
  solver.add_eq_const ("x", 15);
  solver.add_eq_const ("y", 1);
  solver.add_eq_const ("z", 1); // 15 + 1 != 1 mod 16
  solver.add_add ("x", "y", "z");
  REQUIRE (solver.check ().unsatisfiable ());
}

// ---- EUF -----------------------------------------------------------------

TEST_CASE ("EUF congruence propagates") {
  satie::euf::EUFSolver solver;
  int f = solver.add_func ("f", 1);
  int x = solver.add_var ("x");
  int y = solver.add_var ("y");
  int fx = solver.mk_app (f, { x });
  int fy = solver.mk_app (f, { y });
  solver.add_eq (x, y);
  solver.add_diseq (fx, fy);
  REQUIRE (solver.check ().unsatisfiable ());
}

TEST_CASE ("EUF distinct constants stay SAT") {
  satie::euf::EUFSolver solver;
  int a = solver.add_const ("a");
  int b = solver.add_const ("b");
  solver.add_diseq (a, b);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (!solver.equal_terms (a, b));
}

// ---- IDL -----------------------------------------------------------------

TEST_CASE ("IDL feasible bounds give a model") {
  satie::idl::IDLSolver solver;
  int x = solver.add_var ("x");
  int y = solver.add_var ("y");
  solver.add_le (x, y, 5);
  solver.add_le (y, x, -2);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (solver.value (x) - solver.value (y) <= 5);
  REQUIRE (solver.value (y) - solver.value (x) <= -2);
}

TEST_CASE ("IDL negative cycle is UNSAT") {
  satie::idl::IDLSolver solver;
  int x = solver.add_var ("x");
  int y = solver.add_var ("y");
  solver.add_le (x, y, -1);
  solver.add_le (y, x, -1);
  REQUIRE (solver.check ().unsatisfiable ());
}

// ---- LP ------------------------------------------------------------------

TEST_CASE ("LP feasible box gives a model") {
  satie::lp::LPSolver solver;
  int x = solver.add_var ("x");
  solver.add_bound (x, 1.0, 2.0);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (solver.value (x) >= 1.0);
  REQUIRE (solver.value (x) <= 2.0);
}

TEST_CASE ("LP contradictory bounds are UNSAT") {
  satie::lp::LPSolver solver;
  int x = solver.add_var ("x");
  solver.add_le ({ { x, 1.0 } }, 1.0);
  solver.add_le ({ { x, -1.0 } }, -2.0);
  REQUIRE (solver.check ().unsatisfiable ());
}

TEST_CASE ("LP two-variable system") {
  satie::lp::LPSolver solver;
  int x = solver.add_var ("x");
  int y = solver.add_var ("y");
  solver.add_le ({ { x, 1.0 }, { y, 1.0 } }, 3.0);
  solver.add_le ({ { x, -1.0 } }, 0.0);
  solver.add_le ({ { y, -1.0 } }, 0.0);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (solver.value (x) + solver.value (y) <= 3.0 + 1e-6);
}

// ---- LIA -----------------------------------------------------------------

TEST_CASE ("LIA linear system is SAT") {
  satie::lia::LIASolver solver;
  int x = solver.add_var ("x");
  int y = solver.add_var ("y");
  solver.add_le ({ { x, 2 }, { y, 3 } }, 20);
  solver.add_le ({ { x, -1 } }, 0);
  solver.add_le ({ { y, -1 } }, 0);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (2 * solver.value (x) + 3 * solver.value (y) <= 20);
}

TEST_CASE ("LIA rationally infeasible is UNSAT") {
  satie::lia::LIASolver solver;
  int x = solver.add_var ("x");
  solver.add_le ({ { x, 1 } }, 1);
  solver.add_le ({ { x, -1 } }, -2);
  REQUIRE (solver.check ().unsatisfiable ());
}

TEST_CASE ("LIA integer-infeasible is UNKNOWN") {
  satie::lia::LIASolver solver;
  int x = solver.add_var ("x");
  solver.add_eq ({ { x, 2 } }, 3); // 2x == 3 has no integer solution
  auto result = solver.check ();
  REQUIRE (result.status == satie::SolveStatus::UNKNOWN);
}

// ---- ISL -----------------------------------------------------------------

TEST_CASE ("ISL even stride is SAT") {
  satie::isl::ISLSolver solver;
  int x = solver.add_var ("x");
  solver.add_bound (x, 0, 10);
  solver.add_divisible (x, 2, 0);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE ((solver.value (x) % 2 + 2) % 2 == 0);
}

TEST_CASE ("ISL clashing strides are UNSAT") {
  satie::isl::ISLSolver solver;
  int x = solver.add_var ("x");
  solver.add_bound (x, 0, 10);
  solver.add_divisible (x, 2, 0);
  solver.add_divisible (x, 2, 1);
  REQUIRE (solver.check ().unsatisfiable ());
}

TEST_CASE ("ISL unbounded stride failure is UNKNOWN") {
  satie::isl::ISLSolver solver;
  int x = solver.add_var ("x");
  solver.add_divisible (x, 2, 1); // No bound; IDL model 0 violates the stride.
  auto result = solver.check ();
  REQUIRE (result.status == satie::SolveStatus::UNKNOWN);
}

// ---- MILP ----------------------------------------------------------------

TEST_CASE ("MILP integer point is SAT") {
  satie::milp::MILPSolver solver;
  int x = solver.add_var ("x", true);
  solver.add_le ({ { x, 1.0 } }, 2.5);
  solver.add_le ({ { x, -1.0 } }, -2.0);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (solver.value (x) == 2.0);
}

TEST_CASE ("MILP fractional-forced is UNSAT") {
  satie::milp::MILPSolver solver;
  int x = solver.add_var ("x", true);
  solver.add_le ({ { x, 2.0 } }, 3.0);
  solver.add_le ({ { x, -2.0 } }, -3.0); // 2x == 3: no integer
  REQUIRE (solver.check ().unsatisfiable ());
}

// ---- Automata --------------------------------------------------------------

TEST_CASE ("Automata membership gives a witness") {
  satie::automata::AutomataSolver solver;
  int v = solver.add_var ("v");
  solver.add_match (v, satie::automata::AutomataSolver::string ("ab"));
  solver.add_length_eq (v, 2);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (solver.value (v) == "ab");
}

TEST_CASE ("Automata mismatch is UNSAT") {
  satie::automata::AutomataSolver solver;
  int v = solver.add_var ("v");
  solver.add_match (v, satie::automata::AutomataSolver::string ("a"));
  solver.add_match (v, satie::automata::AutomataSolver::string ("b"));
  REQUIRE (solver.check ().unsatisfiable ());
}

TEST_CASE ("Automata star accepts the empty word") {
  satie::automata::AutomataSolver solver;
  int v = solver.add_var ("v");
  solver.add_match (v, satie::automata::AutomataSolver::star (
                            satie::automata::AutomataSolver::ch ('a')));
  solver.add_length_eq (v, 0);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (solver.value (v).empty ());
}

// ---- Sequence --------------------------------------------------------------

TEST_CASE ("Sequence concat lengths agree") {
  satie::sequence::SequenceSolver solver;
  int a = solver.add_var ("a");
  int b = solver.add_var ("b");
  int c = solver.add_var ("c");
  solver.add_length_eq (a, 2);
  solver.add_length_eq (b, 1);
  solver.add_concat_vars (c, a, b);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (solver.length_value (c) == 3);
}

TEST_CASE ("Sequence element conflict is UNSAT") {
  satie::sequence::SequenceSolver solver;
  int a = solver.add_var ("a");
  solver.add_length_eq (a, 2);
  solver.add_at (a, 0, 1);
  solver.add_at (a, 0, 2);
  REQUIRE (solver.check ().unsatisfiable ());
}

// ---- Set -------------------------------------------------------------------

TEST_CASE ("Set union with cardinality is SAT") {
  satie::set::SetSolver solver;
  int e1 = solver.add_element ("e1");
  int e2 = solver.add_element ("e2");
  int s = solver.add_set ("s");
  int t = solver.add_set ("t");
  int u = solver.add_set ("u");
  solver.add_member (e1, s);
  solver.add_member (e2, t);
  solver.add_union (u, s, t);
  solver.add_card_eq (u, 2);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (solver.members_value (u).size () == 2);
}

TEST_CASE ("Set cardinality contradiction is UNSAT") {
  satie::set::SetSolver solver;
  int e1 = solver.add_element ("e1");
  int s = solver.add_set ("s");
  solver.add_member (e1, s);
  solver.add_card_eq (s, 0);
  REQUIRE (solver.check ().unsatisfiable ());
}

// ---- Bag -------------------------------------------------------------------

TEST_CASE ("Bag counts are SAT") {
  satie::bag::BagSolver solver;
  int e = solver.add_element ("e");
  int b = solver.add_bag ("b");
  solver.add_count_eq (b, e, 3);
  solver.add_card_eq (b, 3);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (solver.count_value (b, e) == 3);
}

TEST_CASE ("Bag overfull card is UNSAT") {
  satie::bag::BagSolver solver;
  int e = solver.add_element ("e");
  int b = solver.add_bag ("b");
  solver.add_count_eq (b, e, 3);
  solver.add_card_le (b, 2);
  REQUIRE (solver.check ().unsatisfiable ());
}

// ---- ADT -------------------------------------------------------------------

TEST_CASE ("ADT occurs check is UNSAT") {
  satie::adt::ADTSolver solver;
  int f = solver.add_constructor ("f", 1);
  int x = solver.add_var ("x");
  int fx = solver.mk_app (f, { x });
  solver.add_eq (x, fx);
  REQUIRE (solver.check ().unsatisfiable ());
}

TEST_CASE ("ADT disequality of constants is SAT") {
  satie::adt::ADTSolver solver;
  int nil = solver.add_constructor ("nil", 0);
  int cons = solver.add_constructor ("cons", 1);
  int a = solver.mk_app (nil, {});
  int b = solver.mk_app (cons, { a });
  solver.add_diseq (a, b);
  REQUIRE (solver.check ().satisfiable ());
}

TEST_CASE ("ADT wrong tester is UNSAT") {
  satie::adt::ADTSolver solver;
  int nil = solver.add_constructor ("nil", 0);
  int cons = solver.add_constructor ("cons", 1);
  int a = solver.mk_app (nil, {});
  int b = solver.mk_app (cons, { a });
  solver.add_tester (b, nil);
  REQUIRE (solver.check ().unsatisfiable ());
}

// ---- Quant -----------------------------------------------------------------

TEST_CASE ("Quant exists is SAT") {
  satie::quant::QuantSolver solver;
  int x = solver.add_bool_var ("x");
  solver.add_exists ({ x });
  solver.add_matrix_clause ({ { x, false } });
  REQUIRE (solver.check ().satisfiable ());
}

TEST_CASE ("Quant forall over a bare variable is UNSAT") {
  satie::quant::QuantSolver solver;
  int x = solver.add_bool_var ("x");
  solver.add_forall ({ x });
  solver.add_matrix_clause ({ { x, false } });
  REQUIRE (solver.check ().unsatisfiable ());
}

TEST_CASE ("Quant integer domain exists") {
  satie::quant::QuantSolver solver;
  int n = solver.add_int_var ("n", { 1, 2, 3 });
  solver.add_exists_int ({ n });
  solver.add_int_eq (n, 2);
  REQUIRE (solver.check ().satisfiable ());
}

// ---- Poly ------------------------------------------------------------------

TEST_CASE ("Poly integer roots solve x^2 == 4") {
  satie::poly::PolySolver solver;
  int x = solver.add_var ("x");
  auto poly = satie::poly::PolySolver::make_poly (
      { { { 0 }, -4 }, { { 1 }, 0 }, { { 2 }, 1 } }, 1);
  (void)x;
  solver.add_constraint (poly, satie::poly::PolySolver::Sense::Eq);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (solver.value (0) * solver.value (0) == 4);
}

TEST_CASE ("Poly bounded miss is UNSAT") {
  satie::poly::PolySolver solver;
  int x = solver.add_var ("x");
  solver.add_bound (x, -2, 2);
  auto poly = satie::poly::PolySolver::make_poly (
      { { { 0 }, -3 }, { { 2 }, 1 } }, 1); // x^2 == 3: no integer root
  solver.add_constraint (poly, satie::poly::PolySolver::Sense::Eq);
  REQUIRE (solver.check ().unsatisfiable ());
}

// ---- NRA -------------------------------------------------------------------

TEST_CASE ("NRA square root is SAT") {
  satie::nra::NRASolver solver;
  int x = solver.add_var ("x");
  (void)x;
  auto poly = satie::nra::NRASolver::make_poly (
      { { { 0 }, -2.0 }, { { 2 }, 1.0 } }, 1); // x^2 == 2
  solver.add_constraint (poly, satie::nra::NRASolver::Sense::Eq);
  REQUIRE (solver.check ().satisfiable ());
  double v = solver.value (x);
  REQUIRE ((v * v > 1.99 && v * v < 2.01));
}

TEST_CASE ("NRA linear contradiction is UNSAT") {
  satie::nra::NRASolver solver;
  int x = solver.add_var ("x");
  (void)x;
  auto lo = satie::nra::NRASolver::make_poly ({ { { 0 }, 1.0 }, { { 1 }, -1.0 } }, 1);
  auto hi = satie::nra::NRASolver::make_poly ({ { { 0 }, -2.0 }, { { 1 }, 1.0 } }, 1);
  solver.add_constraint (lo, satie::nra::NRASolver::Sense::Le); // x >= 1
  solver.add_constraint (hi, satie::nra::NRASolver::Sense::Lt); // x < 2... SAT alone
  REQUIRE (solver.check ().satisfiable ());
  auto tight = satie::nra::NRASolver::make_poly ({ { { 0 }, 2.0 }, { { 1 }, -1.0 } }, 1);
  solver.add_constraint (tight, satie::nra::NRASolver::Sense::Le); // x >= 2
  auto tighter = satie::nra::NRASolver::make_poly ({ { { 0 }, -1.0 }, { { 1 }, 1.0 } }, 1);
  solver.add_constraint (tighter, satie::nra::NRASolver::Sense::Lt); // x < 1
  REQUIRE (solver.check ().unsatisfiable ());
}

TEST_CASE ("NRA negative square is UNKNOWN") {
  satie::nra::NRASolver solver;
  int x = solver.add_var ("x");
  (void)x;
  auto poly = satie::nra::NRASolver::make_poly ({ { { 2 }, 1.0 } }, 1); // x^2 < 0
  solver.add_constraint (poly, satie::nra::NRASolver::Sense::Lt);
  auto result = solver.check ();
  REQUIRE (result.status == satie::SolveStatus::UNKNOWN);
}

// ---- FP --------------------------------------------------------------------

TEST_CASE ("FP positive bound is SAT") {
  satie::fp::FPSolver solver;
  int x = solver.add_var ("x");
  int ex = solver.mk_var (x);
  int zero = solver.mk_const (0.0);
  solver.add_cmp (satie::fp::FPSolver::Cmp::Gt, ex, zero);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (solver.value (x) > 0.0);
}

TEST_CASE ("FP clashing bounds are UNSAT") {
  satie::fp::FPSolver solver;
  int x = solver.add_var ("x");
  int ex = solver.mk_var (x);
  int zero = solver.mk_const (0.0);
  solver.add_cmp (satie::fp::FPSolver::Cmp::Gt, ex, zero);
  solver.add_cmp (satie::fp::FPSolver::Cmp::Lt, ex, zero);
  REQUIRE (solver.check ().unsatisfiable ());
}

TEST_CASE ("FP NaN disequality is SAT") {
  satie::fp::FPSolver solver;
  int x = solver.add_var ("x");
  int ex = solver.mk_var (x);
  solver.add_cmp (satie::fp::FPSolver::Cmp::Ne, ex, ex);
  REQUIRE (solver.check ().satisfiable ());
}

// ---- SymSolve ---------------------------------------------------------------

TEST_CASE ("SymSolve linear system is SAT") {
  satie::symsolve::SymSolveSolver solver;
  int x = solver.add_var ("x");
  int y = solver.add_var ("y");
  int ex = solver.mk_var (x);
  int ey = solver.mk_var (y);
  int three = solver.mk_const (3.0);
  int one = solver.mk_const (1.0);
  solver.add_eq (solver.mk_binary (satie::symsolve::SymSolveSolver::Op::Add, ex, ey),
                 three);
  solver.add_eq (solver.mk_binary (satie::symsolve::SymSolveSolver::Op::Sub, ex, ey),
                 one);
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE ((solver.value (x) > 1.99 && solver.value (x) < 2.01));
  REQUIRE ((solver.value (y) > 0.99 && solver.value (y) < 1.01));
}

TEST_CASE ("SymSolve inconsistent linear is UNSAT") {
  satie::symsolve::SymSolveSolver solver;
  int x = solver.add_var ("x");
  int ex = solver.mk_var (x);
  solver.add_eq (ex, solver.mk_const (1.0));
  solver.add_eq (ex, solver.mk_const (2.0));
  REQUIRE (solver.check ().unsatisfiable ());
}

TEST_CASE ("SymSolve quadratic is SAT") {
  satie::symsolve::SymSolveSolver solver;
  int x = solver.add_var ("x");
  int ex = solver.mk_var (x);
  solver.add_eq (solver.mk_pow (ex, 2), solver.mk_const (4.0));
  REQUIRE (solver.check ().satisfiable ());
  double v = solver.value (x);
  REQUIRE ((v * v > 3.99 && v * v < 4.01));
}

// ---- ODE --------------------------------------------------------------------

TEST_CASE ("ODE driftless safety is SAT") {
  satie::ode::ODESolver solver;
  solver.add_affine ({ 0.0, 0.0 }, { 0.0, 0.0 }, { 5.0, 5.0 }, 1.0, { 4.0, 6.0 });
  REQUIRE (solver.check ().satisfiable ());
}

TEST_CASE ("ODE missed target is UNSAT") {
  satie::ode::ODESolver solver;
  solver.add_affine ({ 0.0, 0.0 }, { 0.0, 0.0 }, { 5.0, 5.0 }, 1.0, { 6.0, 7.0 });
  REQUIRE (solver.check ().unsatisfiable ());
}

TEST_CASE ("ODE exponential growth reaches") {
  satie::ode::ODESolver solver;
  solver.add_affine ({ 1.0, 1.0 }, { 0.0, 0.0 }, { 1.0, 1.0 }, 1.0, { 2.7, 2.8 });
  REQUIRE (solver.check ().satisfiable ());
  REQUIRE (!solver.system_model (0).empty ());
}
