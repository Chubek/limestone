#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"
#include "SatieIDL.hpp"

namespace satie::isl
{

/// Integer stride logic: difference constraints plus divisibility.
///
/// Constraints are `x - y <= c` and `divisor | (var - offset)` with
/// `divisor > 0`. `check` runs the IDL core first (infeasible there means
/// genuinely UNSAT), then validates divisibility on the IDL model. When the
/// model violates a stride, bounded enumeration over explicitly bounded
/// variables repairs it: every variable under a stride must carry an
/// `add_bound`, and the product of the bound widths is capped
/// (`set_search_cap`). An exhaustive pass over the bounded box is a sound
/// UNSAT certificate; anything unbounded degrades honestly to UNKNOWN.
class ISLSolver : public idl::IDLSolver
{
public:
  ISLSolver () = default;
  explicit ISLSolver (CNF cnf) : idl::IDLSolver (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "ISL";
  }

  void clear_theory ()
  {
    idl::IDLSolver::clear_theory ();
    strides_.clear ();
    bounds_.clear ();
  }

  void set_search_cap (std::size_t cap) { search_cap_ = cap == 0 ? 1 : cap; }

  /// Asserts `divisor | (var - offset)` with `divisor > 0`.
  void add_divisible (int var, std::int64_t divisor, std::int64_t offset)
  {
    if (var < 0 || var >= static_cast<int> (variable_count ()))
      throw std::invalid_argument ("unknown ISL variable id");
    if (divisor <= 0)
      throw std::invalid_argument ("ISL divisor must be positive");
    strides_.push_back ({ var, divisor, offset });
  }

  /// Asserts `lo <= var <= hi` and records the range for the repair search.
  void add_bound (int var, std::int64_t lo, std::int64_t hi)
  {
    idl::IDLSolver::add_bound (var, lo, hi);
    bounds_.push_back ({ var, { lo, hi } });
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (problem ());
    if (boolean.unsatisfiable ())
      return boolean;
    model_.clear ();
    if (!solve_difference (nullptr))
      return { SolveStatus::UNSAT, Assignment (problem ().variable_count ()) };
    if (strides_satisfied (model_))
      return { SolveStatus::SAT, boolean.assignment };
    const Repair repair = bounded_repair ();
    if (repair == Repair::Sat)
      return { SolveStatus::SAT, boolean.assignment };
    if (repair == Repair::Unsat)
      return { SolveStatus::UNSAT, Assignment (problem ().variable_count ()) };
    return { SolveStatus::UNKNOWN, Assignment (problem ().variable_count ()) };
  }

private:
  struct Stride
  {
    int var = -1;
    std::int64_t divisor = 1;
    std::int64_t offset = 0;
  };

  enum class Repair
  {
    Sat,
    Unsat,
    Unknown
  };

  bool strides_satisfied (const std::vector<std::int64_t> &values) const
  {
    for (const Stride &s : strides_)
      if ((values[static_cast<std::size_t> (s.var)] - s.offset) % s.divisor != 0)
        return false;
    return true;
  }

  /// Enumerates explicitly bounded stride variables. The IDL constraints are
  /// re-checked for every combination by fixing candidates as equalities, so
  /// an exhaustive pass is a sound UNSAT certificate for the bounded case.
  Repair bounded_repair ()
  {
    std::vector<int> strided;
    for (const Stride &s : strides_)
      strided.push_back (s.var);
    std::sort (strided.begin (), strided.end ());
    strided.erase (std::unique (strided.begin (), strided.end ()), strided.end ());

    std::vector<std::pair<std::int64_t, std::int64_t>> ranges;
    std::size_t cells = 1;
    for (int var : strided)
      {
        bool found = false;
        std::pair<std::int64_t, std::int64_t> range{ 0, -1 };
        for (const auto &[v, r] : bounds_)
          if (v == var)
            {
              range = r;
              found = true;
            }
        if (!found)
          return Repair::Unknown;
        if (range.second < range.first)
          return Repair::Unsat;
        const std::uint64_t width =
            static_cast<std::uint64_t> (range.second - range.first) + 1;
        if (width > search_cap_ || cells > search_cap_ / width)
          return Repair::Unknown;
        cells *= width;
        ranges.push_back (range);
      }

    const int zero = zero_var ();
    std::vector<std::int64_t> cursor;
    for (const auto &[lo, hi] : ranges)
      {
        (void)hi;
        cursor.push_back (lo);
      }
    for (std::size_t step = 0; step < cells; ++step)
      {
        std::vector<Constraint> extra;
        for (std::size_t i = 0; i < strided.size (); ++i)
          {
            extra.push_back ({ strided[i], zero, cursor[i] });
            extra.push_back ({ zero, strided[i], -cursor[i] });
          }
        // NOTE: `solve_difference` overwrites `model_`; only accept it when
        // the strides hold for the fixed combination.
        std::vector<std::int64_t> saved = model_;
        if (solve_difference (&extra) && strides_satisfied (model_))
          return Repair::Sat;
        model_ = saved;
        for (std::size_t i = 0; i < strided.size (); ++i)
          {
            if (cursor[i] < ranges[i].second)
              {
                ++cursor[i];
                break;
              }
            cursor[i] = ranges[i].first;
          }
      }
    model_.clear ();
    return Repair::Unsat;
  }

  std::vector<Stride> strides_;
  std::vector<std::pair<int, std::pair<std::int64_t, std::int64_t>>> bounds_;
  std::size_t search_cap_ = 32768;
};

} // namespace satie::isl
