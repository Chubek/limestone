#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"
#include "SatieLP.hpp"
#include "SatieTheoryUtils.hpp"

namespace satie::lia
{

/// Linear integer arithmetic (sound; complete within the bit-width bound).
///
/// Constraints are `sum(coeff[i] * x[i]) <= bound` over mathematical
/// integers. `check` works in two stages: a rational relaxation decided by
/// the LP solver (infeasible there means genuinely UNSAT), then bounded
/// bit-blasting of each variable as a two's-complement vector of `width`
/// bits (sums use an extended width, so there is no overflow inside the
/// bound). A SAT answer always comes with an integer model validated by
/// direct evaluation. When the bounded encoding is UNSAT the answer is
/// UNKNOWN rather than UNSAT: a wider bound might still hold a model.
class LIASolver
{
public:
  LIASolver () = default;
  explicit LIASolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "LIA";
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

  void set_width (int width)
  {
    if (width < 2 || width > 32)
      throw std::invalid_argument ("LIA width must be in 2..32");
    width_ = width;
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

  /// Asserts `sum(coeff[var] * var) <= bound`.
  void add_le (const std::vector<std::pair<int, std::int64_t>> &coeffs,
               std::int64_t bound)
  {
    require_coeffs (coeffs);
    constraints_.push_back ({ coeffs, bound });
  }

  void add_eq (const std::vector<std::pair<int, std::int64_t>> &coeffs,
               std::int64_t bound)
  {
    add_le (coeffs, bound);
    std::vector<std::pair<int, std::int64_t>> neg = coeffs;
    for (auto &[var, coeff] : neg)
      coeff = -coeff;
    add_le (neg, -bound);
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    model_.clear ();
    if (!rationally_feasible ())
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    if (bounded_search (boolean.assignment))
      return { SolveStatus::SAT, boolean.assignment };
    // The relaxation is feasible but no model fits in `width_` bits: the
    // honest answer is UNKNOWN, not UNSAT.
    return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
  }

  std::int64_t value (int var) const
  {
    require_var (var);
    if (model_.empty ())
      throw std::logic_error ("LIA model is unavailable before a SAT check");
    return model_[static_cast<std::size_t> (var)];
  }

private:
  struct Constraint
  {
    std::vector<std::pair<int, std::int64_t>> coeffs;
    std::int64_t bound = 0;
  };

  void require_var (int var) const
  {
    if (var < 0 || var >= static_cast<int> (var_index_.size ()))
      throw std::invalid_argument ("unknown LIA variable id");
  }

  void require_coeffs (const std::vector<std::pair<int, std::int64_t>> &coeffs) const
  {
    for (const auto &[var, coeff] : coeffs)
      {
        require_var (var);
        (void)coeff;
      }
  }

  bool rationally_feasible () const
  {
    lp::LPSolver lp;
    for (std::size_t i = 0; i < var_index_.size (); ++i)
      lp.add_var ("x" + std::to_string (i));
    for (const Constraint &con : constraints_)
      {
        std::vector<std::pair<int, double>> coeffs;
        for (const auto &[var, coeff] : con.coeffs)
          coeffs.push_back ({ var, static_cast<double> (coeff) });
        lp.add_le (coeffs, static_cast<double> (con.bound));
      }
    return !lp.check ().unsatisfiable ();
  }

  std::int64_t evaluate (const Constraint &con,
                         const std::vector<std::int64_t> &values) const
  {
    std::int64_t total = 0;
    for (const auto &[var, coeff] : con.coeffs)
      total += coeff * values[static_cast<std::size_t> (var)];
    return total;
  }

  /// Encodes `coeff * var` (two's complement, `ext` bits) by shift-and-add.
  static std::vector<Lit> mul_const (theory::Encoder &enc,
                                     const std::vector<Lit> &var_bits,
                                     std::int64_t coeff, int ext)
  {
    const Lit zero = enc.const_false ();
    std::vector<Lit> acc (static_cast<std::size_t> (ext), zero);
    const bool negative = coeff < 0;
    const std::uint64_t mag =
        negative ? std::uint64_t{ 0 } - static_cast<std::uint64_t> (coeff)
                 : static_cast<std::uint64_t> (coeff);
    std::uint64_t remaining = mag;
    int shift = 0;
    while (remaining != 0)
      {
        if ((remaining & 1u) != 0)
          {
            std::vector<Lit> shifted (static_cast<std::size_t> (ext), zero);
            for (int i = 0; i + shift < ext; ++i)
              shifted[static_cast<std::size_t> (i + shift)] =
                  var_bits[static_cast<std::size_t> (i)];
            std::vector<Lit> sum = enc.ripple_add (acc, shifted, enc.const_false ());
            sum.pop_back ();
            acc = std::move (sum);
          }
        remaining >>= 1;
        ++shift;
      }
    if (negative)
      {
        for (Lit &lit : acc)
          lit = negate (lit);
        std::vector<Lit> one (static_cast<std::size_t> (ext), zero);
        one[0] = enc.const_true ();
        std::vector<Lit> sum = enc.ripple_add (acc, one, enc.const_false ());
        sum.pop_back ();
        acc = std::move (sum);
      }
    return acc;
  }

  bool bounded_search (const Assignment &boolean_model)
  {
    const std::size_t n = var_index_.size ();
    // Extended width: variable bits plus room for the largest coefficient
    // magnitude and the accumulation of all terms, plus a sign margin. Sums
    // that overflow the working width wrap silently, but the integer
    // validation below rejects any wrapped model (degrading to UNKNOWN).
    auto bit_length = [] (std::uint64_t m) {
      int bits = 0;
      while (bits < 64 && (m >> static_cast<unsigned> (bits)) != 0)
        ++bits;
      return bits;
    };
    std::uint64_t max_mag = 1;
    std::size_t max_terms = 1;
    for (const Constraint &con : constraints_)
      {
        max_terms = std::max (max_terms, con.coeffs.size ());
        for (const auto &[var, coeff] : con.coeffs)
          {
            (void)var;
            const std::uint64_t mag = coeff < 0 ? std::uint64_t{ 0 } - static_cast<std::uint64_t> (coeff)
                                                : static_cast<std::uint64_t> (coeff);
            max_mag = std::max (max_mag, mag);
          }
      }
    const int ext = width_ + bit_length (max_mag) +
                    bit_length (static_cast<std::uint64_t> (max_terms)) + 2;
    if (ext > 256)
      return false;

    theory::Encoder enc (cnf_.variable_count () + 1);
    std::vector<std::vector<Lit>> bits (n);
    for (std::size_t v = 0; v < n; ++v)
      {
        std::vector<Lit> vec;
        for (int i = 0; i < width_; ++i)
          vec.push_back (enc.fresh_lit ());
        // Sign extension to the working width.
        while (static_cast<int> (vec.size ()) < ext)
          vec.push_back (vec.back ());
        bits[v] = std::move (vec);
      }
    const Lit zero = enc.const_false ();
    for (const Constraint &con : constraints_)
      {
        std::vector<Lit> sum (static_cast<std::size_t> (ext), zero);
        for (const auto &[var, coeff] : con.coeffs)
          {
            std::vector<Lit> term =
                mul_const (enc, bits[static_cast<std::size_t> (var)], coeff, ext);
            std::vector<Lit> next = enc.ripple_add (sum, term, enc.const_false ());
            next.pop_back ();
            sum = std::move (next);
          }
        // sum <= bound  <=>  sum < bound + 1.
        std::vector<Lit> limit = const_bits (enc, con.bound + 1, ext);
        enc.add_clause ({ enc.ult (sum, limit) });
      }

    CNF combined = cnf_;
    for (Clause &clause : enc.take_clauses ())
      combined.add_clause (std::move (clause));
    SolveResult result = solve_cdcl (combined);
    if (result.unsatisfiable ())
      return false;
    // Extract two's-complement values and validate by integer evaluation.
    std::vector<std::int64_t> values (n, 0);
    for (std::size_t v = 0; v < n; ++v)
      {
        std::uint64_t raw = 0;
        for (int i = ext - 1; i >= 0; --i)
          {
            raw <<= 1;
            if (result.assignment.get_literal (bits[v][static_cast<std::size_t> (i)]) ==
                Value::TRUE)
              raw |= 1u;
          }
        // Keep the declared width, then sign-extend back to int64.
        const std::uint64_t mask =
            width_ == 64 ? ~std::uint64_t{ 0 } : ((std::uint64_t{ 1 } << width_) - 1);
        const std::uint64_t low = raw & mask;
        const unsigned shift = static_cast<unsigned> (64 - width_);
        values[v] = static_cast<std::int64_t> (low << shift) >> shift;
      }
    for (const Constraint &con : constraints_)
      if (evaluate (con, values) > con.bound)
        return false;
    // Restrict the model to the declared width's range.
    model_ = std::move (values);
    (void)boolean_model;
    return true;
  }

  static std::vector<Lit> const_bits (theory::Encoder &enc, std::int64_t value,
                                      int ext)
  {
    std::vector<Lit> vec;
    const Lit t = enc.const_true ();
    const Lit f = negate (t);
    for (int i = 0; i < ext; ++i)
      vec.push_back (((value >> i) & 1) != 0 ? t : f);
    return vec;
  }

  CNF cnf_{};
  std::map<std::string, int> var_index_;
  std::vector<Constraint> constraints_;
  std::vector<std::int64_t> model_;
  int width_ = 12;
};

} // namespace satie::lia
