/// Frontend tour: exercises every language frontend through one binary.
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "Satie.hpp"
#include "SatieFrontendCNF.hpp"
#include "SatieFrontendIPASIR.hpp"
#include "SatieFrontendOPB.hpp"
#include "SatieFrontendSMTLIB2.hpp"
#include "SatieFrontendSatieLisp.hpp"
#include "SatieFrontendWCNF.hpp"

namespace
{
std::string read_file (const std::string &path)
{
  std::ifstream in (path);
  if (!in)
    throw std::runtime_error ("cannot open " + path);
  std::ostringstream buffer;
  buffer << in.rdbuf ();
  return buffer.str ();
}
} // namespace

int main (int argc, char **argv)
{
  const std::string dir = argc > 1 ? argv[1] : ".";
  using namespace satie::frontend;

  BoolFormula boolean = parse_bool_formula ("(a | ~b) & (b -> c)");
  std::cout << "bool: " << boolean.cnf.clause_count () << " clauses, "
            << (satie::solve (boolean.cnf).satisfiable () ? "SAT" : "UNSAT") << "\n";

  WeightedCNF wcnf = parse_wcnf_text (read_file (dir + "/sample.wcnf"));
  MaxSATResult maxsat = solve_maxsat (wcnf);
  std::cout << "wcnf: optimum cost " << maxsat.cost << "\n";
  assert (maxsat.status == MaxSATStatus::Optimal && maxsat.cost == 1);

  PBProblem pb = parse_opb_text (read_file (dir + "/sample.opb"));
  PBOptimum optimum = optimize_pb_min (pb);
  std::cout << "opb: minimum " << optimum.value << "\n";
  assert (optimum.status == satie::SolveStatus::SAT && optimum.value == 1);

  SMTCheckResult smt = solve_smtlib2_text (read_file (dir + "/sample.smt2"));
  std::cout << "smt2: x = " << smt.model.ints["x"] << "\n";
  assert (smt.status == satie::SolveStatus::SAT);

  LispResult lisp = solve_satielisp (read_file (dir + "/sample.lisp"));
  std::cout << "lisp: a = " << (lisp.model["a"] ? "true" : "false") << "\n";
  assert (lisp.status == satie::SolveStatus::SAT && lisp.model["a"]);

  IpasirSolver ipasir;
  ipasir.add (1);
  ipasir.add (0);
  ipasir.assume (-1);
  std::cout << "ipasir: " << ipasir.solve () << " (want 20)\n";
  assert (ipasir.solve () == 20);

  std::cout << "frontend-demo-ok\n";
  return 0;
}
