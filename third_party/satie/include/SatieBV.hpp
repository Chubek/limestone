#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "SatieCDCL.hpp"
#include "SatieTheoryUtils.hpp"

namespace satie::bv
{

/// Bit-vector theory solver (complete, via bit-blasting).
///
/// Variables are fixed-width unsigned integers. `add_eq`, `add_ult`, and
/// `add_add` are compiled to CNF with a Tseitin encoder and solved together
/// with the loaded Boolean problem, so `check` is a full decision procedure
/// for the supported operators. Widths are capped at 64 bits; anything wider
/// is rejected. Only the listed operators are supported (no shifts, division,
/// or signed comparisons yet).
class BVSolver
{
public:
  BVSolver () = default;
  explicit BVSolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "BV";
  }

  /// `load` replaces the Boolean problem; declared bit-vector variables and
  /// constraints are preserved. Use `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    widths_.clear ();
    equalities_.clear ();
    comparisons_.clear ();
    additions_.clear ();
    consts_.clear ();
    model_.clear ();
  }

  void add_var (const std::string &name, int width)
  {
    if (width < 1 || width > 64)
      throw std::invalid_argument ("BV variables require a width in 1..64");
    if (!widths_.emplace (name, width).second)
      throw std::invalid_argument ("BV variable is already declared: " + name);
  }

  int width (const std::string &name) const
  {
    auto it = widths_.find (name);
    if (it == widths_.end ())
      throw std::invalid_argument ("unknown BV variable: " + name);
    return it->second;
  }

  /// Unsigned `a + b == c` (modulo 2^w, all widths must match).
  void add_add (const std::string &a, const std::string &b, const std::string &c)
  {
    require_same_width (a, b, c);
    additions_.push_back ({ a, b, c });
  }

  void add_eq (const std::string &a, const std::string &b)
  {
    require_same_width (a, b, a);
    equalities_.push_back ({ a, b });
  }

  /// Unsigned `a < b`.
  void add_ult (const std::string &a, const std::string &b)
  {
    require_same_width (a, b, a);
    comparisons_.push_back ({ a, b });
  }

  /// Equality against an unsigned constant (must fit the variable width).
  void add_eq_const (const std::string &a, std::uint64_t value)
  {
    const int w = width (a);
    if (w < 64 && (value >> w) != 0)
      throw std::invalid_argument ("BV constant exceeds the variable width");
    consts_.push_back ({ a, value, true });
  }

  /// Unsigned `a < value`.
  void add_ult_const (const std::string &a, std::uint64_t value)
  {
    const int w = width (a);
    if (w < 64 && (value >> w) != 0)
      throw std::invalid_argument ("BV constant exceeds the variable width");
    consts_.push_back ({ a, value, false });
  }

  SolveResult check ()
  {
    model_.clear ();
    theory::Encoder enc (cnf_.variable_count () + 1);
    std::map<std::string, std::vector<Lit>> bits;
    for (const auto &[name, width] : widths_)
      {
        std::vector<Lit> vec;
        for (int i = 0; i < width; ++i)
          vec.push_back (enc.fresh_lit ());
        bits.emplace (name, std::move (vec));
      }
    for (const auto &[a, b] : equalities_)
      enc.assert_eq_bits (bits[a], bits[b]);
    for (const auto &[a, value, is_eq] : consts_)
      {
        std::vector<Lit> constant;
        for (int i = 0; i < widths_[a]; ++i)
          {
            Lit bit = enc.fresh_lit ();
            if ((value >> i) & 1u)
              enc.add_clause ({ bit });
            else
              enc.add_clause ({ negate (bit) });
            constant.push_back (bit);
          }
        if (is_eq)
          enc.assert_eq_bits (bits[a], constant);
        else
          enc.add_clause ({ enc.ult (bits[a], constant) });
      }
    for (const auto &[a, b] : comparisons_)
      enc.add_clause ({ enc.ult (bits[a], bits[b]) });
    for (const auto &[a, b, c] : additions_)
      {
        std::vector<Lit> sum = enc.ripple_add (bits[a], bits[b], enc.const_false ());
        sum.pop_back (); // Modular arithmetic: the carry out is discarded.
        enc.assert_eq_bits (sum, bits[c]);
      }

    CNF combined = cnf_;
    for (Clause &clause : enc.take_clauses ())
      combined.add_clause (std::move (clause));
    SolveResult result = solve_cdcl (combined);
    if (result.unsatisfiable ())
      return result;
    // Extract the model and validate it against the integer semantics as a
    // soundness backstop: the encoding is exact, so any mismatch is an
    // internal error rather than a wrong answer.
    for (const auto &[name, vec] : bits)
      {
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < vec.size (); ++i)
          if (result.assignment.get_literal (vec[i]) == Value::TRUE)
            value |= (std::uint64_t{ 1 } << i);
        model_.emplace (name, value);
      }
    if (!validate_model ())
      return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
    return { SolveStatus::SAT, result.assignment };
  }

  /// Unsigned value of `name` in the last model (empty when no model).
  std::optional<std::uint64_t> value (const std::string &name) const
  {
    auto it = model_.find (name);
    if (it == model_.end ())
      return std::nullopt;
    return it->second;
  }

private:
  struct Pair
  {
    std::string first;
    std::string second;
  };
  struct Triple
  {
    std::string first;
    std::string second;
    std::string third;
  };
  struct ConstCmp
  {
    std::string var;
    std::uint64_t value = 0;
    bool is_eq = true;
  };

  void require_same_width (const std::string &a, const std::string &b,
                           const std::string &c) const
  {
    if (width (a) != width (b) || width (a) != width (c))
      throw std::invalid_argument ("BV operand widths must match");
  }

  bool validate_model () const
  {
    for (const auto &[a, b] : equalities_)
      if (model_.at (a) != model_.at (b))
        return false;
    for (const auto &[a, b] : comparisons_)
      if (!(model_.at (a) < model_.at (b)))
        return false;
    for (const auto &[a, b, c] : additions_)
      {
        const int w = widths_.at (a);
        const std::uint64_t mask = w == 64 ? ~std::uint64_t{ 0 } : ((std::uint64_t{ 1 } << w) - 1);
        if ((((model_.at (a) + model_.at (b)) & mask)) != (model_.at (c) & mask))
          return false;
      }
    for (const auto &[a, value, is_eq] : consts_)
      {
        if (is_eq && model_.at (a) != value)
          return false;
        if (!is_eq && !(model_.at (a) < value))
          return false;
      }
    return true;
  }

  CNF cnf_{};
  std::map<std::string, int> widths_;
  std::vector<Pair> equalities_;
  std::vector<Pair> comparisons_;
  std::vector<Triple> additions_;
  std::vector<ConstCmp> consts_;
  std::map<std::string, std::uint64_t> model_;
};

} // namespace satie::bv
