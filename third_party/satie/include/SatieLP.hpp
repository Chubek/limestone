#pragma once

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

namespace satie::lp
{

/// Linear programming feasibility over the rationals (complete, via
/// Fourier-Motzkin elimination).
///
/// Constraints are `sum(coeff[i] * x[i]) <= bound`. `check` first runs the
/// Boolean solver as a layered filter, then eliminates variables pairwise.
/// The elimination is exact over the rationals; floating-point rounding is
/// guarded by an epsilon, and any numeric failure degrades honestly to
/// UNKNOWN instead of a wrong answer. Row blowup is capped
/// (`set_row_cap`); exceeding it also yields UNKNOWN. Models are recovered
/// by back-substitution.
class LPSolver
{
public:
  LPSolver () = default;
  explicit LPSolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "LP";
  }

  /// `load` replaces the Boolean problem; linear constraints are preserved.
  /// Use `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    var_index_.clear ();
    constraints_.clear ();
    model_.clear ();
  }

  void set_row_cap (std::size_t cap) { row_cap_ = cap == 0 ? 1 : cap; }

  int add_var (const std::string &name)
  {
    auto it = var_index_.find (name);
    if (it != var_index_.end ())
      return it->second;
    const int id = static_cast<int> (var_index_.size ());
    var_index_.emplace (name, id);
    return id;
  }

  /// Asserts `sum(coeff[var] * var) <= bound`.
  void add_le (const std::vector<std::pair<int, double>> &coeffs, double bound)
  {
    require_coeffs (coeffs);
    Row row;
    row.coeff.assign (var_index_.size (), 0.0);
    for (const auto &[var, coeff] : coeffs)
      row.coeff[static_cast<std::size_t> (var)] = coeff;
    row.bound = bound;
    constraints_.push_back (std::move (row));
  }

  void add_eq (const std::vector<std::pair<int, double>> &coeffs, double bound)
  {
    add_le (coeffs, bound);
    std::vector<std::pair<int, double>> neg = coeffs;
    for (auto &[var, coeff] : neg)
      coeff = -coeff;
    add_le (neg, -bound);
  }

  void add_bound (int var, double lo, double hi)
  {
    require_var (var);
    add_le ({ { var, 1.0 } }, hi);
    add_le ({ { var, -1.0 } }, -lo);
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    model_.clear ();
    const Status status = eliminate ();
    if (status == Status::Infeasible)
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    if (status == Status::Unknown || !reconstruct ())
      return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
    if (!validate_model ())
      return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
    return { SolveStatus::SAT, boolean.assignment };
  }

  double value (int var) const
  {
    require_var (var);
    if (model_.empty ())
      throw std::logic_error ("LP model is unavailable before a SAT check");
    return model_[static_cast<std::size_t> (var)];
  }

  std::size_t variable_count () const { return var_index_.size (); }

protected:
  struct Row
  {
    std::vector<double> coeff;
    double bound = 0.0;
  };

  static constexpr double kEps = 1e-9;

  void require_var (int var) const
  {
    if (var < 0 || var >= static_cast<int> (var_index_.size ()))
      throw std::invalid_argument ("unknown LP variable id");
  }

  void require_coeffs (const std::vector<std::pair<int, double>> &coeffs) const
  {
    for (const auto &[var, coeff] : coeffs)
      {
        require_var (var);
        (void)coeff;
      }
  }

  enum class Status
  {
    Feasible,
    Infeasible,
    Unknown
  };

  struct Bound
  {
    std::vector<double> coeff; // over not-yet-eliminated variables only
    double rhs = 0.0;
  };

  /// Eliminates all variables, recording per-variable bounds. The row set is
  /// kept over the full variable list with eliminated entries zeroed.
  Status eliminate ()
  {
    const std::size_t n = var_index_.size ();
    std::vector<Row> rows = constraints_;
    for (Row &row : rows)
      row.coeff.resize (n, 0.0);
    lowers_.assign (n, {});
    uppers_.assign (n, {});

    for (std::size_t k = 0; k < n; ++k)
      {
        std::vector<Row> pos, neg, zero;
        for (Row &row : rows)
          {
            const double c = row.coeff[k];
            if (c > kEps)
              pos.push_back (std::move (row));
            else if (c < -kEps)
              neg.push_back (std::move (row));
            else
              {
                row.coeff[k] = 0.0;
                zero.push_back (std::move (row));
              }
          }
        for (const Row &row : pos)
          uppers_[k].push_back (scaled_bound (row, k, row.coeff[k], false));
        for (const Row &row : neg)
          lowers_[k].push_back (scaled_bound (row, k, -row.coeff[k], true));
        rows = std::move (zero);
        for (const Row &p : pos)
          for (const Row &q : neg)
            {
              Row combined;
              combined.coeff.assign (n, 0.0);
              const double pk = p.coeff[k];
              const double qk = -q.coeff[k];
              for (std::size_t j = k + 1; j < n; ++j)
                combined.coeff[j] = p.coeff[j] / pk + q.coeff[j] / qk;
              combined.bound = p.bound / pk + q.bound / qk;
              rows.push_back (std::move (combined));
              if (rows.size () > row_cap_)
                return Status::Unknown;
            }
      }
    for (const Row &row : rows)
      {
        for (double c : row.coeff)
          if (std::fabs (c) > kEps)
            return Status::Unknown; // Numeric residue: refuse to guess.
        if (0.0 > row.bound + kEps)
          return Status::Infeasible;
      }
    return Status::Feasible;
  }

  bool reconstruct ()
  {
    const std::size_t n = var_index_.size ();
    model_.assign (n, 0.0);
    for (std::size_t k = n; k-- > 0;)
      {
        double lo = -std::numeric_limits<double>::infinity ();
        double hi = std::numeric_limits<double>::infinity ();
        for (const Bound &b : lowers_[k])
          lo = std::max (lo, evaluate_bound (b, k));
        for (const Bound &b : uppers_[k])
          hi = std::min (hi, evaluate_bound (b, k));
        if (lo > hi + 1e-7)
          return false;
        double pick = 0.0;
        if (lo == -std::numeric_limits<double>::infinity () &&
            hi == std::numeric_limits<double>::infinity ())
          pick = 0.0;
        else if (hi == std::numeric_limits<double>::infinity ())
          pick = lo;
        else if (lo == -std::numeric_limits<double>::infinity ())
          pick = hi;
        else
          pick = (lo + hi) / 2.0;
        model_[k] = pick;
      }
    return true;
  }

  bool validate_model () const
  {
    for (const Row &row : constraints_)
      {
        double sum = 0.0;
        for (std::size_t j = 0; j < model_.size (); ++j)
          sum += row.coeff[j] * model_[j];
        if (sum > row.bound + 1e-7)
          return false;
      }
    return true;
  }

  CNF cnf_{};
  std::map<std::string, int> var_index_;
  std::vector<Row> constraints_;
  std::vector<double> model_;
  std::size_t row_cap_ = 20000;

private:
  Bound scaled_bound (const Row &row, std::size_t k, double divisor,
                      bool lower) const
  {
    // Upper (`lower == false`): x_k <= (b - Σ a_j x_j) / d.
    // Lower (`lower == true`): x_k >= (Σ a_j x_j - b) / d with d = |a_k|.
    Bound bound;
    bound.coeff.assign (row.coeff.size (), 0.0);
    for (std::size_t j = k + 1; j < row.coeff.size (); ++j)
      bound.coeff[j] = (lower ? row.coeff[j] : -row.coeff[j]) / divisor;
    bound.rhs = (lower ? -row.bound : row.bound) / divisor;
    return bound;
  }

  double evaluate_bound (const Bound &bound, std::size_t k) const
  {
    double total = bound.rhs;
    for (std::size_t j = k + 1; j < model_.size (); ++j)
      total += bound.coeff[j] * model_[j];
    return total;
  }

  std::vector<std::vector<Bound>> lowers_;
  std::vector<std::vector<Bound>> uppers_;
};

} // namespace satie::lp
