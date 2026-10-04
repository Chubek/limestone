#pragma once

#include "catch_shim.hpp"

#include "SatieModule.hpp"
#include "SatieModule.h"
#include "SatiePlugin.h"
#include "SatieSMT.hpp"
#include "SatieCDCL.hpp"
#include "SatieDPLL.hpp"
#include "SatieNative.hpp"
#include "SatieSAT.hpp"

#include <cstring>
#include <string>

TEST_CASE ("module registry is non-empty") {
  REQUIRE (satie::registered_theory_count () > 0);
  REQUIRE (satie::TheoryRegistry::instance ().size () > 0);
}

TEST_CASE ("module registry holds all theories") {
  auto names = satie::TheoryRegistry::instance ().names ();
  REQUIRE (names.size () == 18);
}

TEST_CASE ("module registry contains BV") {
  auto m = satie::TheoryRegistry::instance ().create ("BV");
  REQUIRE (m != nullptr && m->name () == "BV");
}

TEST_CASE ("module registry contains EUF") {
  auto m = satie::TheoryRegistry::instance ().create ("EUF");
  REQUIRE (m != nullptr && m->name () == "EUF");
}

TEST_CASE ("module registry contains LIA") {
  auto m = satie::TheoryRegistry::instance ().create ("LIA");
  REQUIRE (m != nullptr);
}

TEST_CASE ("module registry contains BV check SAT") {
  auto m = satie::TheoryRegistry::instance ().create ("BV");
  REQUIRE (m->check (satie::CNF{}).satisfiable ());
}

TEST_CASE ("module registry contains BV check UNSAT") {
  auto m = satie::TheoryRegistry::instance ().create ("BV");
  REQUIRE (m->check (satie::CNF ({{ 1 }, { -1 }})).unsatisfiable ());
}

TEST_CASE ("module registry rejects unknown theory") {
  REQUIRE (satie::TheoryRegistry::instance ().create ("NOPE") == nullptr);
}

TEST_CASE ("module registry checks every theory SAT") {
  for (const std::string &name : satie::TheoryRegistry::instance ().names ()) {
    auto m = satie::TheoryRegistry::instance ().create (name);
    REQUIRE (m != nullptr && m->check (satie::CNF{}).satisfiable ());
  }
}

TEST_CASE ("module registry checks every theory UNSAT") {
  satie::CNF contra ({{ 1 }, { -1 }});
  for (const std::string &name : satie::TheoryRegistry::instance ().names ()) {
    auto m = satie::TheoryRegistry::instance ().create (name);
    REQUIRE (m != nullptr && m->check (contra).unsatisfiable ());
  }
}

TEST_CASE ("C module count matches registry") {
  REQUIRE (satie_module_count () == satie::TheoryRegistry::instance ().size ());
}

TEST_CASE ("C module name_at enumerates theories") {
  REQUIRE (satie_module_count () == 18);
  bool found_bv = false;
  for (size_t i = 0; i < satie_module_count (); ++i) {
    const char *n = satie_module_name_at (i);
    REQUIRE (n != nullptr);
    if (std::string (n) == "BV")
      found_bv = true;
  }
  REQUIRE (found_bv);
  REQUIRE (satie_module_name_at (1000) == nullptr);
}

TEST_CASE ("C module create and check SAT") {
  SatieCModule *m = satie_module_create ("BV");
  REQUIRE (m != nullptr);
  REQUIRE (satie_module_check (m) == SATIE_C_MODULE_SAT);
  satie_module_destroy (m);
}

TEST_CASE ("C module create and check UNSAT") {
  SatieCModule *m = satie_module_create ("EUF");
  REQUIRE (m != nullptr);
  const int c1[] = { 1 };
  const int c2[] = { -1 };
  REQUIRE (satie_module_add_clause (m, c1, 1) == 0);
  REQUIRE (satie_module_add_clause (m, c2, 1) == 0);
  REQUIRE (satie_module_check (m) == SATIE_C_MODULE_UNSAT);
  satie_module_destroy (m);
}

TEST_CASE ("C module rejects unknown theory") {
  REQUIRE (satie_module_create ("NOPE") == nullptr);
  REQUIRE (satie_module_create (nullptr) == nullptr);
  REQUIRE (std::string (satie_module_last_error ()).size () > 0);
}

TEST_CASE ("C module name reports theory") {
  SatieCModule *m = satie_module_create ("LIA");
  REQUIRE (std::string (satie_module_name (m)) == "LIA");
  REQUIRE (satie_module_name (nullptr) == nullptr);
  satie_module_destroy (m);
}

TEST_CASE ("C module rejects zero literal") {
  SatieCModule *m = satie_module_create ("BV");
  const int bad[] = { 0 };
  REQUIRE (satie_module_add_clause (m, bad, 1) != 0);
  satie_module_destroy (m);
}

TEST_CASE ("C module handles NULL module") {
  REQUIRE (satie_module_check (nullptr) == SATIE_C_MODULE_ERROR);
  REQUIRE (satie_module_add_clause (nullptr, nullptr, 0) != 0);
  satie_module_destroy (nullptr);
}

TEST_CASE ("C plugin creates context") {
  SatieCPluginContext *c = satie_plugin_context_create ();
  REQUIRE (c != nullptr);
  satie_plugin_context_destroy (c);
}

static const char *test_echo_fn (const char *const *args, size_t count, void *userdata) {
  (void)userdata;
  static thread_local std::string out;
  out = "n=" + std::to_string (count);
  for (size_t i = 0; i < count; ++i)
    out += std::string ("|") + (args[i] ? args[i] : "<null>");
  return out.c_str ();
}

TEST_CASE ("C plugin registers and invokes") {
  SatieCPluginContext *c = satie_plugin_context_create ();
  REQUIRE (satie_plugin_register (c, "echo", test_echo_fn, nullptr) == 0);
  REQUIRE (satie_plugin_has (c, "echo") == 1);
  const char *args[] = { "a", "b" };
  char out[64];
  REQUIRE (satie_plugin_invoke (c, "echo", args, 2, out, sizeof (out)) == 0);
  REQUIRE (std::string (out) == "n=2|a|b");
  satie_plugin_context_destroy (c);
}

TEST_CASE ("C plugin rejects duplicates") {
  SatieCPluginContext *c = satie_plugin_context_create ();
  REQUIRE (satie_plugin_register (c, "echo", test_echo_fn, nullptr) == 0);
  REQUIRE (satie_plugin_register (c, "echo", test_echo_fn, nullptr) != 0);
  satie_plugin_context_destroy (c);
}

TEST_CASE ("C plugin reports unknown command") {
  SatieCPluginContext *c = satie_plugin_context_create ();
  char out[64];
  REQUIRE (satie_plugin_invoke (c, "missing", nullptr, 0, out, sizeof (out)) != 0);
  REQUIRE (satie_plugin_has (c, "missing") == 0);
  satie_plugin_context_destroy (c);
}

TEST_CASE ("C plugin detects small buffer") {
  SatieCPluginContext *c = satie_plugin_context_create ();
  REQUIRE (satie_plugin_register (c, "echo", test_echo_fn, nullptr) == 0);
  char out[2];
  REQUIRE (satie_plugin_invoke (c, "echo", nullptr, 0, out, sizeof (out)) != 0);
  satie_plugin_context_destroy (c);
}

TEST_CASE ("C plugin validates arguments") {
  SatieCPluginContext *c = satie_plugin_context_create ();
  REQUIRE (satie_plugin_register (c, "x", nullptr, nullptr) != 0);
  REQUIRE (satie_plugin_register (nullptr, "x", test_echo_fn, nullptr) != 0);
  REQUIRE (satie_plugin_invoke (nullptr, "x", nullptr, 0, nullptr, 0) != 0);
  REQUIRE (satie_plugin_last_error () != nullptr);
  satie_plugin_context_destroy (c);
}

TEST_CASE ("SMT logic names are stable") {
  REQUIRE (std::string (satie::smt::smt_logic_name (satie::smt::Logic::QF_UF)) == "QF_UF");
  REQUIRE (std::string (satie::smt::smt_logic_name (satie::smt::Logic::QF_LIA)) == "QF_LIA");
  REQUIRE (std::string (satie::smt::smt_logic_name (satie::smt::Logic::QF_BV)) == "QF_BV");
  REQUIRE (std::string (satie::smt::smt_logic_name (satie::smt::Logic::ALL)) == "ALL");
}

TEST_CASE ("SMT logic FP and NRA names") {
  REQUIRE (std::string (satie::smt::smt_logic_name (satie::smt::Logic::QF_FP)) == "QF_FP");
  REQUIRE (std::string (satie::smt::smt_logic_name (satie::smt::Logic::QF_NRA)) == "QF_NRA");
}

TEST_CASE ("component versions are non-empty") {
  REQUIRE (satie::cdcl_component_version () != nullptr);
  REQUIRE (satie::dpll_component_version () != nullptr);
  REQUIRE (satie::native_component_version () != nullptr);
  REQUIRE (satie::sat_component_version () != nullptr);
  REQUIRE (satie::smt::smt_component_version () != nullptr);
}

TEST_CASE ("SMT solver honors logic selection") {
  satie::smt::SMTSolver s (satie::smt::Logic::QF_BV);
  REQUIRE (s.logic () == satie::smt::Logic::QF_BV);
  s.set_logic (satie::smt::Logic::QF_LIA);
  REQUIRE (s.logic () == satie::smt::Logic::QF_LIA);
  REQUIRE (s.check ().satisfiable ());
}

TEST_CASE ("CDCL unit learns are counted") {
  satie::CDCLSolver solver (satie::CNF ({{ 1, 2 }, { 1, -2 }, { -1, 2 }, { -1, -2 }}));
  REQUIRE (solver.solve ().unsatisfiable ());
  REQUIRE (solver.statistics ().learned_clauses > 0);
}

TEST_CASE ("CDCL stats reset across solves") {
  satie::CDCLSolver solver (satie::CNF ({{ 1 }, { -1 }}));
  REQUIRE (solver.solve ().unsatisfiable ());
  REQUIRE (solver.statistics ().conflicts == 1);
  REQUIRE (solver.solve ().unsatisfiable ());
  REQUIRE (solver.statistics ().conflicts == 1);
}

TEST_CASE ("DPLL stats reset across solves") {
  satie::DPLLSolver solver (satie::CNF ({{ 1 }, { -1 }}));
  REQUIRE (solver.solve ().unsatisfiable ());
  auto first = solver.statistics ().recursive_calls;
  REQUIRE (solver.solve ().unsatisfiable ());
  REQUIRE (solver.statistics ().recursive_calls == first);
}

TEST_CASE ("native stats reset across solves") {
  satie::NaiveSolver solver (satie::CNF ({{ 1 }, { -1 }}));
  REQUIRE (solver.solve ().unsatisfiable ());
  auto first = solver.statistics ().recursive_calls;
  REQUIRE (solver.solve ().unsatisfiable ());
  REQUIRE (solver.statistics ().recursive_calls == first);
}

TEST_CASE ("empty CNF is SAT in all engines") {
  for (satie::Engine e : { satie::Engine::Native, satie::Engine::DPLL, satie::Engine::CDCL })
    REQUIRE (satie::solve (satie::CNF{}, e).satisfiable ());
}

TEST_CASE ("empty clause is UNSAT in all engines") {
  satie::CNF cnf ({{}});
  for (satie::Engine e : { satie::Engine::Native, satie::Engine::DPLL, satie::Engine::CDCL })
    REQUIRE (satie::solve (cnf, e).unsatisfiable ());
}

TEST_CASE ("SAT never returns a violating model") {
  satie::CNF cnf ({{ 1, 2 }, { -1, 2 }});
  for (satie::Engine e : { satie::Engine::Native, satie::Engine::DPLL, satie::Engine::CDCL }) {
    auto r = satie::solve (cnf, e);
    REQUIRE (r.satisfiable ());
    REQUIRE (satie::is_formula_satisfied (cnf, r.assignment));
  }
}

TEST_CASE ("DIMACS with comments parses") {
  satie::CNF cnf = satie::parse ("c hi\np cnf 1 1\n1 0\n", satie::ParseFormat::DIMACS);
  REQUIRE (cnf.clause_count () == 1);
}

TEST_CASE ("DIMACS percent terminator ends input") {
  satie::CNF cnf = satie::parse ("p cnf 1 1\n1 0\n%\n", satie::ParseFormat::DIMACS);
  REQUIRE (cnf.clause_count () == 1);
}

TEST_CASE ("DIMACS missing terminator throws") {
  bool threw = false;
  try {
    satie::parse ("p cnf 1 1\n1\n", satie::ParseFormat::DIMACS);
  } catch (const satie::ParseError &) {
    threw = true;
  }
  REQUIRE (threw);
}
