#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"
#include "SatieTheoryUtils.hpp"

namespace satie::quant
{

/// Quantified Boolean formulas by expansion (complete for finite domains).
///
/// Propositional variables come from `add_bool_var`; integer variables from
/// `add_int_var` over an explicit finite domain (one-hot encoded, exactly
/// one value holds). Quantifier blocks (`add_exists`/`add_forall` and the
/// `_int` variants) are eliminated by expansion — universal as conjunction,
/// existential as disjunction — which preserves equivalence, so block order
/// does not matter. The matrix is a CNF over Boolean literals plus integer
/// equality atoms. Unquantified variables stay free and receive models;
/// quantified variables report defaults. Expansion blowup is capped
/// (`set_expansion_cap`); exceeding it yields UNKNOWN.
class QuantSolver
{
public:
  QuantSolver () = default;
  explicit QuantSolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "Quant";
  }

  /// `load` replaces the Boolean problem; quantifier state is preserved.
  /// Use `clear_theory` to drop that as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    bool_vars_.clear ();
    int_vars_.clear ();
    int_domains_.clear ();
    blocks_.clear ();
    matrix_.clear ();
    int_eqs_.clear ();
    model_bool_.clear ();
    model_int_.clear ();
    has_encoding_ = false;
  }

  void set_expansion_cap (std::size_t cap) { expansion_cap_ = cap == 0 ? 1 : cap; }

  int add_bool_var (const std::string &name)
  {
    auto it = bool_vars_.find (name);
    if (it != bool_vars_.end ())
      return it->second;
    const int id = static_cast<int> (bool_vars_.size ());
    bool_vars_.emplace (name, id);
    return id;
  }

  /// Integer variable over an explicit finite domain (non-empty).
  int add_int_var (const std::string &name, const std::vector<std::int64_t> &domain)
  {
    if (domain.empty ())
      throw std::invalid_argument ("quantifier integer domain must be non-empty");
    auto it = int_vars_.find (name);
    if (it != int_vars_.end ())
      return it->second;
    const int id = static_cast<int> (int_domains_.size ());
    int_vars_.emplace (name, id);
    int_domains_.push_back (domain);
    return id;
  }

  void add_exists (const std::vector<int> &vars)
  {
    for (int var : vars)
      require_bool_var (var);
    blocks_.push_back ({ true, vars, {} });
  }

  void add_forall (const std::vector<int> &vars)
  {
    for (int var : vars)
      require_bool_var (var);
    blocks_.push_back ({ false, vars, {} });
  }

  void add_exists_int (const std::vector<int> &vars)
  {
    for (int var : vars)
      require_int_var (var);
    blocks_.push_back ({ true, {}, vars });
  }

  void add_forall_int (const std::vector<int> &vars)
  {
    for (int var : vars)
      require_int_var (var);
    blocks_.push_back ({ false, {}, vars });
  }

  /// Matrix clause over Boolean literals: `(var, negated)`.
  void add_matrix_clause (const std::vector<std::pair<int, bool>> &literals)
  {
    for (const auto &[var, negated] : literals)
      {
        require_bool_var (var);
        (void)negated;
      }
    matrix_.push_back (literals);
  }

  /// Integer atom `var == value`, conjoined with the matrix.
  void add_int_eq (int var, std::int64_t value)
  {
    require_int_var (var);
    int_eqs_.push_back ({ var, value, -1 });
  }

  /// Integer atom `a == b`, conjoined with the matrix.
  void add_int_eq_vars (int a, int b)
  {
    require_int_var (a);
    require_int_var (b);
    int_eqs_.push_back ({ a, 0, b });
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    model_bool_.clear ();
    model_int_.clear ();
    ClauseList expanded;
    if (!expand (expanded))
      return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
    CNF combined = cnf_;
    for (Clause &clause : expanded)
      combined.add_clause (std::move (clause));
    SolveResult result = solve_cdcl (combined);
    if (result.unsatisfiable ())
      return result;
    extract_model (result.assignment);
    return { SolveStatus::SAT, boolean.assignment };
  }

  /// Model of a free Boolean variable (false for quantified variables).
  bool bool_value (int var) const
  {
    require_bool_var (var);
    if (model_bool_.empty ())
      throw std::logic_error ("quantifier model is unavailable before a SAT check");
    return model_bool_[static_cast<std::size_t> (var)];
  }

  /// Model of a free integer variable (domain front for quantified ones).
  std::int64_t int_value (int var) const
  {
    require_int_var (var);
    if (model_int_.empty ())
      throw std::logic_error ("quantifier model is unavailable before a SAT check");
    return model_int_[static_cast<std::size_t> (var)];
  }

private:
  struct Block
  {
    bool exists = true;
    std::vector<int> bools;
    std::vector<int> ints;
  };
  struct IntEq
  {
    int var = -1;
    std::int64_t value = 0;
    int other = -1; // >= 0 for var-var equality
  };
  struct Encoding
  {
    std::vector<Lit> bools;
    std::vector<std::vector<Lit>> ints;
  };

  void require_bool_var (int var) const
  {
    if (var < 0 || var >= static_cast<int> (bool_vars_.size ()))
      throw std::invalid_argument ("unknown quantifier boolean variable id");
  }

  void require_int_var (int var) const
  {
    if (var < 0 || var >= static_cast<int> (int_domains_.size ()))
      throw std::invalid_argument ("unknown quantifier integer variable id");
  }

  bool is_quantified_bool (int var) const
  {
    for (const Block &block : blocks_)
      for (int q : block.bools)
        if (q == var)
          return true;
    return false;
  }

  bool is_quantified_int (int var) const
  {
    for (const Block &block : blocks_)
      for (int q : block.ints)
        if (q == var)
          return true;
    return false;
  }

  /// Index of `value` in the domain, or -1 when absent.
  int domain_index (int var, std::int64_t value) const
  {
    const std::vector<std::int64_t> &domain =
        int_domains_[static_cast<std::size_t> (var)];
    for (std::size_t i = 0; i < domain.size (); ++i)
      if (domain[i] == value)
        return static_cast<int> (i);
    return -1;
  }

  bool expand (ClauseList &out)
  {
    const std::size_t nb = bool_vars_.size ();
    const std::size_t ni = int_domains_.size ();

    struct QVar
    {
      bool exists = true;
      bool integer = false;
      int id = -1;
    };
    std::vector<QVar> quantifiers;
    for (const Block &block : blocks_)
      {
        for (int var : block.bools)
          quantifiers.push_back ({ block.exists, false, var });
        for (int var : block.ints)
          quantifiers.push_back ({ block.exists, true, var });
      }
    std::vector<std::size_t> option_counts;
    for (const QVar &q : quantifiers)
      option_counts.push_back (
          q.integer ? int_domains_[static_cast<std::size_t> (q.id)].size () : 2);

    std::size_t product = 1;
    for (std::size_t count : option_counts)
      {
        if (product > expansion_cap_ / count)
          return false;
        product *= count;
      }

    theory::Encoder enc (cnf_.variable_count () + 1);
    Encoding encoding;
    for (std::size_t i = 0; i < nb; ++i)
      encoding.bools.push_back (enc.fresh_lit ());
    for (std::size_t i = 0; i < ni; ++i)
      {
        std::vector<Lit> one_hot;
        for (std::size_t j = 0; j < int_domains_[i].size (); ++j)
          one_hot.push_back (enc.fresh_lit ());
        enc.add_clause (one_hot);
        for (std::size_t a = 0; a < one_hot.size (); ++a)
          for (std::size_t b = a + 1; b < one_hot.size (); ++b)
            enc.add_clause ({ negate (one_hot[a]), negate (one_hot[b]) });
        encoding.ints.push_back (std::move (one_hot));
      }

    // One indicator per combination; substitution state per copy.
    std::vector<Lit> copy_indicators;
    copy_indicators.reserve (product == 0 ? 1 : product);
    std::vector<int> bool_subst (nb, -1);
    std::vector<std::size_t> int_subst (ni, 0);
    std::vector<bool> int_fixed (ni, false);

    const std::size_t combinations = quantifiers.empty () ? 1 : product;
    for (std::size_t step = 0; step < combinations; ++step)
      {
        std::size_t residual = step;
        for (std::size_t i = 0; i < quantifiers.size (); ++i)
          {
            const std::size_t choice = residual % option_counts[i];
            residual /= option_counts[i];
            const QVar &q = quantifiers[i];
            if (q.integer)
              {
                int_subst[static_cast<std::size_t> (q.id)] = choice;
                int_fixed[static_cast<std::size_t> (q.id)] = true;
              }
            else
              bool_subst[static_cast<std::size_t> (q.id)] = choice != 0 ? 1 : 0;
          }
        const Lit indicator = enc.fresh_lit ();
        copy_indicators.push_back (indicator);
        if (!emit_copy (enc, encoding, bool_subst, int_subst, int_fixed, indicator))
          return false;
        for (std::size_t i = 0; i < nb; ++i)
          bool_subst[i] = -1;
        for (std::size_t i = 0; i < ni; ++i)
          int_fixed[i] = false;
      }

    // Fold copies inside-out: exists = OR, forall = AND (Tseitin).
    std::vector<Lit> current = copy_indicators;
    std::vector<std::size_t> strides (quantifiers.size () + 1, 1);
    for (std::size_t i = 0; i < quantifiers.size (); ++i)
      strides[i + 1] = strides[i] * option_counts[i];
    for (std::size_t qi = 0; qi < quantifiers.size (); ++qi)
      {
        std::vector<Lit> next;
        const std::size_t block = strides[qi];
        const std::size_t count = option_counts[qi];
        for (std::size_t base = 0; base < current.size (); base += block * count)
          for (std::size_t b = 0; b < block; ++b)
            {
              const Lit folded = enc.fresh_lit ();
              if (quantifiers[qi].exists)
                {
                  // folded <-> OR: (¬child ∨ folded) + (¬folded ∨ children).
                  Clause back;
                  back.push_back (negate (folded));
                  for (std::size_t k = 0; k < count; ++k)
                    {
                      enc.add_clause (
                          { negate (current[base + b + k * block]), folded });
                      back.push_back (current[base + b + k * block]);
                    }
                  enc.add_clause (std::move (back));
                }
              else
                {
                  // folded <-> AND: (¬folded ∨ child) + (folded ∨ ¬children).
                  Clause back;
                  back.push_back (folded);
                  for (std::size_t k = 0; k < count; ++k)
                    {
                      enc.add_clause (
                          { negate (folded), current[base + b + k * block] });
                      back.push_back (negate (current[base + b + k * block]));
                    }
                  enc.add_clause (std::move (back));
                }
              next.push_back (folded);
            }
        current = std::move (next);
      }
    for (Lit lit : current)
      enc.add_clause ({ lit });

    out = enc.take_clauses ();
    last_encoding_ = std::move (encoding);
    has_encoding_ = true;
    return true;
  }

  /// Emits the matrix under one substitution, guarded by `indicator`.
  bool emit_copy (theory::Encoder &enc, const Encoding &encoding,
                  const std::vector<int> &bool_subst,
                  const std::vector<std::size_t> &int_subst,
                  const std::vector<bool> &int_fixed, Lit indicator)
  {
    for (const auto &clause : matrix_)
      {
        Clause instantiated;
        bool satisfied = false;
        for (const auto &[var, negated] : clause)
          {
            const int sub = bool_subst[static_cast<std::size_t> (var)];
            if (sub < 0)
              instantiated.push_back (negated ? negate (encoding.bools[static_cast<std::size_t> (var)])
                                              : encoding.bools[static_cast<std::size_t> (var)]);
            else if ((sub != 0) != negated)
              {
                satisfied = true;
                break;
              }
          }
        if (satisfied)
          continue;
        if (instantiated.empty ())
          {
            enc.add_clause ({ negate (indicator) });
            continue;
          }
        instantiated.push_back (negate (indicator));
        enc.add_clause (std::move (instantiated));
      }
    for (const IntEq &eq : int_eqs_)
      emit_atom (enc, encoding, int_subst, int_fixed, eq, indicator);
    return true;
  }

  void emit_atom (theory::Encoder &enc, const Encoding &encoding,
                  const std::vector<std::size_t> &int_subst,
                  const std::vector<bool> &int_fixed, const IntEq &eq,
                  Lit indicator)
  {
    if (eq.other >= 0)
      {
        const bool a_fixed = int_fixed[static_cast<std::size_t> (eq.var)];
        const bool b_fixed = int_fixed[static_cast<std::size_t> (eq.other)];
        if (a_fixed && b_fixed)
          {
            const std::int64_t a =
                int_domains_[static_cast<std::size_t> (eq.var)]
                             [int_subst[static_cast<std::size_t> (eq.var)]];
            const std::int64_t b =
                int_domains_[static_cast<std::size_t> (eq.other)]
                             [int_subst[static_cast<std::size_t> (eq.other)]];
            if (a != b)
              enc.add_clause ({ negate (indicator) });
            return;
          }
        if (a_fixed && !b_fixed)
          {
            pin_value (enc, encoding, eq.other,
                       int_domains_[static_cast<std::size_t> (eq.var)]
                                    [int_subst[static_cast<std::size_t> (eq.var)]],
                       indicator);
            return;
          }
        if (b_fixed && !a_fixed)
          {
            pin_value (enc, encoding, eq.var,
                       int_domains_[static_cast<std::size_t> (eq.other)]
                                    [int_subst[static_cast<std::size_t> (eq.other)]],
                       indicator);
            return;
          }
        equate_vars (enc, encoding, eq.var, eq.other, indicator);
        return;
      }
    if (int_fixed[static_cast<std::size_t> (eq.var)])
      {
        if (int_domains_[static_cast<std::size_t> (eq.var)]
                         [int_subst[static_cast<std::size_t> (eq.var)]] != eq.value)
          enc.add_clause ({ negate (indicator) });
        return;
      }
    pin_value (enc, encoding, eq.var, eq.value, indicator);
  }

  /// Under `indicator`, pins a free variable to `value` (or kills the copy
  /// when the value is outside the domain).
  void pin_value (theory::Encoder &enc, const Encoding &encoding, int var,
                  std::int64_t value, Lit indicator)
  {
    const int index = domain_index (var, value);
    if (index < 0)
      {
        enc.add_clause ({ negate (indicator) });
        return;
      }
    enc.add_clause (
        { negate (indicator),
          encoding.ints[static_cast<std::size_t> (var)][static_cast<std::size_t> (index)] });
  }

  /// Under `indicator`, equates two free variables. Domains must agree
  /// element-wise; with one-hotness, per-value coincidence clauses plus the
  /// reverse implications are exact.
  void equate_vars (theory::Encoder &enc, const Encoding &encoding, int a, int b,
                    Lit indicator)
  {
    const std::vector<Lit> &da = encoding.ints[static_cast<std::size_t> (a)];
    const std::vector<Lit> &db = encoding.ints[static_cast<std::size_t> (b)];
    const std::vector<std::int64_t> &va = int_domains_[static_cast<std::size_t> (a)];
    const std::vector<std::int64_t> &vb = int_domains_[static_cast<std::size_t> (b)];
    for (std::size_t i = 0; i < da.size (); ++i)
      for (std::size_t j = 0; j < db.size (); ++j)
        {
          // (copy ∧ a=i ∧ b=j) -> (values equal): kill mismatched pairs.
          if (va[i] != vb[j])
            enc.add_clause ({ negate (indicator), negate (da[i]), negate (db[j]) });
        }
    for (std::size_t i = 0; i < da.size (); ++i)
      {
        // (copy ∧ a=i) -> b holds the same value (when present).
        int j = domain_index (b, va[i]);
        if (j < 0)
          enc.add_clause ({ negate (indicator), negate (da[i]) });
        else
          enc.add_clause (
              { negate (indicator), negate (da[i]), db[static_cast<std::size_t> (j)] });
      }
    for (std::size_t j = 0; j < db.size (); ++j)
      {
        int i = domain_index (a, vb[j]);
        if (i < 0)
          enc.add_clause ({ negate (indicator), negate (db[j]) });
        else
          enc.add_clause (
              { negate (indicator), negate (db[j]), da[static_cast<std::size_t> (i)] });
      }
  }

  void extract_model (const Assignment &assignment)
  {
    model_bool_.assign (bool_vars_.size (), false);
    model_int_.assign (int_domains_.size (), 0);
    for (std::size_t i = 0; i < int_domains_.size (); ++i)
      model_int_[i] = int_domains_[i].front ();
    if (!has_encoding_)
      return;
    for (std::size_t v = 0; v < bool_vars_.size (); ++v)
      if (!is_quantified_bool (static_cast<int> (v)) &&
          assignment.get_literal (last_encoding_.bools[v]) == Value::TRUE)
        model_bool_[v] = true;
    for (std::size_t v = 0; v < int_domains_.size (); ++v)
      {
        if (is_quantified_int (static_cast<int> (v)))
          continue;
        const std::vector<Lit> &one_hot = last_encoding_.ints[v];
        for (std::size_t i = 0; i < one_hot.size (); ++i)
          if (assignment.get_literal (one_hot[i]) == Value::TRUE)
            {
              model_int_[v] = int_domains_[v][i];
              break;
            }
      }
  }

  CNF cnf_{};
  std::map<std::string, int> bool_vars_;
  std::map<std::string, int> int_vars_;
  std::vector<std::vector<std::int64_t>> int_domains_;
  std::vector<Block> blocks_;
  std::vector<std::vector<std::pair<int, bool>>> matrix_;
  std::vector<IntEq> int_eqs_;
  std::vector<bool> model_bool_;
  std::vector<std::int64_t> model_int_;
  Encoding last_encoding_;
  bool has_encoding_ = false;
  std::size_t expansion_cap_ = 4096;
};

} // namespace satie::quant
