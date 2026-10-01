#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Common.hpp"
#include "SatieCDCL.hpp"
#include "SatieFrontendCommon.hpp"
#include "SatieTheoryUtils.hpp"

namespace satie::frontend
{

/// OPB (pseudo-Boolean) parsing and solving (the OPB frontend).
///
/// Supports the competition subset: `*` comment lines, an optional
/// `min:`/`max:` objective line, and constraints
/// `coeff*xN [+ ...] (>=|<=|=) rhs ;` with (possibly negative) integer
/// coefficients over `x1..xN`. Constraints are bit-blasted into signed
/// two's-complement arithmetic (width derived from the coefficient/day
/// magnitudes, capped) and solved with CDCL; models are validated by
/// integer evaluation, so SAT answers are exact. Objectives optimize by
/// binary search (`optimize_pb_min`/`optimize_pb_max`); unbounded-looking
/// misses degrade to UNKNOWN rather than wrong optima.
struct PBTerm
{
  long long coeff = 0;
  int var = 0; // 1-based OPB variable number
};

enum class PBCmp
{
  Ge,
  Le,
  Eq
};

struct PBConstraint
{
  std::vector<PBTerm> terms;
  PBCmp cmp = PBCmp::Ge;
  long long rhs = 0;
};

struct PBObjective
{
  bool present = false;
  bool minimize = true;
  std::vector<PBTerm> terms;
};

struct PBProblem
{
  int variables = 0;
  std::vector<PBConstraint> constraints;
  PBObjective objective;
};

namespace pb_detail
{

struct Token
{
  enum class Kind
  {
    End,
    Ident,
    Number,
    Plus,
    Star,
    Colon,
    Semicolon,
    Ge,
    Le,
    Eq
  };
  explicit Token (Kind k = Kind::End) : kind (k) {}
  Kind kind = Kind::End;
  std::string text;
  long long number = 0;
};

class Lexer
{
public:
  explicit Lexer (const std::string &line, std::size_t line_no)
      : line_ (line), line_no_ (line_no)
  {
  }

  Token next ()
  {
    skip_ws ();
    if (pos_ >= line_.size ())
      return Token ();
    const char c = line_[pos_];
    if (c == '+')
      {
        ++pos_;
        return Token (Token::Kind::Plus);
      }
    if (c == '*')
      {
        ++pos_;
        return Token (Token::Kind::Star);
      }
    if (c == ':')
      {
        ++pos_;
        return Token (Token::Kind::Colon);
      }
    if (c == ';')
      {
        ++pos_;
        return Token (Token::Kind::Semicolon);
      }
    if (c == '>' && peek (1) == '=')
      {
        pos_ += 2;
        return Token (Token::Kind::Ge);
      }
    if (c == '<' && peek (1) == '=')
      {
        pos_ += 2;
        return Token (Token::Kind::Le);
      }
    if (c == '=')
      {
        ++pos_;
        return Token (Token::Kind::Eq);
      }
    if (c == '-' || std::isdigit (static_cast<unsigned char> (c)) != 0)
      {
        std::size_t start = pos_;
        if (c == '-')
          ++pos_;
        while (pos_ < line_.size () &&
               std::isdigit (static_cast<unsigned char> (line_[pos_])) != 0)
          ++pos_;
        Token token (Token::Kind::Number);
        token.text = line_.substr (start, pos_ - start);
        try
          {
            token.number = std::stoll (token.text);
          }
        catch (const std::exception &)
          {
            throw ParseError (line_no_, start + 1, "integer literal out of range");
          }
        return token;
      }
    if (std::isalpha (static_cast<unsigned char> (c)) != 0 || c == '_')
      {
        std::size_t start = pos_;
        while (pos_ < line_.size () &&
               (std::isalnum (static_cast<unsigned char> (line_[pos_])) != 0 ||
                line_[pos_] == '_'))
          ++pos_;
        Token token (Token::Kind::Ident);
        token.text = line_.substr (start, pos_ - start);
        return token;
      }
    throw ParseError (line_no_, pos_ + 1, "unexpected character in OPB input");
  }

private:
  void skip_ws ()
  {
    while (pos_ < line_.size () &&
           std::isspace (static_cast<unsigned char> (line_[pos_])) != 0)
      ++pos_;
  }
  char peek (std::size_t ahead) const
  {
    return pos_ + ahead < line_.size () ? line_[pos_ + ahead] : '\0';
  }

  std::string line_;
  std::size_t line_no_ = 1;
  std::size_t pos_ = 0;
};

inline int parse_opb_var (const std::string &name, std::size_t line_no)
{
  if (name.size() < 2 || (name[0] != 'x' && name[0] != 'X'))
    throw ParseError (line_no, 1, "OPB variables must look like xN, got '" + name + "'");
  try
    {
      const int var = std::stoi (name.substr (1));
      if (var <= 0)
        throw ParseError (line_no, 1, "OPB variable numbers start at 1");
      return var;
    }
  catch (const ParseError &)
    {
      throw;
    }
  catch (const std::exception &)
    {
      throw ParseError (line_no, 1, "bad OPB variable '" + name + "'");
    }
}

} // namespace pb_detail

inline PBProblem parse_opb (std::istream &input)
{
  PBProblem problem;
  std::string line;
  std::size_t line_no = 0;

  // Line-oriented parse: split each line into a term list, a comparison or
  // colon, and a right-hand side.
  while (std::getline (input, line))
    {
      ++line_no;
      std::string stripped = trim_view (line);
      if (stripped.empty () || stripped[0] == '*')
        continue;
      pb_detail::Lexer lexer (stripped, line_no);
      pb_detail::Token first = lexer.next ();
      bool is_objective = false;
      bool minimize = true;
      if (first.kind == pb_detail::Token::Kind::Ident &&
          (first.text == "min" || first.text == "max"))
        {
          is_objective = true;
          minimize = first.text == "min";
          pb_detail::Token colon = lexer.next ();
          if (colon.kind != pb_detail::Token::Kind::Colon)
            throw ParseError (line_no, 1, "expected ':' after min/max");
        }
      // Collect terms until a comparison/colon/semicolon/end token. For
      // objective lines `first` was the min/max keyword (already consumed);
      // otherwise it is the first term token and must be replayed.
      std::vector<PBTerm> terms;
      pb_detail::Token lookahead = first;
      bool has_lookahead = !is_objective;
      auto next_meaningful = [&] () {
        if (has_lookahead)
          {
            has_lookahead = false;
            return lookahead;
          }
        return lexer.next ();
      };
      while (true)
        {
          pb_detail::Token token = next_meaningful ();
          if (token.kind == pb_detail::Token::Kind::Plus)
            token = lexer.next ();
          if (token.kind == pb_detail::Token::Kind::Ge ||
              token.kind == pb_detail::Token::Kind::Le ||
              token.kind == pb_detail::Token::Kind::Eq ||
              token.kind == pb_detail::Token::Kind::Colon ||
              token.kind == pb_detail::Token::Kind::Semicolon ||
              token.kind == pb_detail::Token::Kind::End)
            {
              lookahead = token;
              has_lookahead = true;
              break;
            }
          if (token.kind != pb_detail::Token::Kind::Number)
            throw ParseError (line_no, 1, "expected a coefficient in OPB term");
          const long long coeff = token.number;
          token = lexer.next ();
          if (token.kind == pb_detail::Token::Kind::Star)
            token = lexer.next ();
          if (token.kind != pb_detail::Token::Kind::Ident)
            throw ParseError (line_no, 1, "expected a variable in OPB term");
          const int var = pb_detail::parse_opb_var (token.text, line_no);
          problem.variables = std::max (problem.variables, var);
          terms.push_back ({ coeff, var });
        }
      pb_detail::Token delim = next_meaningful ();
      if (is_objective)
        {
          if (delim.kind != pb_detail::Token::Kind::Semicolon &&
              delim.kind != pb_detail::Token::Kind::End)
            throw ParseError (line_no, 1, "expected ';' after the objective");
          if (problem.objective.present)
            throw ParseError (line_no, 1, "duplicate OPB objective");
          problem.objective = { true, minimize, std::move (terms) };
          continue;
        }
      PBCmp cmp = PBCmp::Ge;
      if (delim.kind == pb_detail::Token::Kind::Ge)
        cmp = PBCmp::Ge;
      else if (delim.kind == pb_detail::Token::Kind::Le)
        cmp = PBCmp::Le;
      else if (delim.kind == pb_detail::Token::Kind::Eq)
        cmp = PBCmp::Eq;
      else
        throw ParseError (line_no, 1, "expected >=, <=, or = in OPB constraint");
      pb_detail::Token rhs_token = lexer.next ();
      if (rhs_token.kind != pb_detail::Token::Kind::Number)
        throw ParseError (line_no, 1, "expected an integer right-hand side");
      pb_detail::Token end = lexer.next ();
      if (end.kind != pb_detail::Token::Kind::Semicolon &&
          end.kind != pb_detail::Token::Kind::End)
        throw ParseError (line_no, 1, "expected ';' after OPB constraint");
      problem.constraints.push_back ({ std::move (terms), cmp, rhs_token.number });
    }
  return problem;
}

inline PBProblem parse_opb_text (const std::string &text)
{
  std::istringstream input (text);
  return parse_opb (input);
}

/// Bit-blasts one constraint into `enc` over 0/1 variable bits `var_bits`
/// (indexed by 0-based SAT variable), using a signed working width.
inline void blast_pb_constraint (theory::Encoder &enc,
                                 const std::vector<Lit> &var_bits,
                                 const PBConstraint &constraint, int width)
{
  const Lit zero = enc.const_false ();
  const Lit one = enc.const_true ();
  std::vector<Lit> sum (static_cast<std::size_t> (width), zero);
  for (const PBTerm &term : constraint.terms)
    {
      const Lit bit = var_bits[static_cast<std::size_t> (term.var - 1)];
      // coeff * x by shift-and-add over the single 0/1 bit.
      std::vector<Lit> acc (static_cast<std::size_t> (width), zero);
      const bool negative = term.coeff < 0;
      std::uint64_t mag = negative ? std::uint64_t{ 0 } - static_cast<std::uint64_t> (term.coeff)
                                    : static_cast<std::uint64_t> (term.coeff);
      int shift = 0;
      while (mag != 0)
        {
          if ((mag & 1u) != 0)
            {
              std::vector<Lit> shifted (static_cast<std::size_t> (width), zero);
              if (shift < width)
                shifted[static_cast<std::size_t> (shift)] = bit;
              std::vector<Lit> next = enc.ripple_add (acc, shifted, enc.const_false ());
              next.pop_back ();
              acc = std::move (next);
            }
          mag >>= 1;
          ++shift;
        }
      if (negative)
        {
          for (Lit &lit : acc)
            lit = negate (lit);
          std::vector<Lit> unit (static_cast<std::size_t> (width), zero);
          unit[0] = one;
          std::vector<Lit> next = enc.ripple_add (acc, unit, enc.const_false ());
          next.pop_back ();
          acc = std::move (next);
        }
      std::vector<Lit> next = enc.ripple_add (sum, acc, enc.const_false ());
      next.pop_back ();
      sum = std::move (next);
    }
  auto const_bits = [&] (long long value) {
    std::vector<Lit> vec;
    for (int i = 0; i < width; ++i)
      vec.push_back (((value >> i) & 1) != 0 ? one : zero);
    return vec;
  };
  if (constraint.cmp == PBCmp::Ge)
    {
      // sum >= rhs  <=>  sum > rhs - 1  <=>  !(sum < rhs).
      std::vector<Lit> limit = const_bits (constraint.rhs);
      enc.add_clause ({ negate (enc.ult (sum, limit)) });
    }
  else if (constraint.cmp == PBCmp::Le)
    {
      std::vector<Lit> limit = const_bits (constraint.rhs + 1);
      enc.add_clause ({ enc.ult (sum, limit) });
    }
  else
    {
      std::vector<Lit> lo = const_bits (constraint.rhs);
      std::vector<Lit> hi = const_bits (constraint.rhs + 1);
      enc.add_clause ({ negate (enc.ult (sum, lo)) });
      enc.add_clause ({ enc.ult (sum, hi) });
    }
}

inline int pb_working_width (const PBProblem &problem)
{
  std::uint64_t magnitude = 1;
  std::size_t terms = 1;
  auto account = [&] (const std::vector<PBTerm> &list) {
    terms = std::max (terms, list.size ());
    for (const PBTerm &term : list)
      {
        const std::uint64_t mag =
            term.coeff < 0 ? std::uint64_t{ 0 } - static_cast<std::uint64_t> (term.coeff)
                           : static_cast<std::uint64_t> (term.coeff);
        magnitude = std::max (magnitude, mag);
      }
  };
  for (const PBConstraint &constraint : problem.constraints)
    account (constraint.terms);
  if (problem.objective.present)
    account (problem.objective.terms);
  int bits = 2;
  const long double need =
      static_cast<long double> (magnitude) * static_cast<long double> (terms) + 1.0L;
  while (bits < 60 && std::pow (2.0L, bits) <= need)
    ++bits;
  return bits + 2;
}

/// Encodes the constraints (not the objective) as CNF over SAT variables
/// `1..variables` matching the OPB `xN` numbering.
inline CNF pb_to_cnf (const PBProblem &problem)
{
  if (problem.variables <= 0)
    return CNF{};
  const int width = pb_working_width (problem);
  if (width > 256)
    throw std::invalid_argument ("OPB coefficients exceed the supported width");
  theory::Encoder enc (static_cast<Var> (problem.variables + 1));
  std::vector<Lit> var_bits;
  for (int v = 1; v <= problem.variables; ++v)
    var_bits.push_back (v);
  for (const PBConstraint &constraint : problem.constraints)
    blast_pb_constraint (enc, var_bits, constraint, width);
  CNF cnf;
  for (Clause &clause : enc.take_clauses ())
    cnf.add_clause (std::move (clause));
  // Preserve the variable numbering even with no clauses.
  cnf.set_declared_variable_count (static_cast<std::size_t> (problem.variables));
  return cnf;
}

inline long long evaluate_pb_terms (const std::vector<PBTerm> &terms,
                                    const std::vector<int> &values)
{
  long long total = 0;
  for (const PBTerm &term : terms)
    total += term.coeff * values[static_cast<std::size_t> (term.var - 1)];
  return total;
}

inline SolveResult solve_pb (const PBProblem &problem)
{
  CNF cnf = pb_to_cnf (problem);
  SolveResult result = solve_cdcl (cnf);
  if (result.unsatisfiable ())
    return result;
  // Validate against the integer semantics (guards width truncation).
  std::vector<int> values (static_cast<std::size_t> (problem.variables), 0);
  for (int v = 1; v <= problem.variables; ++v)
    values[static_cast<std::size_t> (v - 1)] =
        result.assignment.get_var (v) == Value::TRUE ? 1 : 0;
  for (const PBConstraint &constraint : problem.constraints)
    {
      const long long sum = evaluate_pb_terms (constraint.terms, values);
      const bool ok = constraint.cmp == PBCmp::Ge  ? sum >= constraint.rhs
                      : constraint.cmp == PBCmp::Le ? sum <= constraint.rhs
                                                    : sum == constraint.rhs;
      if (!ok)
        return { SolveStatus::UNKNOWN, Assignment (cnf.variable_count ()) };
    }
  return { SolveStatus::SAT, result.assignment };
}

struct PBOptimum
{
  SolveStatus status = SolveStatus::UNKNOWN;
  long long value = 0;
  Assignment assignment{};
};

/// Minimizes (or maximizes) the objective by binary search over asserted
/// bounds. Bounds outside the derivable range report UNKNOWN.
inline PBOptimum optimize_pb (const PBProblem &problem, bool minimize)
{
  PBOptimum optimum;
  if (!problem.objective.present)
    {
      SolveResult base = solve_pb (problem);
      optimum.status = base.status;
      optimum.assignment = base.assignment;
      return optimum;
    }
  // Objective range from coefficient magnitudes (0/1 variables).
  long long lo = 0, hi = 0;
  for (const PBTerm &term : problem.objective.terms)
    {
      if (term.coeff < 0)
        lo += term.coeff;
      else
        hi += term.coeff;
    }
  PBProblem bounded = problem;
  long long best = minimize ? hi : lo;
  Assignment best_model;
  bool found = false;
  long long low = lo, high = hi;
  while (low <= high)
    {
      const long long mid = low + (high - low) / 2;
      bounded.constraints = problem.constraints;
      bounded.constraints.push_back (
          { problem.objective.terms, minimize ? PBCmp::Le : PBCmp::Ge, mid });
      SolveResult attempt = solve_pb (bounded);
      if (attempt.unsatisfiable ())
        {
          if (minimize)
            low = mid + 1;
          else
            high = mid - 1;
          continue;
        }
      if (!attempt.satisfiable ())
        {
          optimum.status = SolveStatus::UNKNOWN;
          return optimum;
        }
      found = true;
      best = mid;
      best_model = attempt.assignment;
      if (minimize)
        high = mid - 1;
      else
        low = mid + 1;
    }
  if (!found)
    {
      optimum.status = SolveStatus::UNSAT;
      return optimum;
    }
  optimum.status = SolveStatus::SAT;
  optimum.value = best;
  optimum.assignment = best_model;
  return optimum;
}

inline PBOptimum optimize_pb_min (const PBProblem &problem)
{
  return optimize_pb (problem, true);
}

inline PBOptimum optimize_pb_max (const PBProblem &problem)
{
  return optimize_pb (problem, false);
}

/// Version anchor defined in src/SatieFrontendOPB.cpp.
const char *frontend_opb_component_version () noexcept;

} // namespace satie::frontend
