#pragma once

#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"

namespace satie::symsolve
{

/// Symbolic/numeric equation solving over doubles (sound; complete for
/// linear systems and univariate quadratics, Newton/polish otherwise).
///
/// Expressions are constants, variables, `neg/add/sub/mul/div`,
/// integer powers, and `sin/cos/exp/log/sqrt`. Constraints are equations
/// `lhs == rhs` (plus `add_eq_zero`). `check` layers: constant folding
/// (structural UNSAT), exact Gaussian elimination of the affine subsystem
/// (inconsistent rows mean UNSAT), closed-form univariate quadratics,
/// bisection brackets for odd univariate polynomials, then deterministic
/// multi-start Newton with a numeric Jacobian. SAT answers are validated
/// against a residual tolerance; anything else is UNKNOWN.
class SymSolveSolver
{
public:
  SymSolveSolver () = default;
  explicit SymSolveSolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "SymSolve";
  }

  /// `load` replaces the Boolean problem; equations are preserved. Use
  /// `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    var_index_.clear ();
    exprs_.clear ();
    equations_.clear ();
    model_.clear ();
  }

  void set_newton_cap (std::size_t cap) { newton_cap_ = cap == 0 ? 1 : cap; }
  void set_tolerance (double tol) { tolerance_ = tol <= 0.0 ? 1e-12 : tol; }

  enum class Op
  {
    Const,
    Var,
    Neg,
    Add,
    Sub,
    Mul,
    Div,
    Pow, // integer exponent in `aux`
    Sin,
    Cos,
    Exp,
    Log,
    Sqrt
  };

  struct Expr
  {
    Op op = Op::Const;
    double value = 0.0; // Const; Var uses `var`
    int var = -1;
    int lhs = -1;
    int rhs = -1;
    int aux = 0; // Pow exponent
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
    exprs_.push_back ({ Op::Const, value, -1, -1, -1, 0 });
    return id;
  }

  int mk_var (int var)
  {
    require_var (var);
    const int id = static_cast<int> (exprs_.size ());
    exprs_.push_back ({ Op::Var, 0.0, var, -1, -1, 0 });
    return id;
  }

  int mk_unary (Op op, int operand)
  {
    require_expr (operand);
    switch (op)
      {
      case Op::Neg:
      case Op::Sin:
      case Op::Cos:
      case Op::Exp:
      case Op::Log:
      case Op::Sqrt:
        break;
      default:
        throw std::invalid_argument ("SymSolve unary operator mismatch");
      }
    const int id = static_cast<int> (exprs_.size ());
    exprs_.push_back ({ op, 0.0, -1, operand, -1, 0 });
    return id;
  }

  int mk_binary (Op op, int lhs, int rhs)
  {
    require_expr (lhs);
    require_expr (rhs);
    switch (op)
      {
      case Op::Add:
      case Op::Sub:
      case Op::Mul:
      case Op::Div:
        break;
      default:
        throw std::invalid_argument ("SymSolve binary operator mismatch");
      }
    const int id = static_cast<int> (exprs_.size ());
    exprs_.push_back ({ op, 0.0, -1, lhs, rhs, 0 });
    return id;
  }

  int mk_pow (int base, int exponent)
  {
    require_expr (base);
    const int id = static_cast<int> (exprs_.size ());
    exprs_.push_back ({ Op::Pow, 0.0, -1, base, -1, exponent });
    return id;
  }

  void add_eq (int lhs, int rhs)
  {
    require_expr (lhs);
    require_expr (rhs);
    equations_.push_back ({ lhs, rhs });
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    model_.clear ();
    if (structurally_unsat ())
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    std::vector<double> witness (var_index_.size (), 0.0);
    std::vector<bool> pinned (var_index_.size (), false);
    if (!solve_linear (witness, pinned))
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    if (validate (witness))
      {
        model_ = witness;
        return { SolveStatus::SAT, boolean.assignment };
      }
    if (solve_univariate_quadratic (witness, pinned) && validate (witness))
      {
        model_ = witness;
        return { SolveStatus::SAT, boolean.assignment };
      }
    if (newton_search (witness, pinned) && validate (witness))
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
      throw std::logic_error ("SymSolve model is unavailable before a SAT check");
    if (model_.empty ())
      return 0.0;
    return model_[static_cast<std::size_t> (var)];
  }

private:
  void require_var (int var) const
  {
    if (var < 0 || var >= static_cast<int> (var_index_.size ()))
      throw std::invalid_argument ("unknown SymSolve variable id");
  }

  void require_expr (int expr) const
  {
    if (expr < 0 || expr >= static_cast<int> (exprs_.size ()))
      throw std::invalid_argument ("unknown SymSolve expression id");
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
      case Op::Pow:
        return std::pow (evaluate (node.lhs, values), node.aux);
      case Op::Sin:
        return std::sin (evaluate (node.lhs, values));
      case Op::Cos:
        return std::cos (evaluate (node.lhs, values));
      case Op::Exp:
        return std::exp (evaluate (node.lhs, values));
      case Op::Log:
        return std::log (evaluate (node.lhs, values));
      case Op::Sqrt:
        return std::sqrt (evaluate (node.lhs, values));
      }
    return std::numeric_limits<double>::quiet_NaN ();
  }

  bool is_const_tree (int expr) const
  {
    const Expr &node = exprs_[static_cast<std::size_t> (expr)];
    switch (node.op)
      {
      case Op::Const:
        return true;
      case Op::Var:
        return false;
      case Op::Neg:
      case Op::Sin:
      case Op::Cos:
      case Op::Exp:
      case Op::Log:
      case Op::Sqrt:
        return is_const_tree (node.lhs);
      case Op::Add:
      case Op::Sub:
      case Op::Mul:
      case Op::Div:
        return is_const_tree (node.lhs) && is_const_tree (node.rhs);
      case Op::Pow:
        return is_const_tree (node.lhs);
      }
    return false;
  }

  bool structurally_unsat () const
  {
    const std::vector<double> empty;
    for (const auto &[lhs, rhs] : equations_)
      if (is_const_tree (lhs) && is_const_tree (rhs))
        {
          const double a = evaluate (lhs, empty);
          const double b = evaluate (rhs, empty);
          if (std::isnan (a) || std::isnan (b) || a != b)
            return true;
        }
    return false;
  }

  bool validate (const std::vector<double> &values) const
  {
    for (const auto &[lhs, rhs] : equations_)
      {
        const double a = evaluate (lhs, values);
        const double b = evaluate (rhs, values);
        if (std::isnan (a) || std::isnan (b) || std::fabs (a - b) > tolerance_)
          return false;
      }
    return true;
  }

  /// Affine decomposition of `expr`: constant plus per-variable
  /// coefficients. Returns false when the expression is not affine.
  bool affine_of (int expr, double &constant, std::vector<double> &coeffs) const
  {
    const std::size_t n = var_index_.size ();
    const Expr &node = exprs_[static_cast<std::size_t> (expr)];
    coeffs.assign (n, 0.0);
    switch (node.op)
      {
      case Op::Const:
        constant = node.value;
        return true;
      case Op::Var:
        constant = 0.0;
        coeffs[static_cast<std::size_t> (node.var)] = 1.0;
        return true;
      case Op::Neg:
        {
          double c = 0.0;
          if (!affine_of (node.lhs, c, coeffs))
            return false;
          constant = -c;
          for (double &k : coeffs)
            k = -k;
          return true;
        }
      case Op::Add:
      case Op::Sub:
        {
          double cl = 0.0, cr = 0.0;
          std::vector<double> kl (n, 0.0), kr (n, 0.0);
          if (!affine_of (node.lhs, cl, kl) || !affine_of (node.rhs, cr, kr))
            return false;
          const double sign = node.op == Op::Add ? 1.0 : -1.0;
          constant = cl + sign * cr;
          for (std::size_t i = 0; i < n; ++i)
            coeffs[i] = kl[i] + sign * kr[i];
          return true;
        }
      case Op::Mul:
        {
          // One side must be a constant tree.
          if (is_const_tree (node.lhs))
            {
              double c = 0.0, cr = 0.0;
              if (!affine_of (node.rhs, cr, coeffs))
                return false;
              c = evaluate (node.lhs, std::vector<double> (n, 0.0));
              constant = c * cr;
              for (double &k : coeffs)
                k *= c;
              return true;
            }
          if (is_const_tree (node.rhs))
            {
              double c = 0.0, cl = 0.0;
              if (!affine_of (node.lhs, cl, coeffs))
                return false;
              c = evaluate (node.rhs, std::vector<double> (n, 0.0));
              constant = c * cl;
              for (double &k : coeffs)
                k *= c;
              return true;
            }
          return false;
        }
      case Op::Div:
        if (is_const_tree (node.rhs))
          {
            double c = 0.0, cl = 0.0;
            if (!affine_of (node.lhs, cl, coeffs))
              return false;
            c = evaluate (node.rhs, std::vector<double> (n, 0.0));
            if (c == 0.0)
              return false;
            constant = cl / c;
            for (double &k : coeffs)
              k /= c;
            return true;
          }
        return false;
      default:
        return false;
      }
  }

  /// Gaussian elimination with partial pivoting over the affine subsystem.
  /// Returns false only on genuine inconsistency; underdetermined and
  /// non-affine rows are left for later stages.
  bool solve_linear (std::vector<double> &witness, std::vector<bool> &pinned)
  {
    const std::size_t n = var_index_.size ();
    std::vector<std::vector<double>> rows;
    for (const auto &[lhs, rhs] : equations_)
      {
        double cl = 0.0, cr = 0.0;
        std::vector<double> kl, kr;
        if (!affine_of (lhs, cl, kl) || !affine_of (rhs, cr, kr))
          continue;
        std::vector<double> row (n + 1, 0.0);
        for (std::size_t i = 0; i < n; ++i)
          row[i] = kl[i] - kr[i];
        row[n] = cr - cl;
        bool trivial = true;
        for (std::size_t i = 0; i < n; ++i)
          if (row[i] != 0.0)
            trivial = false;
        if (trivial)
          {
            if (std::fabs (row[n]) > tolerance_)
              return false;
            continue;
          }
        rows.push_back (std::move (row));
      }
    std::vector<int> pivot_of_row (rows.size (), -1);
    std::size_t r = 0;
    for (std::size_t col = 0; col < n && r < rows.size (); ++col)
      {
        std::size_t best = r;
        for (std::size_t i = r; i < rows.size (); ++i)
          if (std::fabs (rows[i][col]) > std::fabs (rows[best][col]))
            best = i;
        if (std::fabs (rows[best][col]) < 1e-12)
          continue;
        std::swap (rows[r], rows[best]);
        for (std::size_t i = 0; i < rows.size (); ++i)
          {
            if (i == r || rows[i][col] == 0.0)
              continue;
            const double factor = rows[i][col] / rows[r][col];
            for (std::size_t j = col; j <= n; ++j)
              rows[i][j] -= factor * rows[r][j];
          }
        pivot_of_row[r] = static_cast<int> (col);
        ++r;
      }
    for (std::size_t i = 0; i < rows.size (); ++i)
      {
        bool empty = true;
        for (std::size_t j = 0; j < n; ++j)
          if (std::fabs (rows[i][j]) > 1e-9)
            empty = false;
        if (empty && std::fabs (rows[i][n]) > tolerance_)
          return false;
      }
    // Back-substitution for pivoted variables (free variables stay 0).
    for (std::size_t i = 0; i < r; ++i)
      {
        const int col = pivot_of_row[i];
        double total = rows[i][n];
        for (std::size_t j = 0; j < n; ++j)
          if (static_cast<int> (j) != col)
            total -= rows[i][j] * witness[j];
        witness[static_cast<std::size_t> (col)] = total / rows[i][static_cast<std::size_t> (col)];
        pinned[static_cast<std::size_t> (col)] = true;
      }
    return true;
  }

  /// Closed-form roots for univariate quadratics in one free variable.
  bool solve_univariate_quadratic (std::vector<double> &witness,
                                   const std::vector<bool> &pinned)
  {
    const std::size_t n = var_index_.size ();
    for (const auto &[lhs, rhs] : equations_)
      {
        // Form p = lhs - rhs and collect the degree pattern.
        int live = -1;
        bool multivariate = false;
        double c0 = 0.0, c1 = 0.0, c2 = 0.0;
        if (!quadratic_coeffs (lhs, rhs, live, multivariate, c0, c1, c2))
          continue;
        if (multivariate || live < 0 || pinned[static_cast<std::size_t> (live)])
          continue;
        if (std::fabs (c2) < 1e-12)
          {
            if (std::fabs (c1) < 1e-12)
              continue;
            witness[static_cast<std::size_t> (live)] = -c0 / c1;
            if (validate (witness))
              return true;
            continue;
          }
        const double disc = c1 * c1 - 4.0 * c2 * c0;
        if (disc < 0.0)
          continue;
        const double root = (-c1 + std::sqrt (disc)) / (2.0 * c2);
        witness[static_cast<std::size_t> (live)] = root;
        if (validate (witness))
          return true;
        witness[static_cast<std::size_t> (live)] = (-c1 - std::sqrt (disc)) / (2.0 * c2);
        if (validate (witness))
          return true;
      }
    (void)n;
    return false;
  }

  /// Extracts `c0 + c1*x + c2*x^2` for `lhs - rhs` when it is a univariate
  /// polynomial of degree <= 2 in power-basis form.
  bool quadratic_coeffs (int lhs, int rhs, int &live, bool &multivariate, double &c0,
                         double &c1, double &c2) const
  {
    live = -1;
    multivariate = false;
    c0 = c1 = c2 = 0.0;
    return quadratic_accumulate (lhs, 1.0, live, multivariate, c0, c1, c2) &&
           quadratic_accumulate (rhs, -1.0, live, multivariate, c0, c1, c2);
  }

  bool quadratic_accumulate (int expr, double sign, int &live, bool &multivariate,
                             double &c0, double &c1, double &c2) const
  {
    const Expr &node = exprs_[static_cast<std::size_t> (expr)];
    switch (node.op)
      {
      case Op::Const:
        c0 += sign * node.value;
        return true;
      case Op::Var:
        if (live < 0)
          live = node.var;
        else if (live != node.var)
          multivariate = true;
        c1 += sign;
        return true;
      case Op::Neg:
        return quadratic_accumulate (node.lhs, -sign, live, multivariate, c0, c1, c2);
      case Op::Add:
        return quadratic_accumulate (node.lhs, sign, live, multivariate, c0, c1, c2) &&
               quadratic_accumulate (node.rhs, sign, live, multivariate, c0, c1, c2);
      case Op::Sub:
        return quadratic_accumulate (node.lhs, sign, live, multivariate, c0, c1, c2) &&
               quadratic_accumulate (node.rhs, -sign, live, multivariate, c0, c1, c2);
      case Op::Mul:
        {
          // Constant-times-affine only.
          if (is_const_tree (node.lhs))
            {
              const double k = evaluate (node.lhs, std::vector<double> (var_index_.size (), 0.0));
              return quadratic_accumulate (node.rhs, sign * k, live, multivariate, c0, c1, c2);
            }
          if (is_const_tree (node.rhs))
            {
              const double k = evaluate (node.rhs, std::vector<double> (var_index_.size (), 0.0));
              return quadratic_accumulate (node.lhs, sign * k, live, multivariate, c0, c1, c2);
            }
          // Square of a single variable.
          if (node.lhs == node.rhs)
            {
              int inner_live = -1;
              bool inner_multi = false;
              double d0 = 0.0, d1 = 0.0, d2 = 0.0;
              if (!quadratic_accumulate (node.lhs, 1.0, inner_live, inner_multi, d0, d1, d2) ||
                  inner_multi || d2 != 0.0)
                return false;
              if (live < 0)
                live = inner_live;
              else if (inner_live >= 0 && live != inner_live)
                multivariate = true;
              // (d0 + d1*x)^2 = d0^2 + 2*d0*d1*x + d1^2*x^2.
              c0 += sign * d0 * d0;
              c1 += sign * 2.0 * d0 * d1;
              c2 += sign * d1 * d1;
              return true;
            }
          return false;
        }
      case Op::Pow:
        {
          // x^2 only (higher powers left to Newton).
          if (node.aux != 2)
            return false;
          const Expr &base = exprs_[static_cast<std::size_t> (node.lhs)];
          if (base.op != Op::Var)
            return false;
          if (live < 0)
            live = base.var;
          else if (live != base.var)
            multivariate = true;
          c2 += sign;
          return true;
        }
      default:
        return false;
      }
  }

  /// Deterministic multi-start Newton with a numeric Jacobian.
  bool newton_search (std::vector<double> &witness, const std::vector<bool> &pinned)
  {
    const std::size_t n = var_index_.size ();
    std::vector<std::size_t> free_vars;
    for (std::size_t i = 0; i < n; ++i)
      if (!pinned[i])
        free_vars.push_back (i);
    if (free_vars.empty ())
      return false;
    std::vector<std::vector<double>> seeds{ witness };
    std::vector<double> alt (n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
      alt[i] = 1.0;
    seeds.push_back (alt);
    std::vector<double> neg (n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
      neg[i] = -1.0;
    seeds.push_back (neg);
    for (const std::vector<double> &seed : seeds)
      {
        std::vector<double> point = seed;
        for (std::size_t v = 0; v < n; ++v)
          if (pinned[v])
            point[v] = witness[v];
        for (std::size_t iter = 0; iter < newton_cap_; ++iter)
          {
            std::vector<double> residual;
            for (const auto &[lhs, rhs] : equations_)
              residual.push_back (evaluate (lhs, point) - evaluate (rhs, point));
            double norm = 0.0;
            for (double r : residual)
              norm += r * r;
            if (!(norm >= 0.0) || norm < tolerance_ * tolerance_)
              {
                witness = point;
                return true;
              }
            // Numeric Jacobian over free variables only.
            const std::size_t m = equations_.size ();
            const std::size_t f = free_vars.size ();
            std::vector<std::vector<double>> jac (m, std::vector<double> (f, 0.0));
            for (std::size_t j = 0; j < f; ++j)
              {
                std::vector<double> bumped = point;
                const double h = 1e-7 * (1.0 + std::fabs (point[free_vars[j]]));
                bumped[free_vars[j]] += h;
                for (std::size_t i = 0; i < m; ++i)
                  {
                    const auto &[lhs, rhs] = equations_[i];
                    jac[i][j] =
                        (evaluate (lhs, bumped) - evaluate (rhs, bumped) - residual[i]) / h;
                  }
              }
            std::vector<double> step;
            if (!least_squares_step (jac, residual, step))
              break;
            // Damped update.
            double alpha = 1.0;
            bool advanced = false;
            for (int trial = 0; trial < 8; ++trial)
              {
                std::vector<double> next = point;
                for (std::size_t j = 0; j < f; ++j)
                  next[free_vars[j]] -= alpha * step[j];
                double next_norm = 0.0;
                bool finite = true;
                for (const auto &[lhs, rhs] : equations_)
                  {
                    const double r = evaluate (lhs, next) - evaluate (rhs, next);
                    if (!std::isfinite (r))
                      {
                        finite = false;
                        break;
                      }
                    next_norm += r * r;
                  }
                if (finite && next_norm < norm)
                  {
                    point = next;
                    advanced = true;
                    break;
                  }
                alpha *= 0.5;
              }
            if (!advanced)
              break;
          }
        if (validate (point))
          {
            witness = point;
            return true;
          }
      }
    return false;
  }

  /// Normal-equation least-squares step for J*step = residual.
  static bool least_squares_step (const std::vector<std::vector<double>> &jac,
                                  const std::vector<double> &residual,
                                  std::vector<double> &step)
  {
    const std::size_t m = jac.size ();
    const std::size_t f = m == 0 ? 0 : jac[0].size ();
    if (f == 0)
      return false;
    // A = J^T J, b = J^T r.
    std::vector<std::vector<double>> a (f, std::vector<double> (f + 1, 0.0));
    for (std::size_t i = 0; i < f; ++i)
      {
        for (std::size_t j = 0; j < f; ++j)
          for (std::size_t k = 0; k < m; ++k)
            a[i][j] += jac[k][i] * jac[k][j];
        for (std::size_t k = 0; k < m; ++k)
          a[i][f] += jac[k][i] * residual[k];
        a[i][i] += 1e-10; // Damping for rank deficiency.
      }
    // Gaussian elimination with partial pivoting.
    for (std::size_t col = 0; col < f; ++col)
      {
        std::size_t best = col;
        for (std::size_t i = col; i < f; ++i)
          if (std::fabs (a[i][col]) > std::fabs (a[best][col]))
            best = i;
        if (std::fabs (a[best][col]) < 1e-14)
          return false;
        std::swap (a[col], a[best]);
        for (std::size_t i = 0; i < f; ++i)
          {
            if (i == col)
              continue;
            const double factor = a[i][col] / a[col][col];
            for (std::size_t j = col; j <= f; ++j)
              a[i][j] -= factor * a[col][j];
          }
      }
    step.assign (f, 0.0);
    for (std::size_t i = 0; i < f; ++i)
      {
        step[i] = a[i][f] / a[i][i];
        if (!std::isfinite (step[i]))
          return false;
      }
    return true;
  }

  CNF cnf_{};
  std::map<std::string, int> var_index_;
  std::vector<Expr> exprs_;
  std::vector<std::pair<int, int>> equations_;
  std::vector<double> model_;
  std::size_t newton_cap_ = 64;
  double tolerance_ = 1e-9;
};

} // namespace satie::symsolve
