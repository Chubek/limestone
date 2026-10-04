#pragma once

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Common.hpp"

namespace satie::frontend
{

struct SExpression
{
  bool is_list = false;
  std::string atom;
  std::vector<SExpression> children;
};

struct BooleanSyntax
{
  enum class Kind { Var, True, False, Not, And, Or, Xor, Imp, Iff };
  explicit BooleanSyntax (Kind k = Kind::False) : kind (k) {}
  Kind kind;
  std::string name;
  std::vector<BooleanSyntax> children;
};

/// Shared libglr parsers. Syntax errors retain original source coordinates.
/// Nesting is limited to 256 levels to bound AST traversal stack use.
std::vector<SExpression> parse_sexpressions (const std::string &text,
                                            std::size_t line_base = 1);
SExpression parse_expression_tokens (const std::vector<std::string> &tokens,
                                     std::size_t &position, std::size_t line);
BooleanSyntax parse_boolean_syntax (const std::string &text);

/// Shared types for the language frontends (`wcnf`/`opb`/`smt2`/ ...).
/// Parsing errors reuse `satie::ParseError` with line/column positions.
struct WeightedClause
{
  Clause clause;
  long long weight = 0;
};

/// Weighted CNF: `hard` clauses plus `soft` clauses with weights. A model
/// with `cost` falsifies soft clauses totaling `cost`; clauses with weight
/// `>= top` are hard (`top` is the DIMACS-WCNF convention).
struct WeightedCNF
{
  long long top = 0;
  ClauseList hard;
  std::vector<WeightedClause> soft;
  std::size_t variables = 0;

  long long total_soft () const
  {
    long long total = 0;
    for (const WeightedClause &soft : soft)
      total += soft.weight;
    return total;
  }
};

inline std::string trim_view (std::string_view text)
{
  std::size_t begin = 0;
  while (begin < text.size () &&
         std::isspace (static_cast<unsigned char> (text[begin])) != 0)
    ++begin;
  std::size_t end = text.size ();
  while (end > begin && std::isspace (static_cast<unsigned char> (text[end - 1])) != 0)
    --end;
  return std::string (text.substr (begin, end - begin));
}

inline bool starts_with_word (std::string_view line, std::string_view word)
{
  if (line.size () < word.size () || line.substr (0, word.size ()) != word)
    return false;
  return line.size () == word.size () ||
         std::isspace (static_cast<unsigned char> (line[word.size ()])) != 0;
}

/// Version anchor defined in src/SatieFrontendCommon.cpp.
const char *frontend_common_component_version () noexcept;

} // namespace satie::frontend
