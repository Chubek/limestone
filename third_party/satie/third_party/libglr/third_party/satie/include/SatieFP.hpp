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

namespace satie::fp
{

/// IEEE-754 double constraints with round-to-nearest (sound; complete for
/// structural contradictions and unary bounds, local search otherwise).
///
/// Expressions are variables, constants (including infinities and NaN), and
/// `+ - * / sqrt neg`. Comparisons use IEEE semantics (NaN poisons every
/// comparison except `!=`). `check` layers: Boolean filter, constant
/// folding, same-expression contradictions (`e < e`), per-variable intervals
/// from unary comparisons against constants, then deterministic coordinate
/// descent over a candidate lattice. SAT answers carry validated models;
/// inconclusive searches yield UNKNOWN. Only round-to-nearest is modeled.
class FPSolver
{
public:
  FPSolver () = default;
  explicit FPSolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "FP";
  }

  /// `load` replaces the Boolean problem; FP constraints are preserved.
  /// Use `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    var_index_.clear ();
    exprs_.clear ();
    comparisons_.clear ();
    model_.clear ();
  }

  void set_search_rounds (std::size_t rounds) { search_rounds_ = rounds; }

  enum class Op
  {
    Const,
    Var,
    Neg,
    Add,
    Sub,
    Mul,
    Div,
    Sqrt
  };

  struct Expr
  {
    Op op = Op::Const;
    double value = 0.0; // Const, or Var (variable id stored below)
    int var = -1;
    int lhs = -1;
    int rhs = -1;
  };

  enum class Cmp
  {
    Lt,
    Le,
    Eq,
    Ne,
    Ge,
    Gt
  };

  int add_var (const std::string &name)
  {
    auto it = var_index_.find (name);
    if (it != var_index_.end ())
      return it->second;
    const int id = static_cast<int> (var_index_.size ());
    var_index_.emplace (name, id);
    return id;
  }

  int mk_const (double value)
  {
    const int id = static_cast<int> (exprs_.size ());
    exprs_.push_back ({ Op::Const, value, -1, -1, -1 });
    return id;
  }

  int mk_var (int var)
  {
    require_var (var);
    const int id = static_cast<int> (exprs_.size ());
    exprs_.push_back ({ Op::Var, 0.0, var, -1, -1 });
    return id;
  }

  int mk_unary (Op op, int operand)
  {
    require_expr (operand);
    if (op != Op::Neg && op != Op::Sqrt)
      throw std::invalid_argument ("FP unary operator must be neg or sqrt");
    const int id = static_cast<int> (exprs_.size ());
    exprs_.push_back ({ op, 0.0, -1, operand, -1 });
    return id;
  }

  int mk_binary (Op op, int lhs, int rhs)
  {
    require_expr (lhs);
    require_expr (rhs);
    if (op != Op::Add && op != Op::Sub && op != Op::Mul && op != Op::Div)
      throw std::invalid_argument ("FP binary operator must be add/sub/mul/div");
    const int id = static_cast<int> (exprs_.size ());
    exprs_.push_back ({ op, 0.0, -1, lhs, rhs });
    return id;
  }

  void add_cmp (Cmp cmp, int lhs, int rhs)
  {
    require_expr (lhs);
    require_expr (rhs);
    comparisons_.push_back ({ cmp, lhs, rhs });
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    model_.clear ();
    if (structurally_unsat ())
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    if (!unary_intervals_consistent ())
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    std::vector<double> witness;
    if (local_search (witness))
      {
        model_ = witness;
        return { SolveStatus::SAT, boolean.assignment };
      }
    return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
  }

  double value (int var) const
  {
    require_var (var);
    if (model_.empty () && !var_index_.empty ())
      throw std::logic_error ("FP model is unavailable before a SAT check");
    if (model_.empty ())
      return 0.0;
    return model_[static_cast<std::size_t> (var)];
  }

private:
  struct Comparison
  {
    Cmp cmp = Cmp::Eq;
    int lhs = -1;
    int rhs = -1;
  };

  void require_var (int var) const
  {
    if (var < 0 || var >= static_cast<int> (var_index_.size ()))
      throw std::invalid_argument ("unknown FP variable id");
  }

  void require_expr (int expr) const
  {
    if (expr < 0 || expr >= static_cast<int> (exprs_.size ()))
      throw std::invalid_argument ("unknown FP expression id");
  }

  double evaluate (int expr, const std::vector<double> &values) const
  {
    const Expr &node = exprs_[static_cast<std::size_t> (expr)];
    switch (node.op)
      {
      case Op::Const:
        return node.value;
      case Op::Var:
        return values[static_cast<std::size_t> (node.var)];
      case Op::Neg:
        return -evaluate (node.lhs, values);
      case Op::Add:
        return evaluate (node.lhs, values) + evaluate (node.rhs, values);
      case Op::Sub:
        return evaluate (node.lhs, values) - evaluate (node.rhs, values);
      case Op::Mul:
        return evaluate (node.lhs, values) * evaluate (node.rhs, values);
      case Op::Div:
        return evaluate (node.lhs, values) / evaluate (node.rhs, values);
      case Op::Sqrt:
        return std::sqrt (evaluate (node.lhs, values));
      }
    return std::numeric_limits<double>::quiet_NaN ();
  }

  static bool holds_cmp (Cmp cmp, double a, double b)
  {
    // IEEE: any comparison with NaN is false, except `!=` which is true.
    if (std::isnan (a) || std::isnan (b))
      return cmp == Cmp::Ne;
    switch (cmp)
      {
      case Cmp::Lt:
        return a < b;
      case Cmp::Le:
        return a <= b;
      case Cmp::Eq:
        return a == b;
      case Cmp::Ne:
        return a != b;
      case Cmp::Ge:
        return a >= b;
      case Cmp::Gt:
        return a > b;
      }
    return false;
  }

  bool holds_all (const std::vector<double> &values) const
  {
    for (const Comparison &c : comparisons_)
      if (!holds_cmp (c.cmp, evaluate (c.lhs, values), evaluate (c.rhs, values)))
        return false;
    return true;
  }

  bool is_const_expr (int expr, double &value) const
  {
    // Structural constants only (no evaluation through variables).
    const Expr &node = exprs_[static_cast<std::size_t> (expr)];
    if (node.op == Op::Const)
      {
        value = node.value;
        return true;
      }
    return false;
  }

  bool structurally_unsat () const
  {
    std::vector<double> empty;
    for (const Comparison &c : comparisons_)
      {
        double a = 0.0, b = 0.0;
        const bool a_const = is_const_expr (c.lhs, a);
        const bool b_const = is_const_expr (c.rhs, b);
        if (a_const && b_const && !holds_cmp (c.cmp, a, b))
          return true;
        if (c.lhs == c.rhs && (c.cmp == Cmp::Lt || c.cmp == Cmp::Gt))
          return true; // e < e (or e > e) is false for every e, NaN included.
        // Same-expression bound contradictions: e < k and e >= k, etc.
        for (const Comparison &d : comparisons_)
          {
            if (d.lhs != c.lhs || d.rhs != c.rhs || d.cmp == c.cmp)
              continue;
            double k1 = 0.0, k2 = 0.0;
            if (!is_const_expr (c.rhs, k1) || !is_const_expr (d.rhs, k2) || k1 != k2)
              continue;
            // Opposite strict/non-strict pairs over the same bound clash.
            const bool clash =
                (c.cmp == Cmp::Lt && (d.cmp == Cmp::Ge || d.cmp == Cmp::Gt)) ||
                (c.cmp == Cmp::Le && d.cmp == Cmp::Gt) ||
                (c.cmp == Cmp::Gt && (d.cmp == Cmp::Le || d.cmp == Cmp::Lt)) ||
                (c.cmp == Cmp::Ge && d.cmp == Cmp::Lt);
            if (clash)
              return true;
          }
      }
    (void)empty;
    return false;
  }

  /// Interval analysis from unary comparisons `x op constant`.
  bool unary_intervals_consistent () const
  {
    const std::size_t n = var_index_.size ();
    std::vector<double> lo (n, -std::numeric_limits<double>::infinity ());
    std::vector<double> hi (n, std::numeric_limits<double>::infinity ());
    for (const Comparison &c : comparisons_)
      {
        int var = -1;
        double bound = 0.0;
        bool flipped = false;
        const Expr &l = exprs_[static_cast<std::size_t> (c.lhs)];
        const Expr &r = exprs_[static_cast<std::size_t> (c.rhs)];
        if (l.op == Op::Var && is_const_expr (c.rhs, bound))
          var = l.var;
        else if (r.op == Op::Var && is_const_expr (c.lhs, bound))
          {
            var = r.var;
            flipped = true;
          }
        else if (l.op == Op::Neg && exprs_[static_cast<std::size_t> (l.lhs)].op == Op::Var &&
                 is_const_expr (c.rhs, bound))
          {
            // -x op k  <=>  x op' -k.
            var = exprs_[static_cast<std::size_t> (l.lhs)].var;
            bound = -bound;
            flipped = true;
          }
        else
          continue;
        Cmp cmp = c.cmp;
        if (flipped)
          {
            switch (cmp)
              {
              case Cmp::Lt:
                cmp = Cmp::Gt;
                break;
              case Cmp::Le:
                cmp = Cmp::Ge;
                break;
              case Cmp::Gt:
                cmp = Cmp::Lt;
                break;
              case Cmp::Ge:
                cmp = Cmp::Le;
                break;
              default:
                break;
              }
          }
        if (std::isnan (bound) && cmp != Cmp::Ne)
          return false; // x < NaN (etc.) is unsatisfiable.
        auto tighten = [&] (bool lower, double edge, bool strict) {
          if (strict)
            edge = std::nextafter (edge, lower ? std::numeric_limits<double>::infinity ()
                                               : -std::numeric_limits<double>::infinity ());
          if (lower)
            lo[static_cast<std::size_t> (var)] =
                std::max (lo[static_cast<std::size_t> (var)], edge);
          else
            hi[static_cast<std::size_t> (var)] =
                std::min (hi[static_cast<std::size_t> (var)], edge);
        };
        switch (cmp)
          {
          case Cmp::Lt:
            tighten (false, bound, true);
            break;
          case Cmp::Le:
            tighten (false, bound, false);
            break;
          case Cmp::Gt:
            tighten (true, bound, true);
            break;
          case Cmp::Ge:
            tighten (true, bound, false);
            break;
          case Cmp::Eq:
            tighten (true, bound, false);
            tighten (false, bound, false);
            break;
          case Cmp::Ne:
            break;
          }
      }
    for (std::size_t i = 0; i < n; ++i)
      if (lo[i] > hi[i])
        return false;
    return true;
  }

  /// Deterministic coordinate descent over a candidate lattice.
  bool local_search (std::vector<double> &witness)
  {
    const std::size_t n = var_index_.size ();
    const double inf = std::numeric_limits<double>::infinity ();
    const double nan = std::numeric_limits<double>::quiet_NaN ();
    std::vector<double> lattice{ 0.0, 1.0, -1.0, 0.5, -0.5, 2.0, -2.0,
                                 10.0, inf, -inf, nan };
    for (const Expr &node : exprs_)
      if (node.op == Op::Const && std::isfinite (node.value))
        lattice.push_back (node.value);
    std::vector<double> current (n, 0.0);
    auto satisfied_count = [&] (const std::vector<double> &values) {
      std::size_t count = 0;
      for (const Comparison &c : comparisons_)
        if (holds_cmp (c.cmp, evaluate (c.lhs, values), evaluate (c.rhs, values)))
          ++count;
      return count;
    };
    if (holds_all (current))
      {
        witness = current;
        return true;
      }
    for (std::size_t round = 0; round < search_rounds_; ++round)
      {
        bool improved = false;
        for (std::size_t v = 0; v < n; ++v)
          {
            const std::size_t before = satisfied_count (current);
            double best = current[v];
            std::size_t best_count = before;
            for (double candidate : lattice)
              {
                current[v] = candidate;
                const std::size_t scored = satisfied_count (current);
                if (scored > best_count)
                  {
                    best = candidate;
                    best_count = scored;
                  }
              }
            // Hill-climb around the best lattice point.
            current[v] = best;
            for (double step : { 0.25, 0.125 })
              for (double direction : { -1.0, 1.0 })
                {
                  current[v] = best + direction * step;
                  const std::size_t scored = satisfied_count (current);
                  if (scored > best_count)
                    {
                      best = current[v];
                      best_count = scored;
                    }
                }
            current[v] = best;
            if (best_count > before)
              improved = true;
            if (holds_all (current))
              {
                witness = current;
                return true;
              }
          }
        if (!improved)
          break;
      }
    return false;
  }

  CNF cnf_{};
  std::map<std::string, int> var_index_;
  std::vector<Expr> exprs_;
  std::vector<Comparison> comparisons_;
  std::vector<double> model_;
  std::size_t search_rounds_ = 64;
};

} // namespace satie::fp
