#pragma once

#include <utility>
#include <vector>

#include "Common.hpp"

namespace satie::theory
{

/// Tseitin encoder shared by the bit-blasting theories (BV, LIA, Set, ...).
/// Literals follow the SAT convention (`Var` ids, negative = negated) and all
/// bit vectors are little-endian (`bits[0]` is the least significant bit).
class Encoder
{
public:
  explicit Encoder (Var first_fresh) : next_ (first_fresh) {}

  Var fresh_var () { return next_++; }
  Lit fresh_lit () { return fresh_var (); }

  void add_clause (Clause clause) { clauses_.push_back (std::move (clause)); }
  const ClauseList &clauses () const { return clauses_; }
  ClauseList take_clauses () { return std::move (clauses_); }

  /// Fresh literal constrained to true (adds the unit clause).
  Lit const_true ()
  {
    Lit t = fresh_lit ();
    add_clause ({ t });
    return t;
  }

  Lit const_false () { return negate (const_true ()); }

  Lit land2 (Lit a, Lit b)
  {
    Lit r = fresh_lit ();
    add_clause ({ negate (a), negate (b), r });
    add_clause ({ a, negate (r) });
    add_clause ({ b, negate (r) });
    return r;
  }

  Lit lor2 (Lit a, Lit b)
  {
    Lit r = fresh_lit ();
    add_clause ({ a, b, negate (r) });
    add_clause ({ negate (a), r });
    add_clause ({ negate (b), r });
    return r;
  }

  Lit lxor2 (Lit a, Lit b)
  {
    Lit r = fresh_lit ();
    add_clause ({ negate (a), negate (b), negate (r) });
    add_clause ({ a, b, negate (r) });
    add_clause ({ negate (a), b, r });
    add_clause ({ a, negate (b), r });
    return r;
  }

  /// Constrains `a` and `b` (equal length) to be bitwise identical.
  void assert_eq_bits (const std::vector<Lit> &a, const std::vector<Lit> &b)
  {
    for (std::size_t i = 0; i < a.size (); ++i)
      {
        add_clause ({ negate (a[i]), b[i] });
        add_clause ({ a[i], negate (b[i]) });
      }
  }

  /// Ripple-carry addition. `a` and `b` must share a length; returns the sum
  /// bits plus the carry out (`a.size () + 1` literals).
  std::vector<Lit> ripple_add (const std::vector<Lit> &a, const std::vector<Lit> &b,
                              Lit carry_in)
  {
    std::vector<Lit> sum;
    sum.reserve (a.size () + 1);
    Lit carry = carry_in;
    for (std::size_t i = 0; i < a.size (); ++i)
      {
        Lit t = lxor2 (a[i], b[i]);
        sum.push_back (lxor2 (t, carry));
        Lit cout = fresh_lit ();
        add_clause ({ negate (a[i]), negate (b[i]), cout });
        add_clause ({ negate (carry), negate (t), cout });
        add_clause ({ negate (cout), a[i], b[i] });
        add_clause ({ negate (cout), a[i], carry });
        add_clause ({ negate (cout), b[i], carry });
        carry = cout;
      }
    sum.push_back (carry);
    return sum;
  }

  /// Unsigned `a < b` (equal length). Returns a literal that is true exactly
  /// when the relation holds.
  Lit ult (const std::vector<Lit> &a, const std::vector<Lit> &b)
  {
    std::vector<Lit> inverted = b;
    for (Lit &lit : inverted)
      lit = negate (lit);
    std::vector<Lit> diff = ripple_add (a, inverted, const_true ());
    // a - b borrows (carry out 0) exactly when a < b.
    return negate (diff.back ());
  }

  /// At-most-`k` over `xs` (Sinz sequential counter). `k < 0` encodes FALSE.
  void at_most_k (const std::vector<Lit> &xs, int k)
  {
    if (k < 0)
      {
        add_clause ({});
        return;
      }
    if (xs.empty ())
      return;
    if (k == 0)
      {
        for (Lit x : xs)
          add_clause ({ negate (x) });
        return;
      }
    if (xs.size () <= static_cast<std::size_t> (k))
      return;
    const std::size_t n = xs.size ();
    std::vector<std::vector<Lit>> s (n + 1, std::vector<Lit> (k + 1, 0));
    for (std::size_t i = 1; i <= n; ++i)
      for (int j = 1; j <= k; ++j)
        s[i][j] = fresh_lit ();
    add_clause ({ negate (xs[0]), s[1][1] });
    for (int j = 2; j <= k; ++j)
      add_clause ({ negate (s[1][j]) });
    for (std::size_t i = 2; i <= n; ++i)
      {
        add_clause ({ negate (xs[i - 1]), s[i][1] });
        add_clause ({ negate (s[i - 1][1]), s[i][1] });
        for (int j = 2; j <= k; ++j)
          {
            add_clause ({ negate (xs[i - 1]), negate (s[i - 1][j - 1]), s[i][j] });
            add_clause ({ negate (s[i - 1][j]), s[i][j] });
          }
        add_clause ({ negate (xs[i - 1]), negate (s[i - 1][k]) });
      }
  }

  /// At-least-`k` over `xs`, via at-most over the negated inputs.
  void at_least_k (const std::vector<Lit> &xs, int k)
  {
    std::vector<Lit> negated = xs;
    for (Lit &lit : negated)
      lit = negate (lit);
    at_most_k (negated, static_cast<int> (xs.size ()) - k);
  }

private:
  Var next_ = 1;
  ClauseList clauses_;
};

} // namespace satie::theory
