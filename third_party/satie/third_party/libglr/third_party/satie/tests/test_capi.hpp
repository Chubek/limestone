#pragma once

#include "catch_shim.hpp"

#include "Satie.h"
#include "Satie.hpp"

#include <cstring>
#include <string>

TEST_CASE ("C API reports a version string") {
  const char *v = satie_version ();
  REQUIRE (v != nullptr);
  REQUIRE (std::string (v).size () > 0);
}

TEST_CASE ("C API reports build info") {
  const char *b = satie_build_info ();
  REQUIRE (b != nullptr);
  REQUIRE (std::string (b).find ("satie") != std::string::npos);
}

TEST_CASE ("C API last error starts empty") {
  REQUIRE (satie_last_error () != nullptr);
}

TEST_CASE ("C API creates and destroys a solver") {
  SatieSolver *s = satie_solver_create ();
  REQUIRE (s != nullptr);
  satie_solver_destroy (s);
}

TEST_CASE ("C API status is UNKNOWN before solve") {
  SatieSolver *s = satie_solver_create ();
  REQUIRE (satie_solver_status (s) == SATIE_C_UNKNOWN);
  satie_solver_destroy (s);
}

TEST_CASE ("C API status handles NULL solver") {
  REQUIRE (satie_solver_status (nullptr) == SATIE_C_UNKNOWN);
  REQUIRE (satie_solver_variable_count (nullptr) == 0);
  REQUIRE (satie_solver_variable_value (nullptr, 1) == -1);
}

TEST_CASE ("C API solves empty formula as SAT") {
  SatieSolver *s = satie_solver_create ();
  REQUIRE (satie_solver_solve (s, SATIE_C_CDCL) == SATIE_C_SAT);
  satie_solver_destroy (s);
}

TEST_CASE ("C API solves a unit clause") {
  SatieSolver *s = satie_solver_create ();
  REQUIRE (satie_solver_load_cnf (s, "(a)") == 0);
  REQUIRE (satie_solver_solve (s, SATIE_C_CDCL) == SATIE_C_SAT);
  REQUIRE (satie_solver_variable_value (s, 1) == 1);
  satie_solver_destroy (s);
}

TEST_CASE ("C API detects direct contradiction") {
  SatieSolver *s = satie_solver_create ();
  const int c1[] = { 1 };
  const int c2[] = { -1 };
  REQUIRE (satie_solver_add_clause (s, c1, 1) == 0);
  REQUIRE (satie_solver_add_clause (s, c2, 1) == 0);
  REQUIRE (satie_solver_solve (s, SATIE_C_CDCL) == SATIE_C_UNSAT);
  REQUIRE (satie_solver_status (s) == SATIE_C_UNSAT);
  satie_solver_destroy (s);
}

TEST_CASE ("C API solves empty clause as UNSAT") {
  SatieSolver *s = satie_solver_create ();
  REQUIRE (satie_solver_add_clause (s, nullptr, 0) == 0);
  REQUIRE (satie_solver_solve (s, SATIE_C_CDCL) == SATIE_C_UNSAT);
  satie_solver_destroy (s);
}

TEST_CASE ("C API rejects zero literal") {
  SatieSolver *s = satie_solver_create ();
  const int bad[] = { 0 };
  REQUIRE (satie_solver_add_clause (s, bad, 1) != 0);
  REQUIRE (std::string (satie_last_error ()).size () > 0);
  satie_solver_destroy (s);
}

TEST_CASE ("C API rejects NULL text") {
  SatieSolver *s = satie_solver_create ();
  REQUIRE (satie_solver_load_cnf (s, nullptr) != 0);
  REQUIRE (satie_solver_load_dimacs (s, nullptr) != 0);
  satie_solver_destroy (s);
}

TEST_CASE ("C API rejects NULL solver") {
  REQUIRE (satie_solver_load_cnf (nullptr, "(a)") != 0);
  REQUIRE (satie_solver_add_clause (nullptr, nullptr, 0) != 0);
  REQUIRE (satie_solver_solve (nullptr, SATIE_C_CDCL) == SATIE_C_UNKNOWN);
  satie_solver_reset (nullptr);
  satie_solver_destroy (nullptr);
}

TEST_CASE ("C API parses DIMACS input") {
  SatieSolver *s = satie_solver_create ();
  REQUIRE (satie_solver_load_dimacs (s, "p cnf 2 2\n1 -2 0\n2 0\n") == 0);
  REQUIRE (satie_solver_solve (s, SATIE_C_CDCL) == SATIE_C_SAT);
  REQUIRE (satie_solver_variable_count (s) == 2);
  satie_solver_destroy (s);
}

TEST_CASE ("C API reports DIMACS parse errors") {
  SatieSolver *s = satie_solver_create ();
  REQUIRE (satie_solver_load_dimacs (s, "p cnf 1 1\n1\n") != 0);
  satie_solver_destroy (s);
}

TEST_CASE ("C API native engine agrees on SAT") {
  SatieSolver *s = satie_solver_create ();
  REQUIRE (satie_solver_load_cnf (s, "(a | b) & (~a | b)") == 0);
  REQUIRE (satie_solver_solve (s, SATIE_C_NATIVE) == SATIE_C_SAT);
  satie_solver_destroy (s);
}

TEST_CASE ("C API dpll engine agrees on UNSAT") {
  SatieSolver *s = satie_solver_create ();
  const int c1[] = { 1 };
  const int c2[] = { -1 };
  satie_solver_add_clause (s, c1, 1);
  satie_solver_add_clause (s, c2, 1);
  REQUIRE (satie_solver_solve (s, SATIE_C_DPLL) == SATIE_C_UNSAT);
  satie_solver_destroy (s);
}

TEST_CASE ("C API model values are consistent") {
  SatieSolver *s = satie_solver_create ();
  REQUIRE (satie_solver_load_cnf (s, "(a | b) & (~a | b)") == 0);
  REQUIRE (satie_solver_solve (s, SATIE_C_CDCL) == SATIE_C_SAT);
  int a = satie_solver_variable_value (s, 1);
  int b = satie_solver_variable_value (s, 2);
  REQUIRE ((a == 0 || a == 1));
  REQUIRE (b == 1);
  satie_solver_destroy (s);
}

TEST_CASE ("C API unknown var has no model value") {
  SatieSolver *s = satie_solver_create ();
  REQUIRE (satie_solver_solve (s, SATIE_C_CDCL) == SATIE_C_SAT);
  REQUIRE (satie_solver_variable_value (s, 99) == -1);
  REQUIRE (satie_solver_variable_value (s, -1) == -1);
  satie_solver_destroy (s);
}

TEST_CASE ("C API reset clears problem and result") {
  SatieSolver *s = satie_solver_create ();
  const int c1[] = { 1 };
  satie_solver_add_clause (s, c1, 1);
  REQUIRE (satie_solver_solve (s, SATIE_C_CDCL) == SATIE_C_SAT);
  satie_solver_reset (s);
  REQUIRE (satie_solver_status (s) == SATIE_C_UNKNOWN);
  REQUIRE (satie_solver_variable_count (s) == 0);
  REQUIRE (satie_solver_solve (s, SATIE_C_CDCL) == SATIE_C_SAT);
  satie_solver_destroy (s);
}

TEST_CASE ("C API tautological clause stays SAT") {
  SatieSolver *s = satie_solver_create ();
  const int taut[] = { 1, -1 };
  const int unit[] = { 2 };
  REQUIRE (satie_solver_add_clause (s, taut, 2) == 0);
  REQUIRE (satie_solver_add_clause (s, unit, 1) == 0);
  REQUIRE (satie_solver_solve (s, SATIE_C_CDCL) == SATIE_C_SAT);
  satie_solver_destroy (s);
}

TEST_CASE ("C API duplicate literals stay SAT") {
  SatieSolver *s = satie_solver_create ();
  const int dup[] = { 1, 1 };
  REQUIRE (satie_solver_add_clause (s, dup, 2) == 0);
  REQUIRE (satie_solver_solve (s, SATIE_C_CDCL) == SATIE_C_SAT);
  REQUIRE (satie_solver_variable_value (s, 1) == 1);
  satie_solver_destroy (s);
}

TEST_CASE ("C API branching formula") {
  SatieSolver *s = satie_solver_create ();
  REQUIRE (satie_solver_load_cnf (s, "(a | b) & (~a | ~b) & (a | ~b)") == 0);
  int status = satie_solver_solve (s, SATIE_C_CDCL);
  REQUIRE (status == SATIE_C_SAT);
  satie_solver_destroy (s);
}

TEST_CASE ("C++ version matches C version") {
  REQUIRE (std::string (satie::satie_library_version ()) == std::string (satie_version ()));
  REQUIRE (satie::satie_library_build_info ().find ("satie") != std::string::npos);
}

TEST_CASE ("C API unknown engine falls back to CDCL") {
  SatieSolver *s = satie_solver_create ();
  REQUIRE (satie_solver_load_cnf (s, "(a)") == 0);
  REQUIRE (satie_solver_solve (s, 999) == SATIE_C_SAT);
  satie_solver_destroy (s);
}
