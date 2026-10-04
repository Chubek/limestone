#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Common.hpp"
#include "SatieTheoryUtils.hpp"

namespace satie::stdlib::encoding
{

/// Shared CNF cardinality encodings over caller-owned literals (1-based SAT
/// variables; `first_fresh` is the first auxiliary id). All encodings are
/// exact: at-most/at-least use a sequential counter, exactly-one combines
/// both with a long clause. WCNF/PB frontends use these shapes internally;
/// this module exposes them for embedding code.
inline ClauseList encode_at_most_k (const std::vector<Lit> &literals, int k,
                                    Var first_fresh)
{
  theory::Encoder enc (first_fresh);
  enc.at_most_k (literals, k);
  return enc.take_clauses ();
}

inline ClauseList encode_at_least_k (const std::vector<Lit> &literals, int k,
                                     Var first_fresh)
{
  theory::Encoder enc (first_fresh);
  enc.at_least_k (literals, k);
  return enc.take_clauses ();
}

inline ClauseList encode_at_most_one (const std::vector<Lit> &literals)
{
  ClauseList clauses;
  for (std::size_t i = 0; i < literals.size (); ++i)
    for (std::size_t j = i + 1; j < literals.size (); ++j)
      clauses.push_back ({ negate (literals[i]), negate (literals[j]) });
  return clauses;
}

inline ClauseList encode_exactly_one (const std::vector<Lit> &literals,
                                      Var first_fresh)
{
  ClauseList clauses = encode_at_most_k (literals, 1, first_fresh);
  clauses.push_back (literals);
  return clauses;
}

} // namespace satie::stdlib::encoding
