#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"

namespace satie::poly
{

/// Multivariate integer polynomials with exact normal forms (sound;
/// complete for univariate integer-root queries and explicitly bounded
/// problems).
///
/// Polynomials are stored as sorted monomial maps (exponent vector ->
/// coefficient) with exact addition and multiplication. Constraints are
/// `p ==/!=/<=/</>=/> 0`. `check` decides: univariate equalities by the
/// rational-root theorem (complete for integer roots); everything else by
/// bounded-box search (`set_search_bound`, explicit `add_bound` ranges make
/// exhaustion a sound UNSAT certificate). The Boolean problem is checked
/// first as a layered filter.
class PolySolver
{
public:
  PolySolver () = default;
  explicit PolySolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "Poly";
  }

  /// `load` replaces the Boolean problem; polynomial constraints are
  /// preserved. Use `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    var_index_.clear ();
    constraints_.clear ();
    bounds_.clear ();
    model_.clear ();
  }

  void set_search_bound (std::int64_t bound)
  {
    if (bound < 0)
      throw std::invalid_argument ("poly search bound must be non-negative");
    search_bound_ = bound;
  }

  int add_var (const std::string &name)
  {
    auto it = var_index_.find (name);
    if (it != var_index_.end ())
      return it->second;
    const int id = static_cast<int> (var_index_.size ());
    var_index_.emplace (name, id);
    return id;
  }

  /// Explicit range for `var`, making box exhaustion sound for UNSAT.
  void add_bound (int var, std::int64_t lo, std::int64_t hi)
  {
    require_var (var);
    if (hi < lo)
      throw std::invalid_argument ("poly bound is inverted");
    bounds_.push_back ({ var, lo, hi });
  }

  /// Monomial `coeff * prod(vars[i]^exps[i])`; exponents must align with the
  /// variable count at call time (missing entries are zero).
  using Monomial = std::pair<std::vector<int>, std::int64_t>;

  struct Polynomial
  {
    std::map<std::vector<int>, std::int64_t> terms;
  };

  static Polynomial make_poly (const std::vector<Monomial> &monomials, std::size_t nvars)
  {
    Polynomial poly;
    for (const auto &[exps, coeff] : monomials)
      {
        if (exps.size () != nvars || coeff == 0)
          continue;
        poly.terms[exps] += coeff;
        if (poly.terms[exps] == 0)
          poly.terms.erase (exps);
      }
    return poly;
  }

  static Polynomial add (const Polynomial &a, const Polynomial &b)
  {
    Polynomial out = a;
    for (const auto &[exps, coeff] : b.terms)
      {
        out.terms[exps] += coeff;
        if (out.terms[exps] == 0)
          out.terms.erase (exps);
      }
    return out;
  }

  static Polynomial negate_poly (const Polynomial &a)
  {
    Polynomial out;
    for (const auto &[exps, coeff] : a.terms)
      out.terms[exps] = -coeff;
    return out;
  }

  static Polynomial mul (const Polynomial &a, const Polynomial &b, std::size_t nvars)
  {
    Polynomial out;
    for (const auto &[ea, ca] : a.terms)
      for (const auto &[eb, cb] : b.terms)
        {
          std::vector<int> exps (nvars, 0);
          for (std::size_t i = 0; i < nvars; ++i)
            exps[i] = ea[i] + eb[i];
          out.terms[exps] = add_sat (out.terms[exps], mul_sat (ca, cb));
          if (out.terms[exps] == 0)
            out.terms.erase (exps);
        }
    return out;
  }

  enum class Sense
  {
    Eq,
    Ne,
    Le,
    Lt,
    Ge,
    Gt
  };

  void add_constraint (Polynomial poly, Sense sense)
  {
    for (const auto &[exps, coeff] : poly.terms)
      {
        (void)coeff;
        if (exps.size () != var_index_.size ())
          throw std::invalid_argument ("poly exponents must match the variable count");
      }
    constraints_.push_back ({ std::move (poly), sense });
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    model_.clear ();
    std::vector<std::int64_t> witness;
    const BoxResult boxed = boxed_search (witness);
    if (boxed == BoxResult::Sat)
      {
        model_ = witness;
        return { SolveStatus::SAT, boolean.assignment };
      }
    // Univariate integer roots can lie outside the searched box.
    if (univariate_search (witness))
      {
        model_ = witness;
        return { SolveStatus::SAT, boolean.assignment };
      }
    // `Unsat` from the boxed search is exhaustive (all variables explicitly
    // bounded inside the box), hence a sound UNSAT certificate.
    if (boxed == BoxResult::Unsat)
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
  }

  std::int64_t value (int var) const
  {
    require_var (var);
    if (model_.empty ())
      throw std::logic_error ("poly model is unavailable before a SAT check");
    return model_[static_cast<std::size_t> (var)];
  }

private:
  struct Constraint
  {
    Polynomial poly;
    Sense sense = Sense::Eq;
  };
  struct Bound
  {
    int var = -1;
    std::int64_t lo = 0;
    std::int64_t hi = 0;
  };

  enum class BoxResult
  {
    Sat,
    Unsat,
    Unknown
  };

  void require_var (int var) const
  {
    if (var < 0 || var >= static_cast<int> (var_index_.size ()))
      throw std::invalid_argument ("unknown poly variable id");
  }

  static std::int64_t add_sat (std::int64_t a, std::int64_t b)
  {
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
      return b > 0 ? INT64_MAX : INT64_MIN;
    return a + b;
  }

  static std::int64_t mul_sat (std::int64_t a, std::int64_t b)
  {
    if (a == 0 || b == 0)
      return 0;
    if (a > 0 && b > 0)
      return a > INT64_MAX / b ? INT64_MAX : a * b;
    if (a < 0 && b < 0)
      return a < INT64_MAX / b ? INT64_MAX : a * b;
    if (a > 0)
      return b < INT64_MIN / a ? INT64_MIN : a * b;
    return a < INT64_MIN / b ? INT64_MIN : a * b;
  }

  static std::int64_t power_sat (std::int64_t base, int exp)
  {
    std::int64_t result = 1;
    for (int i = 0; i < exp; ++i)
      result = mul_sat (result, base);
    return result;
  }

  /// Saturating evaluation; overflows clamp (the boxed search stays inside
  /// small bounds, so clamping only triggers on adversarial coefficients).
  static std::int64_t evaluate (const Polynomial &poly,
                                const std::vector<std::int64_t> &values)
  {
    std::int64_t total = 0;
    for (const auto &[exps, coeff] : poly.terms)
      {
        std::int64_t term = coeff;
        for (std::size_t i = 0; i < exps.size (); ++i)
          term = mul_sat (term, power_sat (values[i], exps[i]));
        total = add_sat (total, term);
      }
    return total;
  }

  static bool holds (std::int64_t value, Sense sense)
  {
    switch (sense)
      {
      case Sense::Eq:
        return value == 0;
      case Sense::Ne:
        return value != 0;
      case Sense::Le:
        return value <= 0;
      case Sense::Lt:
        return value < 0;
      case Sense::Ge:
        return value >= 0;
      case Sense::Gt:
        return value > 0;
      }
    return false;
  }

  bool holds_all (const std::vector<std::int64_t> &values) const
  {
    for (const Constraint &con : constraints_)
      if (!holds (evaluate (con.poly, values), con.sense))
        return false;
    return true;
  }

  BoxResult boxed_search (std::vector<std::int64_t> &witness)
  {
    const std::size_t n = var_index_.size ();
    std::vector<std::int64_t> lo (n, -search_bound_), hi (n, search_bound_);
    for (const Bound &b : bounds_)
      {
        lo[static_cast<std::size_t> (b.var)] =
            std::max (lo[static_cast<std::size_t> (b.var)], b.lo);
        hi[static_cast<std::size_t> (b.var)] =
            std::min (hi[static_cast<std::size_t> (b.var)], b.hi);
      }
    for (std::size_t i = 0; i < n; ++i)
      if (hi[i] < lo[i])
        return BoxResult::Unsat;
    std::size_t cells = 1;
    for (std::size_t i = 0; i < n; ++i)
      {
        const std::uint64_t width =
            static_cast<std::uint64_t> (hi[i] - lo[i]) + 1;
        if (width > search_cap_ || cells > search_cap_ / width)
          return BoxResult::Unknown;
        cells *= width;
      }
    std::vector<std::int64_t> cursor = lo;
    for (std::size_t step = 0; step < cells; ++step)
      {
        if (holds_all (cursor))
          {
            witness = cursor;
            return BoxResult::Sat;
          }
        for (std::size_t i = 0; i < n; ++i)
          {
            if (cursor[i] < hi[i])
              {
                ++cursor[i];
                break;
              }
            cursor[i] = lo[i];
          }
      }
    // Exhaustive over the box. Sound UNSAT only when every variable is
    // explicitly bounded inside the searched box.
    for (std::size_t i = 0; i < n; ++i)
      {
        bool covered = false;
        for (const Bound &b : bounds_)
          if (b.var == static_cast<int> (i) && b.lo >= -search_bound_ &&
              b.hi <= search_bound_)
            covered = true;
        if (!covered)
          return BoxResult::Unknown;
      }
    return BoxResult::Unsat;
  }

  /// Integer roots of a univariate polynomial by the rational-root theorem.
  static std::vector<std::int64_t> integer_roots_univariate (const Polynomial &poly,
                                                             int var, std::size_t nvars)
  {
    // Coefficients of x^0..x^d.
    std::map<int, std::int64_t> coeff_of_degree;
    for (const auto &[exps, coeff] : poly.terms)
      {
        bool univariate = true;
        int degree = 0;
        for (std::size_t i = 0; i < nvars; ++i)
          if (static_cast<int> (i) == var)
            degree = exps[i];
          else if (exps[i] != 0)
            univariate = false;
        if (!univariate)
          return {};
        coeff_of_degree[degree] += coeff;
      }
    if (coeff_of_degree.empty ())
      return {};
    int top = coeff_of_degree.rbegin ()->first;
    const std::int64_t leading = coeff_of_degree[top];
    if (leading == 0)
      return {};
    std::vector<std::int64_t> candidates;
    if (coeff_of_degree.count (0) == 0 || coeff_of_degree[0] == 0)
      candidates.push_back (0); // Zero constant term: 0 is a root.
    const std::int64_t constant =
        coeff_of_degree.count (0) != 0 ? coeff_of_degree[0] : 0;
    const std::uint64_t mag = constant < 0
                                  ? std::uint64_t{ 0 } - static_cast<std::uint64_t> (constant)
                                  : static_cast<std::uint64_t> (constant);
    for (std::uint64_t d = 1; d * d <= mag; ++d)
      if (mag % d == 0)
        {
          candidates.push_back (static_cast<std::int64_t> (d));
          candidates.push_back (-static_cast<std::int64_t> (d));
          if (d * d != mag)
            {
              candidates.push_back (static_cast<std::int64_t> (mag / d));
              candidates.push_back (-static_cast<std::int64_t> (mag / d));
            }
        }
    // Filter by exact evaluation (rational-root theorem over-approximates).
    std::vector<std::int64_t> roots;
    std::vector<std::int64_t> probe (nvars, 0);
    for (std::int64_t candidate : candidates)
      {
        probe[static_cast<std::size_t> (var)] = candidate;
        if (evaluate (poly, probe) == 0 &&
            std::find (roots.begin (), roots.end (), candidate) == roots.end ())
          roots.push_back (candidate);
      }
    return roots;
  }

  /// Tries single-variable integer roots for equalities; other variables
  /// are held at zero (best effort beyond the searched box).
  bool univariate_search (std::vector<std::int64_t> &witness)
  {
    const std::size_t n = var_index_.size ();
    if (n == 0)
      return false;
    for (const Constraint &con : constraints_)
      {
        if (con.sense != Sense::Eq)
          continue;
        int live = -1;
        bool multivariate = false;
        for (const auto &[exps, coeff] : con.poly.terms)
          {
            (void)coeff;
            for (std::size_t i = 0; i < exps.size (); ++i)
              if (exps[i] != 0)
                {
                  if (live < 0)
                    live = static_cast<int> (i);
                  else if (live != static_cast<int> (i))
                    multivariate = true;
                }
          }
        if (multivariate || live < 0)
          continue;
        std::vector<std::int64_t> roots =
            integer_roots_univariate (con.poly, live, n);
        for (std::int64_t root : roots)
          {
            std::vector<std::int64_t> probe (n, 0);
            probe[static_cast<std::size_t> (live)] = root;
            if (holds_all (probe))
              {
                witness = probe;
                return true;
              }
          }
      }
    return false;
  }

  CNF cnf_{};
  std::map<std::string, int> var_index_;
  std::vector<Constraint> constraints_;
  std::vector<Bound> bounds_;
  std::vector<std::int64_t> model_;
  std::int64_t search_bound_ = 8;
  std::size_t search_cap_ = 200000;
};

} // namespace satie::poly
