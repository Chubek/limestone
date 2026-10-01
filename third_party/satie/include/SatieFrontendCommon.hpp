#pragma once

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Common.hpp"

namespace satie::frontend
{

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
