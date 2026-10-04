#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <queue>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"

namespace satie::ode
{

/// Scalar initial-value problems with safety queries (sound; complete
/// within the subdivision cap).
///
/// Dynamics are affine (`y' = a*y + b`) with an analytic closed form, or
/// polynomial (`y' = p(y)`) integrated by validated Euler with a Lipschitz
/// remainder. Parameters (`a`, `b`, `y0`, polynomial coefficients) are
/// existentially quantified over given ranges (degenerate ranges pin them).
/// The query asks whether some parameter choice steers `y(horizon)` into
/// `[target_lo, target_hi]`. Interval branch-and-bound prunes boxes whose
/// enclosure misses the target and accepts boxes enclosed in it; exhausting
/// the queue proves UNSAT, hitting the iteration cap yields UNKNOWN.
class ODESolver
{
public:
  ODESolver () = default;
  explicit ODESolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "ODE";
  }

  /// `load` replaces the Boolean problem; ODE systems are preserved. Use
  /// `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    systems_.clear ();
    model_.clear ();
  }

  void set_iteration_cap (std::size_t cap) { iteration_cap_ = cap == 0 ? 1 : cap; }
  void set_precision (double precision)
  {
    precision_ = precision <= 0.0 ? 1e-9 : precision;
  }

  struct Range
  {
    double lo = 0.0;
    double hi = 0.0;
  };

  /// Affine system `y' = a*y + b`, `y(0) = y0`, safety at `horizon`.
  int add_affine (Range a, Range b, Range y0, double horizon, Range target)
  {
    require_range (a);
    require_range (b);
    require_range (y0);
    if (!(horizon >= 0.0) || !std::isfinite (horizon))
      throw std::invalid_argument ("ODE horizon must be finite and non-negative");
    require_range (target);
    const int id = static_cast<int> (systems_.size ());
    systems_.push_back ({ true, a, b, y0, {}, horizon, target });
    return id;
  }

  /// Polynomial system `y' = sum(coeff[k] * y^k)`, `y(0) = y0`.
  int add_poly (const std::vector<Range> &coeffs, Range y0, double horizon,
                Range target, std::size_t euler_steps = 200)
  {
    if (coeffs.empty ())
      throw std::invalid_argument ("ODE polynomial needs at least one coefficient");
    for (const Range &r : coeffs)
      require_range (r);
    require_range (y0);
    if (!(horizon >= 0.0) || !std::isfinite (horizon))
      throw std::invalid_argument ("ODE horizon must be finite and non-negative");
    require_range (target);
    const int id = static_cast<int> (systems_.size ());
    systems_.push_back ({ false, { 0.0, 0.0 }, { 0.0, 0.0 }, y0, coeffs, horizon,
                          target, euler_steps });
    return id;
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    model_.clear ();
    for (const System &system : systems_)
      {
        std::vector<double> witness;
        const Verdict verdict = decide_system (system, witness);
        if (verdict == Verdict::Unsat)
          return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
        if (verdict == Verdict::Unknown)
          return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
        model_.push_back (std::move (witness));
      }
    return { SolveStatus::SAT, boolean.assignment };
  }

  /// Witness parameters of system `id` (layout below).
  const std::vector<double> &system_model (int id) const
  {
    if (id < 0 || id >= static_cast<int> (model_.size ()))
      throw std::invalid_argument ("unknown ODE system id");
    return model_[static_cast<std::size_t> (id)];
  }

private:
  struct System
  {
    bool affine = true;
    Range a{ 0.0, 0.0 };
    Range b{ 0.0, 0.0 };
    Range y0{ 0.0, 0.0 };
    std::vector<Range> coeffs;
    double horizon = 0.0;
    Range target{ 0.0, 0.0 };
    std::size_t euler_steps = 200;
  };

  enum class Verdict
  {
    Sat,
    Unsat,
    Unknown
  };

  static void require_range (const Range &range)
  {
    if (!(range.lo <= range.hi) || !std::isfinite (range.lo) ||
        !std::isfinite (range.hi))
      throw std::invalid_argument ("ODE range must be finite with lo <= hi");
  }

  struct Interval
  {
    double lo = 0.0;
    double hi = 0.0;
  };

  static Interval add_iv (Interval a, Interval b) { return { a.lo + b.lo, a.hi + b.hi }; }
  static Interval mul_iv (Interval a, Interval b)
  {
    const double p1 = a.lo * b.lo, p2 = a.lo * b.hi;
    const double p3 = a.hi * b.lo, p4 = a.hi * b.hi;
    return { std::min (std::min (p1, p2), std::min (p3, p4)),
             std::max (std::max (p1, p2), std::max (p3, p4)) };
  }
  // NOTE: `exp_iv` over-approximates with endpoint evaluation, valid because
  // exp is monotone.
  static Interval exp_iv (Interval a) { return { std::exp (a.lo), std::exp (a.hi) }; }

  /// Closed-form enclosure of the affine flow over the parameter box.
  static Interval affine_enclosure (const System &system, Interval a, Interval y0,
                                   Interval b)
  {
    const double t = system.horizon;
    if (a.lo == 0.0 && a.hi == 0.0)
      return add_iv (y0, { b.lo * t, b.hi * t });
    // y(t) = (y0 + b/a) * e^{a t} - b/a. Evaluated with interval
    // arithmetic; division by an `a` range straddling zero widens to the
    // whole line (handled by the caller splitting the box).
    if (a.lo <= 0.0 && a.hi >= 0.0)
      return { -std::numeric_limits<double>::infinity (),
               std::numeric_limits<double>::infinity () };
    Interval a_safe = a.lo > 0.0 ? a : Interval{ -a.hi, -a.lo };
    (void)a_safe;
    Interval inv_a{ 1.0 / a.hi, 1.0 / a.lo }; // a does not straddle zero here
    Interval b_over_a = mul_iv (b, inv_a);
    Interval base = add_iv (y0, b_over_a);
    Interval growth = exp_iv ({ a.lo * t, a.hi * t });
    Interval grown = mul_iv (base, growth);
    return { grown.lo - b_over_a.hi, grown.hi - b_over_a.lo };
  }

  /// Validated Euler enclosure for polynomial dynamics over the box.
  static Interval poly_enclosure (const System &system, const std::vector<Interval> &coeffs,
                                 Interval y0)
  {
    const std::size_t steps = std::max<std::size_t> (system.euler_steps, 1);
    const double h = system.horizon / static_cast<double> (steps);
    Interval y = y0;
    for (std::size_t s = 0; s < steps; ++s)
      {
        // f enclosure over the current box.
        Interval f{ 0.0, 0.0 };
        Interval power{ 1.0, 1.0 };
        for (const Interval &c : coeffs)
          {
            f = add_iv (f, mul_iv (c, power));
            power = mul_iv (power, y);
          }
        // Lipschitz-based remainder: |R| <= (M h^2 / 2) e^{L h} with L, M
        // bounded on the current box (derivative of a polynomial).
        Interval deriv{ 0.0, 0.0 };
        Interval dpower{ 1.0, 1.0 };
        for (std::size_t k = 1; k < coeffs.size (); ++k)
          {
            Interval term = mul_iv ({ static_cast<double> (k), static_cast<double> (k) },
                                     coeffs[k]);
            deriv = add_iv (deriv, mul_iv (term, dpower));
            dpower = mul_iv (dpower, y);
          }
        const double lipschitz =
            std::max (std::fabs (deriv.lo), std::fabs (deriv.hi));
        const double magnitude = std::max (std::fabs (f.lo), std::fabs (f.hi));
        const double remainder =
            (magnitude * h * h / 2.0) * std::exp (lipschitz * h) + 1e-12;
        Interval next{ y.lo + h * f.lo - remainder, y.hi + h * f.hi + remainder };
        if (!std::isfinite (next.lo) || !std::isfinite (next.hi))
          return { -std::numeric_limits<double>::infinity (),
                   std::numeric_limits<double>::infinity () };
        y = next;
      }
    return y;
  }

  Verdict decide_system (const System &system, std::vector<double> &witness) const
  {
    // Parameter box: affine has 3 params (a, b, y0), poly has coeffs + y0.
    struct Box
    {
      std::vector<Interval> params;
    };
    Box root;
    if (system.affine)
      root.params = { { system.a.lo, system.a.hi },
                      { system.b.lo, system.b.hi },
                      { system.y0.lo, system.y0.hi } };
    else
      {
        for (const Range &c : system.coeffs)
          root.params.push_back ({ c.lo, c.hi });
        root.params.push_back ({ system.y0.lo, system.y0.hi });
      }
    auto enclosure = [&] (const Box &box) {
      if (system.affine)
        return affine_enclosure (system, box.params[0], box.params[2], box.params[1]);
      std::vector<Interval> coeffs (box.params.begin (), box.params.end () - 1);
      return poly_enclosure (system, coeffs, box.params.back ());
    };
    auto inside_target = [&] (const Interval &iv) {
      return iv.lo >= system.target.lo && iv.hi <= system.target.hi;
    };
    auto misses_target = [&] (const Interval &iv) {
      return iv.hi < system.target.lo || iv.lo > system.target.hi;
    };
    auto midpoint_witness = [&] (const Box &box) {
      std::vector<double> point;
      for (const Interval &iv : box.params)
        point.push_back ((iv.lo + iv.hi) / 2.0);
      return point;
    };
    auto validate_point = [&] (const std::vector<double> &point) {
      Box singleton;
      for (double v : point)
        singleton.params.push_back ({ v, v });
      const Interval iv = enclosure (singleton);
      // A point validates when its (near-degenerate) enclosure meets the
      // target; midpoint rounding is absorbed by a small epsilon.
      return !(iv.hi < system.target.lo - 1e-9 || iv.lo > system.target.hi + 1e-9);
    };

    std::vector<Box> stack{ root };
    std::size_t iterations = 0;
    while (!stack.empty ())
      {
        if (++iterations > iteration_cap_)
          return Verdict::Unknown;
        Box box = std::move (stack.back ());
        stack.pop_back ();
        const Interval iv = enclosure (box);
        if (misses_target (iv))
          continue;
        if (inside_target (iv))
          {
            std::vector<double> point = midpoint_witness (box);
            if (validate_point (point))
              {
                witness = std::move (point);
                return Verdict::Sat;
              }
          }
        // Split the widest parameter.
        std::size_t widest = 0;
        double width = -1.0;
        for (std::size_t i = 0; i < box.params.size (); ++i)
          {
            const double w = box.params[i].hi - box.params[i].lo;
            if (w > width)
              {
                width = w;
                widest = i;
              }
          }
        if (!(width > precision_))
          {
            std::vector<double> point = midpoint_witness (box);
            if (validate_point (point))
              {
                witness = std::move (point);
                return Verdict::Sat;
              }
            continue;
          }
        Box left = box, right = box;
        const double mid = (box.params[widest].lo + box.params[widest].hi) / 2.0;
        left.params[widest].hi = mid;
        right.params[widest].lo = mid;
        stack.push_back (std::move (left));
        stack.push_back (std::move (right));
      }
    return Verdict::Unsat;
  }

  CNF cnf_{};
  std::vector<System> systems_;
  std::vector<std::vector<double>> model_;
  std::size_t iteration_cap_ = 20000;
  double precision_ = 1e-6;
};

} // namespace satie::ode
