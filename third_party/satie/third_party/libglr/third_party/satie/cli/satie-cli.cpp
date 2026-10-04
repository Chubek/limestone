/// @file satie-cli.cpp
/// @brief Non-interactive batch driver for Satie.
///
/// Usage:
///   satie-cli [--engine native|dpll|cdcl] [--model] [--format auto|cnf|dimacs|bool|wcnf|opb|smt2|lisp] <file>
///   cat formula.cnf | satie-cli [--engine cdcl] -
///
/// Exits 0 on SAT, 1 on UNSAT, 2 on error. The `wcnf` format runs MaxSAT
/// and prints the optimum cost; `smt2` may additionally print UNKNOWN.

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "Satie.hpp"
#include "SatieFrontendCNF.hpp"
#include "SatieFrontendOPB.hpp"
#include "SatieFrontendSMTLIB2.hpp"
#include "SatieFrontendSatieLisp.hpp"
#include "SatieFrontendWCNF.hpp"

namespace {

void usage() {
  std::cerr <<
    "usage: satie-cli [--engine native|dpll|cdcl] [--model] [--format auto|cnf|dimacs|bool|wcnf|opb|smt2|lisp] <file|->\n"
    "  --engine   solver backend (default: cdcl)\n"
    "  --model    print the satisfying model on SAT\n"
    "  --format   input language (default: auto = CNF DSL vs DIMACS)\n"
    "  -          read formula from stdin\n";
}

std::string readAll(std::istream &in) {
  std::ostringstream buf;
  buf << in.rdbuf();
  return buf.str();
}

int printResult(const satie::SolveResult &r, bool showModel) {
  if (r.status == satie::SolveStatus::SAT) {
    std::cout << "SAT\n";
    if (showModel) {
      for (std::size_t v = 1; v <= r.assignment.size(); ++v) {
        satie::Value val = r.assignment.get_var(static_cast<satie::Var>(v));
        if (val == satie::Value::UNKNOWN) continue;
        std::cout << "x" << v << " = " << satie::value_name(val) << "\n";
      }
    }
    return 0;
  }
  if (r.status == satie::SolveStatus::UNSAT) {
    std::cout << "UNSAT\n";
    return 1;
  }
  std::cout << "UNKNOWN\n";
  return 2;
}

} // namespace

int main(int argc, char **argv) {
  satie::Engine engine = satie::Engine::CDCL;
  bool showModel = false;
  std::string format = "auto";
  std::string path;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--engine") {
      if (++i >= argc) { usage(); return 2; }
      std::string e = argv[i];
      if (e == "native") engine = satie::Engine::Native;
      else if (e == "dpll") engine = satie::Engine::DPLL;
      else if (e == "cdcl") engine = satie::Engine::CDCL;
      else { std::cerr << "unknown engine: " << e << "\n"; return 2; }
    } else if (arg == "--format") {
      if (++i >= argc) { usage(); return 2; }
      format = argv[i];
      if (format != "auto" && format != "cnf" && format != "dimacs" &&
          format != "bool" && format != "wcnf" && format != "opb" &&
          format != "smt2" && format != "lisp") {
        std::cerr << "unknown format: " << format << "\n"; return 2;
      }
    } else if (arg == "--model") {
      showModel = true;
    } else if (arg == "-h" || arg == "--help") {
      usage(); return 0;
    } else if (arg == "-") {
      path = "-";
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "unknown option: " << arg << "\n"; usage(); return 2;
    } else {
      path = arg;
    }
  }

  if (path.empty()) { usage(); return 2; }

  std::string text;
  try {
    if (path == "-") {
      text = readAll(std::cin);
    } else {
      std::ifstream in(path);
      if (!in) { std::cerr << "error: cannot open " << path << "\n"; return 2; }
      text = readAll(in);
    }
    namespace fe = satie::frontend;
    if (format == "wcnf") {
      fe::WeightedCNF wcnf = fe::parse_wcnf_text(text);
      fe::MaxSATResult r = fe::solve_maxsat(wcnf, engine);
      if (r.status == fe::MaxSATStatus::Optimal) {
        std::cout << "SAT\ncost = " << r.cost << "\n";
        if (showModel) {
          for (std::size_t v = 1; v <= r.assignment.size(); ++v) {
            satie::Value val = r.assignment.get_var(static_cast<satie::Var>(v));
            if (val == satie::Value::UNKNOWN) continue;
            std::cout << "x" << v << " = " << satie::value_name(val) << "\n";
          }
        }
        return 0;
      }
      std::cout << "UNSAT\n";
      return 1;
    }
    if (format == "opb") {
      fe::PBProblem pb = fe::parse_opb_text(text);
      return printResult(fe::solve_pb(pb), showModel);
    }
    if (format == "smt2") {
      fe::SMTCheckResult r = fe::solve_smtlib2_text(text);
      satie::SolveResult wrapped{r.status, satie::Assignment{}};
      return printResult(wrapped, false);
    }
    if (format == "lisp") {
      fe::LispResult r = fe::solve_satielisp(text, engine);
      satie::SolveResult wrapped{r.status, satie::Assignment{}};
      int code = printResult(wrapped, false);
      if (r.status == satie::SolveStatus::SAT && showModel) {
        for (const auto &[name, value] : r.model)
          std::cout << name << " = " << (value ? "true" : "false") << "\n";
      }
      return code;
    }
    satie::CNF cnf =
      format == "auto" ? satie::parse_auto(text) :
      format == "dimacs" ? satie::parse(text, satie::ParseFormat::DIMACS) :
      format == "bool" ? fe::parse_bool_formula(text).cnf :
      satie::parse(text, satie::ParseFormat::CNF);
    satie::Solver solver(std::move(cnf));
    return printResult(solver.solve(engine), showModel);
  } catch (const satie::ParseError &e) {
    std::cerr << "parse error: " << e.what() << "\n";
    return 2;
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << "\n";
    return 2;
  }
}
