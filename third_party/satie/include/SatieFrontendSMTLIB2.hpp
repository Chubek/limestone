#pragma once

#include <cctype>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Common.hpp"
#include "Satie.hpp"
#include "SatieCDCL.hpp"
#include "SatieFrontendCNF.hpp"
#include "SatieFrontendCommon.hpp"
#include "SatieLIA.hpp"

namespace satie::frontend
{

/// SMT-LIB2 subset frontend: propositional logic plus conjunctive linear
/// integer arithmetic.
///
/// Commands: `(set-logic QF_LIA|QF_UF|QF_LRA?...)` (accepted when the script
/// stays inside the supported fragments; anything else is a parse error),
/// `(declare-const name Bool|Int)`, `(assert term)`, `(check-sat)`,
/// `(get-model)` (after a SAT check), `(exit)`, plus ignored
/// `(set-option ...)`/`(set-info ...)`. Boolean terms use
/// `true/false/and/or/not/=>/= /distinct`; integer terms use numerals,
/// `+ - *` (linear only), and comparisons `= <= < > >=`.
///
/// Solving routes: purely Boolean scripts go through Tseitin+CDCL (full);
/// conjunctions of linear integer comparisons go through the LIA solver
/// (sound, bounded-complete); anything mixing Boolean structure with theory
/// atoms answers UNKNOWN honestly.
using SMTSExpr = SExpression;

class SMTLexer
{
public:
  explicit SMTLexer (const std::string &text) : text_ (text) {}

  std::vector<std::string> tokenize (std::size_t line_base = 1)
  {
    std::vector<std::string> tokens;
    std::size_t i = 0;
    line_ = line_base;
    while (i < text_.size ())
      {
        const char c = text_[i];
        if (std::isspace (static_cast<unsigned char> (c)) != 0)
          {
            if (c == '\n')
              ++line_;
            ++i;
            continue;
          }
        if (c == ';')
          {
            while (i < text_.size () && text_[i] != '\n')
              ++i;
            continue;
          }
        if (c == '(' || c == ')')
          {
            tokens.emplace_back (1, c);
            ++i;
            continue;
          }
        if (c == '"')
          throw ParseError (line_, 1, "string literals are not supported");
        std::size_t start = i;
        while (i < text_.size () && !std::isspace (static_cast<unsigned char> (text_[i])) &&
               text_[i] != '(' && text_[i] != ')' && text_[i] != ';')
          ++i;
        tokens.push_back (text_.substr (start, i - start));
      }
    return tokens;
  }

  std::size_t line () const { return line_; }

private:
  std::string text_;
  std::size_t line_ = 1;
};

inline SMTSExpr parse_sexpr (const std::vector<std::string> &tokens, std::size_t &pos,
                             std::size_t line)
{
  return parse_expression_tokens (tokens, pos, line);
}

struct SMTModel
{
  std::map<std::string, bool> bools;
  std::map<std::string, std::int64_t> ints;
};

struct SMTCheckResult
{
  SolveStatus status = SolveStatus::UNKNOWN;
  SMTModel model;
};

class SMTLIB2Script
{
public:
  void load_text (const std::string &text)
  {
    for (const SMTSExpr &command : parse_sexpressions (text))
      {
        run_command (command);
        if (exited_) break;
      }
  }

  bool has_check_sat () const { return saw_check_sat_; }
  SMTCheckResult last_result () const { return last_; }

private:
  void require_list (const SMTSExpr &expr, std::size_t min_size, const std::string &what)
  {
    if (!expr.is_list || expr.children.size () < min_size || expr.children[0].is_list)
      throw ParseError (1, 1, std::string ("malformed ") + what);
  }

  static std::string head (const SMTSExpr &expr) { return expr.children[0].atom; }

  void run_command (const SMTSExpr &command)
  {
    require_list (command, 1, "command");
    const std::string name = head (command);
    if (name == "set-logic")
      {
        require_list (command, 2, "set-logic");
        const std::string logic = command.children[1].atom;
        if (logic != "QF_LIA" && logic != "QF_UF" && logic != "QF_LRA" &&
            logic != "ALL")
          throw ParseError (1, 1, "unsupported logic '" + logic + "'");
        logic_ = logic;
      }
    else if (name == "declare-const")
      {
        require_list (command, 3, "declare-const");
        const std::string var = command.children[1].atom;
        const std::string sort = command.children[2].atom;
        if (sort != "Bool" && sort != "Int")
          throw ParseError (1, 1, "only Bool and Int sorts are supported");
        if (sorts_.count (var) != 0)
          throw ParseError (1, 1, "duplicate declaration '" + var + "'");
        sorts_.emplace (var, sort);
      }
    else if (name == "assert")
      {
        require_list (command, 2, "assert");
        asserts_.push_back (command.children[1]);
        last_ = {};
      }
    else if (name == "check-sat")
      {
        saw_check_sat_ = true;
        last_ = check_sat ();
      }
    else if (name == "get-model")
      {
        if (last_.status != SolveStatus::SAT)
          throw ParseError (1, 1, "get-model requires a preceding sat answer");
      }
    else if (name == "exit")
      {
        exited_ = true;
      }
    else if (name == "set-option" || name == "set-info")
      {
      }
    else
      throw ParseError (1, 1, "unsupported command '" + name + "'");
  }

  static bool is_bool_op (const std::string &op)
  {
    return op == "and" || op == "or" || op == "not" || op == "=>" || op == "=" ||
           op == "distinct";
  }

  /// Collects free variables of a Boolean term (atoms that are not
  /// numerals/true/false and not operators).
  void collect_bool_vars (const SMTSExpr &term, std::set<std::string> &out) const
  {
    if (!term.is_list)
      {
        if (term.atom != "true" && term.atom != "false" && !is_numeral (term.atom))
          out.insert (term.atom);
        return;
      }
    for (const SMTSExpr &child : term.children)
      collect_bool_vars (child, out);
  }

  static bool is_numeral (const std::string &atom)
  {
    if (atom.empty ())
      return false;
    std::size_t i = atom[0] == '-' ? 1 : 0;
    if (i >= atom.size ())
      return false;
    for (; i < atom.size (); ++i)
      if (std::isdigit (static_cast<unsigned char> (atom[i])) == 0)
        return false;
    return true;
  }

  /// Translates a Boolean term to the formula DSL understood by
  /// `parse_bool_formula` (` distinct a b` becomes pairwise `~(... <-> ...)`).
  std::string bool_to_dsl (const SMTSExpr &term) const
  {
    if (!term.is_list)
      {
        if (term.atom == "true" || term.atom == "false")
          return term.atom;
        return term.atom;
      }
    if (term.children.empty () || term.children[0].is_list)
      throw ParseError (1, 1, "malformed Boolean term");
    const std::string op = term.children[0].atom;
    std::vector<std::string> args;
    for (std::size_t i = 1; i < term.children.size (); ++i)
      args.push_back (bool_to_dsl (term.children[i]));
    if (op == "and")
      return join_args (" & ", args, true);
    if (op == "or")
      return args.empty () ? "false" : join_args (" | ", args, true);
    if (op == "not" && args.size () == 1)
      return "(~" + args.front () + ")";
    if (op == "=>" && args.size () == 2)
      return "(" + args[0] + " -> " + args[1] + ")";
    if (op == "=" && args.size () == 2)
      return "(" + args[0] + " <-> " + args[1] + ")";
    if (op == "distinct")
      {
        // Pairwise disequality.
        std::vector<std::string> parts;
        for (std::size_t i = 0; i < args.size (); ++i)
          for (std::size_t j = i + 1; j < args.size (); ++j)
            parts.push_back ("(~(" + args[i] + " <-> " + args[j] + "))");
        return join_args (" & ", parts, true);
      }
    throw ParseError (1, 1, "unsupported Boolean operator '" + op + "'");
  }

  static std::string join_args (const std::string &sep, const std::vector<std::string> &args,
                                bool parens)
  {
    if (args.empty ())
      return "true";
    if (args.size () == 1 && (sep == " & " || sep == " | "))
      return args.front ();
    std::string out = parens ? "(" : "";
    for (std::size_t i = 0; i < args.size (); ++i)
      {
        if (i != 0)
          out += sep;
        out += args[i];
      }
    if (parens)
      out += ")";
    return out;
  }

  /// True when every assert is a conjunction (via nested `and`) of linear
  /// integer comparisons over declared Int constants.
  bool is_lia_conjunction () const
  {
    if (asserts_.empty ())
      return false;
    for (const SMTSExpr &assertion : asserts_)
      if (!is_lia_formula (assertion))
        return false;
    return true;
  }

  bool is_lia_formula (const SMTSExpr &term) const
  {
    if (!term.is_list || term.children.empty () || term.children[0].is_list)
      {
        // Bare Int comparisons only appear as lists; anything else is not
        // part of the LIA fragment (Bool atoms handled elsewhere).
        return false;
      }
    const std::string op = term.children[0].atom;
    if (op == "and")
      {
        for (std::size_t i = 1; i < term.children.size (); ++i)
          if (!is_lia_formula (term.children[i]))
            return false;
        return true;
      }
    return op == "=" || op == "<=" || op == "<" || op == ">" || op == ">=";
  }

  bool uses_int_sort () const
  {
    for (const auto &[var, sort] : sorts_)
      if (sort == "Int")
        return true;
    return false;
  }

  SMTCheckResult check_sat ()
  {
    SMTCheckResult result;
    if (!uses_int_sort ())
      return check_boolean ();
    if (is_lia_conjunction ())
      return check_lia ();
    return result; // Mixed fragments: honest UNKNOWN.
  }

  SMTCheckResult check_boolean ()
  {
    SMTCheckResult result;
    // Every declared variable must be Bool here; undeclared atoms are
    // treated as free Booleans.
    std::string combined = "true";
    for (const SMTSExpr &assertion : asserts_)
      combined += " & (" + bool_to_dsl (assertion) + ")";
    BoolFormula formula;
    try
      {
        formula = parse_bool_formula (combined);
      }
    catch (const ParseError &)
      {
        return result;
      }
    SolveResult solved = solve (formula.cnf, Engine::CDCL);
    if (solved.unsatisfiable ())
      {
        result.status = SolveStatus::UNSAT;
        return result;
      }
    result.status = SolveStatus::SAT;
    for (const auto &[var, sort] : sorts_)
      {
        if (sort != "Bool")
          continue;
        auto id = formula.symbols.lookup (var);
        if (id.has_value ())
          result.model.bools.emplace (
              var, solved.assignment.get_var (*id) == Value::TRUE);
        else
          result.model.bools.emplace (var, false);
      }
    return result;
  }

  struct LinExpr
  {
    std::map<int, std::int64_t> coeffs; // lia var id -> coefficient
    std::int64_t constant = 0;
  };

  SMTCheckResult check_lia ()
  {
    SMTCheckResult result;
    lia::LIASolver solver;
    std::map<std::string, int> ids;
    for (const auto &[var, sort] : sorts_)
      if (sort == "Int")
        ids.emplace (var, solver.add_var (var));
    for (const SMTSExpr &assertion : asserts_)
      {
        std::vector<SMTSExpr> conjuncts;
        flatten_and (assertion, conjuncts);
        for (const SMTSExpr &atom : conjuncts)
          if (!add_lia_atom (solver, ids, atom))
            return result;
      }
    SolveResult solved = solver.check ();
    if (solved.unsatisfiable ())
      {
        result.status = SolveStatus::UNSAT;
        return result;
      }
    if (!solved.satisfiable ())
      return result;
    result.status = SolveStatus::SAT;
    for (const auto &[var, id] : ids)
      result.model.ints.emplace (var, solver.value (id));
    return result;
  }

  static void flatten_and (const SMTSExpr &term, std::vector<SMTSExpr> &out)
  {
    if (term.is_list && !term.children.empty () && !term.children[0].is_list &&
        term.children[0].atom == "and")
      {
        for (std::size_t i = 1; i < term.children.size (); ++i)
          flatten_and (term.children[i], out);
        return;
      }
    out.push_back (term);
  }

  bool add_lia_atom (lia::LIASolver &solver, const std::map<std::string, int> &ids,
                     const SMTSExpr &atom)
  {
    if (!atom.is_list || atom.children.size () != 3 || atom.children[0].is_list)
      return false;
    const std::string op = atom.children[0].atom;
    LinExpr lhs, rhs;
    if (!linear_expr (atom.children[1], ids, lhs) ||
        !linear_expr (atom.children[2], ids, rhs))
      return false;
    // lhs - rhs op 0.
    std::vector<std::pair<int, std::int64_t>> coeffs;
    for (const auto &[var, coeff] : lhs.coeffs)
      coeffs.push_back ({ var, coeff });
    for (const auto &[var, coeff] : rhs.coeffs)
      coeffs.push_back ({ var, -coeff });
    const std::int64_t bound = rhs.constant - lhs.constant;
    if (op == "=")
      solver.add_eq (coeffs, bound);
    else if (op == "<=")
      solver.add_le (coeffs, bound);
    else if (op == "<")
      solver.add_le (coeffs, bound - 1);
    else if (op == ">=")
      {
        for (auto &[var, coeff] : coeffs)
          coeff = -coeff;
        solver.add_le (coeffs, -bound);
      }
    else if (op == ">")
      {
        for (auto &[var, coeff] : coeffs)
          coeff = -coeff;
        solver.add_le (coeffs, -bound - 1);
      }
    else
      return false;
    return true;
  }

  bool linear_expr (const SMTSExpr &term, const std::map<std::string, int> &ids,
                    LinExpr &out) const
  {
    if (!term.is_list)
      {
        if (is_numeral (term.atom))
          {
            out.constant += std::stoll (term.atom);
            return true;
          }
        auto it = ids.find (term.atom);
        if (it == ids.end ())
          return false;
        out.coeffs[it->second] += 1;
        return true;
      }
    if (term.children.empty () || term.children[0].is_list)
      return false;
    const std::string op = term.children[0].atom;
    if (op == "+" || op == "-")
      {
        for (std::size_t i = 1; i < term.children.size (); ++i)
          {
            LinExpr part;
            if (!linear_expr (term.children[i], ids, part))
              return false;
            const bool negate = op == "-" && i > 1;
            const bool first_neg = op == "-" && term.children.size () == 2;
            if (negate || first_neg)
              {
                for (const auto &[var, coeff] : part.coeffs)
                  out.coeffs[var] -= coeff;
                out.constant -= part.constant;
              }
            else
              {
                for (const auto &[var, coeff] : part.coeffs)
                  out.coeffs[var] += coeff;
                out.constant += part.constant;
              }
          }
        return true;
      }
    if (op == "*" && term.children.size () == 3)
      {
        LinExpr a, b;
        if (!linear_expr (term.children[1], ids, a) ||
            !linear_expr (term.children[2], ids, b))
          return false;
        const bool a_const = a.coeffs.empty ();
        const bool b_const = b.coeffs.empty ();
        if (a_const && b_const)
          {
            out.constant += a.constant * b.constant;
            return true;
          }
        if (a_const && !b_const)
          {
            for (const auto &[var, coeff] : b.coeffs)
              out.coeffs[var] += coeff * a.constant;
            out.constant += b.constant * a.constant;
            return true;
          }
        if (b_const && !a_const)
          {
            for (const auto &[var, coeff] : a.coeffs)
              out.coeffs[var] += coeff * b.constant;
            out.constant += a.constant * b.constant;
            return true;
          }
        return false; // Nonlinear.
      }
    return false;
  }

  std::string logic_ = "ALL";
  std::map<std::string, std::string> sorts_;
  std::vector<SMTSExpr> asserts_;
  bool saw_check_sat_ = false;
  bool exited_ = false;
  SMTCheckResult last_;
};

inline SMTCheckResult solve_smtlib2_text (const std::string &text)
{
  SMTLIB2Script script;
  script.load_text (text);
  if (!script.has_check_sat ())
    throw ParseError (1, 1, "SMT-LIB2 script has no (check-sat)");
  return script.last_result ();
}

/// Version anchor defined in src/SatieFrontendSMTLIB2.cpp.
const char *frontend_smtlib2_component_version () noexcept;

} // namespace satie::frontend
