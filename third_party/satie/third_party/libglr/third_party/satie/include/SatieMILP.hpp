#pragma once

#include <cmath>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"
#include "SatieLP.hpp"

namespace satie::milp
{

/// Mixed-integer linear programming (sound; complete within the node cap).
///
/// Rational constraints are decided by the LP solver; integer variables are
/// branched on (floor/ceil split) depth-first. A leaf whose relaxation model
/// rounds to integers (within epsilon, revalidated) yields SAT. Exhausting
/// the search tree without a model proves UNSAT; hitting the node or depth
/// cap degrades honestly to UNKNOWN.
class MILPSolver
{
public:
  MILPSolver () = default;
  explicit MILPSolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "MILP";
  }

  /// `load` replaces the Boolean problem; linear constraints are preserved.
  /// Use `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    var_index_.clear ();
    integer_.clear ();
    constraints_.clear ();
    model_.clear ();
  }

  void set_node_cap (std::size_t cap) { node_cap_ = cap == 0 ? 1 : cap; }
  void set_depth_cap (std::size_t cap) { depth_cap_ = cap == 0 ? 1 : cap; }

  int add_var (const std::string &name, bool integer = false)
  {
    auto it = var_index_.find (name);
    if (it != var_index_.end ())
      {
        if (integer)
          integer_.push_back (it->second);
        return it->second;
      }
    const int id = static_cast<int> (var_index_.size ());
    var_index_.emplace (name, id);
    if (integer)
      integer_.push_back (id);
    return id;
  }

  /// Asserts `sum(coeff[var] * var) <= bound`.
  void add_le (const std::vector<std::pair<int, double>> &coeffs, double bound)
  {
    require_coeffs (coeffs);
    constraints_.push_back ({ coeffs, bound });
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    model_.clear ();
    nodes_ = 0;
    saw_unknown_ = false;
    std::vector<Row> root = constraints_;
    if (branch (root, 0))
      return { SolveStatus::SAT, boolean.assignment };
    if (saw_unknown_)
      return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
    return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
  }

  double value (int var) const
  {
    require_var (var);
    if (model_.empty ())
      throw std::logic_error ("MILP model is unavailable before a SAT check");
    return model_[static_cast<std::size_t> (var)];
  }

private:
  struct Row
  {
    std::vector<std::pair<int, double>> coeffs;
    double bound = 0.0;
  };

  static constexpr double kEps = 1e-6;

  void require_var (int var) const
  {
    if (var < 0 || var >= static_cast<int> (var_index_.size ()))
      throw std::invalid_argument ("unknown MILP variable id");
  }

  void require_coeffs (const std::vector<std::pair<int, double>> &coeffs) const
  {
    for (const auto &[var, coeff] : coeffs)
      {
        require_var (var);
        (void)coeff;
      }
  }

  enum class Relax
  {
    Feasible,
    Infeasible,
    Unknown
  };

  Relax relax (const std::vector<Row> &rows, std::vector<double> &model) const
  {
    lp::LPSolver lp;
    for (std::size_t i = 0; i < var_index_.size (); ++i)
      lp.add_var ("x" + std::to_string (i));
    for (const Row &row : rows)
      lp.add_le (row.coeffs, row.bound);
    SolveResult result = lp.check ();
    if (result.unsatisfiable ())
      return Relax::Infeasible;
    if (!result.satisfiable ())
      return Relax::Unknown;
    model.assign (var_index_.size (), 0.0);
    for (std::size_t i = 0; i < var_index_.size (); ++i)
      model[i] = lp.value (static_cast<int> (i));
    return Relax::Feasible;
  }

  bool validate (const std::vector<double> &model, const std::vector<Row> &rows) const
  {
    for (const Row &row : rows)
      {
        double sum = 0.0;
        for (const auto &[var, coeff] : row.coeffs)
          sum += coeff * model[static_cast<std::size_t> (var)];
        if (sum > row.bound + 1e-6)
          return false;
      }
    return true;
  }

  bool branch (const std::vector<Row> &rows, std::size_t depth)
  {
    if (++nodes_ > node_cap_ || depth > depth_cap_)
      {
        saw_unknown_ = true;
        return false;
      }
    std::vector<double> relaxed;
    switch (relax (rows, relaxed))
      {
      case Relax::Infeasible:
        return false;
      case Relax::Unknown:
        saw_unknown_ = true;
        return false;
      case Relax::Feasible:
        break;
      }
    int split = -1;
    double split_value = 0.0;
    double worst = 0.0;
    for (int var : integer_)
      {
        const double v = relaxed[static_cast<std::size_t> (var)];
        const double frac = std::fabs (v - std::round (v));
        if (frac > kEps && frac > worst)
          {
            worst = frac;
            split = var;
            split_value = v;
          }
      }
    if (split < 0)
      {
        for (std::size_t i = 0; i < relaxed.size (); ++i)
          relaxed[i] = std::round (relaxed[i] * 1e9) / 1e9;
        // Snap integer variables exactly, then revalidate everything.
        for (int var : integer_)
          relaxed[static_cast<std::size_t> (var)] = std::round (relaxed[static_cast<std::size_t> (var)]);
        if (!validate (relaxed, rows))
          {
            saw_unknown_ = true;
            return false;
          }
        model_ = relaxed;
        return true;
      }
    const double floor = std::floor (split_value);
    std::vector<Row> lo = rows;
    lo.push_back ({ { { split, 1.0 } }, floor });
    if (branch (lo, depth + 1))
      return true;
    std::vector<Row> hi = rows;
    hi.push_back ({ { { split, -1.0 } }, -floor - 1.0 });
    return branch (hi, depth + 1);
  }

  CNF cnf_{};
  std::map<std::string, int> var_index_;
  std::vector<int> integer_;
  std::vector<Row> constraints_;
  std::vector<double> model_;
  std::size_t node_cap_ = 2000;
  std::size_t depth_cap_ = 64;
  std::size_t nodes_ = 0;
  bool saw_unknown_ = false;
};

} // namespace satie::milp
