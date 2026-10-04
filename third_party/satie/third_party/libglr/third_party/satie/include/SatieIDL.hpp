#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"

namespace satie::idl
{

/// Integer difference logic (complete, via Bellman-Ford).
///
/// Constraints have the form `x - y <= c`. `check` first runs the Boolean
/// solver as a layered filter, then decides the difference constraints with
/// Bellman-Ford: a negative cycle is an exact UNSAT certificate, and the
/// shortest-path potentials are a model otherwise. There is no theory
/// propagation back into the SAT solver yet.
class IDLSolver
{
public:
  IDLSolver () = default;
  explicit IDLSolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "IDL";
  }

  /// `load` replaces the Boolean problem; difference constraints are
  /// preserved. Use `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    var_index_.clear ();
    constraints_.clear ();
    model_.clear ();
    zero_ = -1;
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

  /// Declares (or fetches) a variable pinned to zero, for absolute bounds.
  int zero_var ()
  {
    if (zero_ < 0)
      {
        zero_ = add_var ("__idl_zero");
        pinned_zero_ = true;
      }
    return zero_;
  }

  /// Asserts `x - y <= c`.
  void add_le (int x, int y, std::int64_t c)
  {
    require_var (x);
    require_var (y);
    constraints_.push_back ({ x, y, c });
  }

  /// Asserts `lo <= x <= hi` via the zero variable.
  void add_bound (int x, std::int64_t lo, std::int64_t hi)
  {
    require_var (x);
    const int z = zero_var ();
    constraints_.push_back ({ x, z, hi });
    constraints_.push_back ({ z, x, -lo });
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    if (!solve_difference (nullptr))
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    return { SolveStatus::SAT, boolean.assignment };
  }

  /// Model value of variable `x` from the last successful `check`.
  std::int64_t value (int x) const
  {
    require_var (x);
    if (model_.empty ())
      throw std::logic_error ("IDL model is unavailable before a SAT check");
    return model_[static_cast<std::size_t> (x)];
  }

  std::size_t variable_count () const { return var_index_.size (); }

protected:
  struct Constraint
  {
    int x = -1;
    int y = -1;
    std::int64_t c = 0;
  };

  void require_var (int var) const
  {
    if (var < 0 || var >= static_cast<int> (var_index_.size ()))
      throw std::invalid_argument ("unknown IDL variable id");
  }

  /// Bellman-Ford over edges `y -> x` with weight `c`. `extra` constraints
  /// (used by subclasses such as ISL) are appended for the feasibility test.
  bool solve_difference (const std::vector<Constraint> *extra)
  {
    const std::size_t n = var_index_.size ();
    std::vector<Constraint> all = constraints_;
    if (extra != nullptr)
      all.insert (all.end (), extra->begin (), extra->end ());

    constexpr std::int64_t inf = std::numeric_limits<std::int64_t>::max () / 4;
    std::vector<std::int64_t> dist (n, 0);
    for (std::size_t i = 0; i < n; ++i)
      {
        bool changed = false;
        for (const Constraint &con : all)
          {
            const std::size_t u = static_cast<std::size_t> (con.y);
            const std::size_t v = static_cast<std::size_t> (con.x);
            if (dist[u] == inf)
              continue;
            // Saturating addition keeps the test total on huge constants.
            std::int64_t next = dist[u] + con.c;
            if ((con.c > 0 && next < dist[u]) || (con.c < 0 && next > dist[u]))
              next = con.c > 0 ? inf : -inf;
            if (next < dist[v])
              {
                if (i + 1 == n)
                  return false; // Negative cycle: infeasible.
                dist[v] = next;
                changed = true;
              }
          }
        if (!changed)
          break;
      }
    if (pinned_zero_)
      {
        const std::int64_t shift = dist[static_cast<std::size_t> (zero_)];
        for (std::int64_t &d : dist)
          d -= shift;
      }
    model_ = std::move (dist);
    return true;
  }

protected:
  CNF cnf_{};
  std::map<std::string, int> var_index_;
  std::vector<Constraint> constraints_;
  std::vector<std::int64_t> model_;
  int zero_ = -1;
  bool pinned_zero_ = false;
};

} // namespace satie::idl
