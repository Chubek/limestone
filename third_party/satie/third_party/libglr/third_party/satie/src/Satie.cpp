#include "Satie.hpp"

#include <cstdlib>
#include <sstream>
#include <stdexcept>

namespace satie
{

static bool looks_like_dimacs (const std::string &text)
{
  std::istringstream in (text);
  std::string line;
  while (std::getline (in, line))
    {
      auto first = line.find_first_not_of (" \t\r\n");
      if (first == std::string::npos)
        continue;
      char lead = line[first];
      if (lead == 'c')
        continue;
      if (lead == 'p')
        return true;
      if (lead == 'p')
        return true;
      return false;
    }
  return false;
}

static CNF parse_dimacs_text (const std::string &text)
{
  std::istringstream in (text);
  return parse_dimacs (in);
}

SolveResult Solver::solve (Engine engine) const
{
  switch (engine)
    {
    case Engine::Native:
      return solve_naive (cnf_);
    case Engine::DPLL:
      return solve_dpll (cnf_);
    case Engine::CDCL:
      return solve_cdcl (cnf_);
    }
  std::abort ();
}

SolverReport Solver::solve_with_report (const SolveOptions &options) const
{
  SolverReport report{};
  report.engine = options.engine;

  switch (options.engine)
    {
    case Engine::Native:
      {
        NaiveSolver solver (cnf_);
        report.result = solver.solve ();
        report.statistics.native = solver.statistics ();
        break;
      }
    case Engine::DPLL:
      {
        DPLLSolver solver (cnf_);
        report.result = solver.solve ();
        report.statistics.dpll = solver.statistics ();
        break;
      }
    case Engine::CDCL:
      {
        CDCLSolver solver (cnf_);
        report.result = solver.solve ();
        report.statistics.cdcl = solver.statistics ();
        break;
      }
    }

  return report;
}

bool Solver::satisfiable (Engine engine) const { return solve (engine).satisfiable (); }

CNF parse_auto (const std::string &text)
{
  return parse (text, ParseFormat::Auto);
}

CNF parse_auto_file (const std::string &path)
{
  return parse_file (path, ParseFormat::Auto);
}

CNF parse (const std::string &text, ParseFormat format)
{
  switch (format)
    {
    case ParseFormat::Auto:
      return looks_like_dimacs (text) ? parse_dimacs_text (text) : parse_cnf (text);
    case ParseFormat::CNF:
      return parse_cnf (text);
    case ParseFormat::DIMACS:
      return parse_dimacs_text (text);
    }
  std::abort ();
}

CNF parse_file (const std::string &path, ParseFormat format)
{
  std::ifstream in (path);
  if (!in)
    throw std::runtime_error ("failed to open input file: " + path);

  std::ostringstream buffer;
  buffer << in.rdbuf ();
  return parse (buffer.str (), format);
}

SolveResult solve (const CNF &cnf, Engine engine)
{
  Solver solver (cnf);
  return solver.solve (engine);
}

SolveResult solve (const std::string &input, Engine engine)
{
  return solve (parse_auto (input), engine);
}

SolverReport solve_with_report (const CNF &cnf, const SolveOptions &options)
{
  Solver solver (cnf);
  return solver.solve_with_report (options);
}

} // namespace satie

#include "Satie.h"

namespace
{
thread_local std::string g_satie_error;

void set_satie_error (const std::string &message) { g_satie_error = message; }

satie::Engine c_engine_to_cpp (int engine)
{
  switch (engine)
    {
    case SATIE_C_NATIVE:
      return satie::Engine::Native;
    case SATIE_C_DPLL:
      return satie::Engine::DPLL;
    case SATIE_C_CDCL:
    default:
      return satie::Engine::CDCL;
    }
}

int satie_status_to_c (satie::SolveStatus status)
{
  switch (status)
    {
    case satie::SolveStatus::SAT:
      return SATIE_C_SAT;
    case satie::SolveStatus::UNSAT:
      return SATIE_C_UNSAT;
    default:
      return SATIE_C_UNKNOWN;
    }
}
} // namespace

struct SatieSolver
{
  satie::CNF problem;
  satie::SolveResult last{};
  bool solved = false;
};

extern "C"
{

const char *satie_version (void) { return satie::satie_library_version (); }
const char *satie_build_info (void)
{
  thread_local std::string cached;
  cached = satie::satie_library_build_info ();
  return cached.c_str ();
}
const char *satie_last_error (void) { return g_satie_error.c_str (); }

SatieSolver *satie_solver_create (void)
{
  try
    {
      return new SatieSolver{};
    }
  catch (const std::exception &e)
    {
      set_satie_error (e.what ());
      return nullptr;
    }
}

void satie_solver_destroy (SatieSolver *solver) { delete solver; }

void satie_solver_reset (SatieSolver *solver)
{
  if (solver == nullptr)
    return;
  solver->problem = satie::CNF{};
  solver->last = satie::SolveResult{};
  solver->solved = false;
}

int satie_solver_load_dimacs (SatieSolver *solver, const char *text)
{
  if (solver == nullptr || text == nullptr)
    {
      set_satie_error ("solver load requires a solver and input text");
      return -1;
    }
  try
    {
      solver->problem = satie::parse (text, satie::ParseFormat::DIMACS);
      solver->solved = false;
      return 0;
    }
  catch (const std::exception &e)
    {
      set_satie_error (e.what ());
      return -1;
    }
}

int satie_solver_load_cnf (SatieSolver *solver, const char *text)
{
  if (solver == nullptr || text == nullptr)
    {
      set_satie_error ("solver load requires a solver and input text");
      return -1;
    }
  try
    {
      solver->problem = satie::parse (text, satie::ParseFormat::CNF);
      solver->solved = false;
      return 0;
    }
  catch (const std::exception &e)
    {
      set_satie_error (e.what ());
      return -1;
    }
}

int satie_solver_add_clause (SatieSolver *solver, const int *literals, size_t count)
{
  if (solver == nullptr)
    {
      set_satie_error ("add_clause requires a solver");
      return -1;
    }
  if (count != 0 && literals == nullptr)
    {
      set_satie_error ("add_clause received a null literal array");
      return -1;
    }
  try
    {
      satie::Clause clause;
      clause.reserve (count);
      for (size_t i = 0; i < count; ++i)
        {
          if (literals[i] == 0)
            {
              set_satie_error ("zero is a DIMACS terminator, not a literal");
              return -1;
            }
          clause.push_back (literals[i]);
        }
      solver->problem.add_clause (std::move (clause));
      solver->solved = false;
      return 0;
    }
  catch (const std::exception &e)
    {
      set_satie_error (e.what ());
      return -1;
    }
}

int satie_solver_solve (SatieSolver *solver, int engine)
{
  if (solver == nullptr)
    {
      set_satie_error ("solve requires a solver");
      return SATIE_C_UNKNOWN;
    }
  try
    {
      satie::Solver cpp_solver (solver->problem);
      solver->last = cpp_solver.solve (c_engine_to_cpp (engine));
      solver->solved = true;
      return satie_status_to_c (solver->last.status);
    }
  catch (const std::exception &e)
    {
      set_satie_error (e.what ());
      return SATIE_C_UNKNOWN;
    }
}

int satie_solver_status (const SatieSolver *solver)
{
  if (solver == nullptr || !solver->solved)
    return SATIE_C_UNKNOWN;
  return satie_status_to_c (solver->last.status);
}

size_t satie_solver_variable_count (const SatieSolver *solver)
{
  return solver != nullptr ? solver->problem.variable_count () : 0;
}

int satie_solver_variable_value (const SatieSolver *solver, int var)
{
  if (solver == nullptr || !solver->solved || var <= 0)
    return -1;
  if (!solver->last.satisfiable ())
    return -1;
  satie::Value value = solver->last.assignment.get_var (var);
  if (value == satie::Value::TRUE)
    return 1;
  if (value == satie::Value::FALSE)
    return 0;
  return -1;
}

} // extern "C"
