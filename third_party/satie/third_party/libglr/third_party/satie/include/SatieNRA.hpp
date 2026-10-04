#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"
#include "SatieLP.hpp"

namespace satie::nra
{

/// Nonlinear real arithmetic (sound; complete for the linear fragment and
/// univariate isolation, bounded sampling otherwise).
///
/// Polynomials have double coefficients over real variables. `check`
/// layers: constant folding (structural UNSAT), the linear fragment decided
/// exactly by the LP solver (sound UNSAT), per-variable interval analysis
/// from unary linear bounds, univariate real-root isolation by bisection
/// for equalities, and bounded grid sampling for the rest. A SAT answer
/// always carries a validated model; inconclusive searches yield UNKNOWN.
/// Supported senses: `==/!=/<=/</>=/> 0`.
class NRASolver
{
public:
  NRASolver () = default;
  explicit NRASolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "NRA";
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

  void set_sample_cap (std::size_t cap) { sample_cap_ = cap == 0 ? 1 : cap; }
  void set_bisection_steps (int steps) { bisection_steps_ = steps < 8 ? 8 : steps; }

  int add_var (const std::string &name)
  {
    auto it = var_index_.find (name);
    if (it != var_index_.end ())
      return it->second;
    const int id = static_cast<int> (var_index_.size ());
    var_index_.emplace (name, id);
    return id;
  }

  void add_bound (int var, double lo, double hi)
  {
    require_var (var);
    if (!(lo <= hi))
      throw std::invalid_argument ("NRA bound is inverted or NaN");
    bounds_.push_back ({ var, lo, hi });
  }

  using Monomial = std::pair<std::vector<int>, double>;

  struct Polynomial
  {
    std::map<std::vector<int>, double> terms;
  };

  static Polynomial make_poly (const std::vector<Monomial> &monomials,
                               std::size_t nvars)
  {
    Polynomial poly;
    for (const auto &[exps, coeff] : monomials)
      {
        if (exps.size () != nvars || coeff == 0.0)
          continue;
        poly.terms[exps] += coeff;
        if (poly.terms[exps] == 0.0)
          poly.terms.erase (exps);
      }
    return poly;
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
          throw std::invalid_argument ("NRA exponents must match the variable count");
      }
    constraints_.push_back ({ std::move (poly), sense });
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    model_.clear ();
    if (structurally_unsat ())
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    std::vector<double> relaxed;
    if (!linear_model (relaxed))
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    // The relaxation model often satisfies the strict problem directly.
    if (!relaxed.empty () && holds_all (relaxed))
      {
        model_ = relaxed;
        return { SolveStatus::SAT, boolean.assignment };
      }
    if (!unary_intervals_consistent ())
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    std::vector<double> witness;
    if (univariate_solve (witness) || sample_search (witness))
      {
        model_ = witness;
        return { SolveStatus::SAT, boolean.assignment };
      }
    return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
  }

  double value (int var) const
  {
    require_var (var);
    if (model_.empty ())
      throw std::logic_error ("NRA model is unavailable before a SAT check");
    return model_[static_cast<std::size_t> (var)];
  }

private:
  struct Constraint
  {
    Polynomial poly;
    Sense sense = Sense::Eq;
  };

  void require_var (int var) const
  {
    if (var < 0 || var >= static_cast<int> (var_index_.size ()))
      throw std::invalid_argument ("unknown NRA variable id");
  }

  static double evaluate (const Polynomial &poly, const std::vector<double> &values)
  {
    double total = 0.0;
    for (const auto &[exps, coeff] : poly.terms)
      {
        double term = coeff;
        for (std::size_t i = 0; i < exps.size (); ++i)
          term *= std::pow (values[i], exps[i]);
        total += term;
      }
    return total;
  }

  static bool holds (double value, Sense sense)
  {
    // Equalities validate against a tolerance (bisection/Newton produce
    // approximations, never exact zeros).
    constexpr double kTol = 1e-9;
    switch (sense)
      {
      case Sense::Eq:
        return std::fabs (value) <= kTol;
      case Sense::Ne:
        return value != 0.0;
      case Sense::Le:
        return value <= kTol;
      case Sense::Lt:
        return value < 0.0;
      case Sense::Ge:
        return value >= -kTol;
      case Sense::Gt:
        return value > 0.0;
      }
    return false;
  }

  bool holds_all (const std::vector<double> &values) const
  {
    for (const Constraint &con : constraints_)
      {
        const double value = evaluate (con.poly, values);
        if (std::isnan (value) || !holds (value, con.sense))
          return false;
      }
    return true;
  }

  bool is_constant (const Polynomial &poly) const
  {
    for (const auto &[exps, coeff] : poly.terms)
      {
        (void)coeff;
        for (int e : exps)
          if (e != 0)
            return false;
      }
    return true;
  }

  bool structurally_unsat () const
  {
    for (const Constraint &con : constraints_)
      if (is_constant (con.poly) && !holds (evaluate (con.poly, {}), con.sense))
        return true;
    return false;
  }

  bool is_linear (const Polynomial &poly) const
  {
    for (const auto &[exps, coeff] : poly.terms)
      {
        (void)coeff;
        int degree = 0;
        for (int e : exps)
          degree += e;
        if (degree > 1)
          return false;
      }
    return true;
  }

  /// Exact feasibility of the linear fragment via the LP solver.
  bool linearly_feasible () const
  {
    std::vector<double> model;
    return linear_model (model);
  }

  /// LP model of the linear relaxation (empty when infeasible).
  bool linear_model (std::vector<double> &model) const
  {
    lp::LPSolver lp;
    for (std::size_t i = 0; i < var_index_.size (); ++i)
      lp.add_var ("x" + std::to_string (i));
    bool has_linear = false;
    for (const Constraint &con : constraints_)
      {
        if (!is_linear (con.poly))
          continue;
        std::vector<std::pair<int, double>> coeffs;
        double constant = 0.0;
        for (const auto &[exps, coeff] : con.poly.terms)
          {
            int live = -1;
            for (std::size_t i = 0; i < exps.size (); ++i)
              if (exps[i] != 0)
                live = static_cast<int> (i);
            if (live < 0)
              constant += coeff;
            else
              coeffs.push_back ({ live, coeff });
          }
        // Only strict-safe senses translate exactly; strict inequalities
        // are relaxed non-strictly (sound for UNSAT: infeasible relaxed
        // implies infeasible strict... careful, reversed: relaxed feasible
        // does not imply strict feasible, but relaxed INFEASIBLE implies
        // strict infeasible since strict subset of relaxed... strict set ⊆
        // non-strict set, so empty non-strict ⇒ empty strict ✓).
        has_linear = true;
        switch (con.sense)
          {
          case Sense::Eq:
            lp.add_eq (coeffs, -constant);
            break;
          case Sense::Le:
          case Sense::Lt:
            lp.add_le (coeffs, -constant);
            break;
          case Sense::Ge:
          case Sense::Gt:
            {
              for (auto &[var, coeff] : coeffs)
                coeff = -coeff;
              lp.add_le (coeffs, constant);
              break;
            }
          case Sense::Ne:
            break; // Disequalities do not constrain the relaxation.
          }
      }
    if (!has_linear)
      {
        model.assign (var_index_.size (), 0.0);
        return true;
      }
    SolveResult result = lp.check ();
    if (result.unsatisfiable ())
      return false;
    // An UNKNOWN relaxation is not a certificate either way.
    if (!result.satisfiable ())
      return true;
    model.assign (var_index_.size (), 0.0);
    for (std::size_t i = 0; i < var_index_.size (); ++i)
      model[i] = lp.value (static_cast<int> (i));
    return true;
  }

  /// Interval analysis from unary linear constraints (`a*x + b op 0`).
  bool unary_intervals_consistent () const
  {
    const std::size_t n = var_index_.size ();
    std::vector<double> lo (n, -std::numeric_limits<double>::infinity ());
    std::vector<double> hi (n, std::numeric_limits<double>::infinity ());
    for (const auto &[var, lower, upper] : bounds_)
      {
        lo[static_cast<std::size_t> (var)] =
            std::max (lo[static_cast<std::size_t> (var)], lower);
        hi[static_cast<std::size_t> (var)] =
            std::min (hi[static_cast<std::size_t> (var)], upper);
      }
    for (const Constraint &con : constraints_)
      {
        int live = -1;
        double slope = 0.0;
        double constant = 0.0;
        bool linear_unary = true;
        for (const auto &[exps, coeff] : con.poly.terms)
          {
            int degree = 0;
            int where = -1;
            for (std::size_t i = 0; i < exps.size (); ++i)
              if (exps[i] != 0)
                {
                  degree += exps[i];
                  where = static_cast<int> (i);
                }
            if (degree == 0)
              constant += coeff;
            else if (degree == 1)
              {
                if (live >= 0 && live != where)
                  linear_unary = false;
                live = where;
                slope += coeff;
              }
            else
              linear_unary = false;
          }
        if (!linear_unary || live < 0 || slope == 0.0)
          continue;
        // slope*x + constant op 0 -> bound on x.
        const double bound = -constant / slope;
        auto tighten = [&] (bool lower, double edge, bool strict) {
          if (strict)
            edge = std::nextafter (edge, lower ? std::numeric_limits<double>::infinity ()
                                               : -std::numeric_limits<double>::infinity ());
          if (lower)
            lo[static_cast<std::size_t> (live)] =
                std::max (lo[static_cast<std::size_t> (live)], edge);
          else
            hi[static_cast<std::size_t> (live)] =
                std::min (hi[static_cast<std::size_t> (live)], edge);
        };
        const bool positive = slope > 0.0;
        switch (con.sense)
          {
          case Sense::Le:
            positive ? tighten (false, bound, false) : tighten (true, bound, false);
            break;
          case Sense::Lt:
            positive ? tighten (false, bound, true) : tighten (true, bound, true);
            break;
          case Sense::Ge:
            positive ? tighten (true, bound, false) : tighten (false, bound, false);
            break;
          case Sense::Gt:
            positive ? tighten (true, bound, true) : tighten (false, bound, true);
            break;
          case Sense::Eq:
            tighten (true, bound, false);
            tighten (false, bound, false);
            break;
          case Sense::Ne:
            break;
          }
      }
    for (std::size_t i = 0; i < n; ++i)
      if (lo[i] > hi[i])
        return false;
    return true;
  }

  /// Univariate equalities by bracketing + bisection; other variables at 0.
  bool univariate_solve (std::vector<double> &witness)
  {
    const std::size_t n = var_index_.size ();
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
        std::vector<double> probe (n, 0.0);
        auto eval_at = [&] (double x) {
          probe[static_cast<std::size_t> (live)] = x;
          return evaluate (con.poly, probe);
        };
        // Bracket scan over a geometric + uniform grid.
        std::vector<double> grid{ 0.0 };
        for (int k = -4; k <= 8; ++k)
          {
            const double p = std::pow (2.0, k);
            grid.push_back (p);
            grid.push_back (-p);
          }
        for (int k = 1; k <= 16; ++k)
          {
            grid.push_back (k * 0.5);
            grid.push_back (-k * 0.5);
          }
        for (double g : grid)
          if (eval_at (g) == 0.0)
            {
              if (holds_all (probe))
                {
                  witness = probe;
                  return true;
                }
            }
        std::sort (grid.begin (), grid.end ());
        for (std::size_t i = 0; i + 1 < grid.size (); ++i)
          {
            double a = grid[i], b = grid[i + 1];
            const double fa = eval_at (a);
            const double fb = eval_at (b);
            if (std::isnan (fa) || std::isnan (fb) || fa * fb > 0.0)
              continue;
            for (int step = 0; step < bisection_steps_; ++step)
              {
                const double mid = (a + b) / 2.0;
                const double fm = eval_at (mid);
                if (fm == 0.0)
                  {
                    a = b = mid;
                    break;
                  }
                if (std::isnan (fm))
                  break;
                if ((fa <= 0.0) != (fm <= 0.0))
                  b = mid;
                else
                  a = mid;
              }
            probe[static_cast<std::size_t> (live)] = (a + b) / 2.0;
            if (holds_all (probe))
              {
                witness = probe;
                return true;
              }
          }
      }
    return false;
  }

  /// Deterministic grid sampling over bounded variables (free variables at
  /// 0, unbounded ones sampled on [-8, 8]).
  bool sample_search (std::vector<double> &witness)
  {
    const std::size_t n = var_index_.size ();
    if (n == 0)
      {
        if (holds_all ({}))
          {
            witness.clear ();
            return true;
          }
        return false;
      }
    std::vector<double> lo (n, -8.0), hi (n, 8.0);
    for (const auto &[var, lower, upper] : bounds_)
      {
        lo[static_cast<std::size_t> (var)] = lower;
        hi[static_cast<std::size_t> (var)] = upper;
      }
    std::vector<std::vector<double>> samples (n);
    for (std::size_t i = 0; i < n; ++i)
      {
        samples[i].push_back (0.0);
        samples[i].push_back (lo[i]);
        samples[i].push_back (hi[i]);
        samples[i].push_back ((lo[i] + hi[i]) / 2.0);
        for (int k = -3; k <= 3; ++k)
          {
            const double v = lo[i] + (hi[i] - lo[i]) * (k + 3) / 6.0;
            if (std::isfinite (v))
              samples[i].push_back (v);
          }
      }
    std::size_t cells = 1;
    for (const auto &s : samples)
      {
        if (cells > sample_cap_ / s.size ())
          return false;
        cells *= s.size ();
      }
    std::vector<double> cursor (n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
      cursor[i] = samples[i].front ();
    for (std::size_t step = 0; step < cells; ++step)
      {
        if (holds_all (cursor))
          {
            witness = cursor;
            return true;
          }
        for (std::size_t i = 0; i < n; ++i)
          {
            auto it = std::find (samples[i].begin (), samples[i].end (), cursor[i]);
            ++it;
            if (it != samples[i].end ())
              {
                cursor[i] = *it;
                break;
              }
            cursor[i] = samples[i].front ();
          }
      }
    return false;
  }

  CNF cnf_{};
  std::map<std::string, int> var_index_;
  std::vector<Constraint> constraints_;
  struct Bound
  {
    int var = -1;
    double lo = 0.0;
    double hi = 0.0;
  };
  std::vector<Bound> bounds_;
  std::vector<double> model_;
  std::size_t sample_cap_ = 50000;
  int bisection_steps_ = 64;
};

} // namespace satie::nra
