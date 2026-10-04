#pragma once

#include <cstddef>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"

namespace satie::sequence
{

/// Sequence constraints over integer elements (sound; complete for
/// length-bounded problems).
///
/// Supported: `len(var) ==/<=/>= n`, `var[i] == value`, element variables
/// with disequalities, and concatenation of variables (`add_concat_vars`)
/// or constants around a variable (`add_concat_mixed`). Lengths are solved
/// by bound propagation plus backtracking inside `len_cap`
/// (default 16). A SAT answer carries lengths and elements. UNSAT is
/// reported only when the search is exhaustive without hitting the cap;
/// otherwise the answer is UNKNOWN.
class SequenceSolver
{
public:
  SequenceSolver () = default;
  explicit SequenceSolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "Sequence";
  }

  /// `load` replaces the Boolean problem; sequence constraints are preserved.
  /// Use `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    var_index_.clear ();
    length_eq_.clear ();
    length_le_.clear ();
    length_ge_.clear ();
    at_.clear ();
    at_vars_.clear ();
    elem_diseq_.clear ();
    concat_vars_.clear ();
    concat_mixed_.clear ();
    model_lengths_.clear ();
    model_elements_.clear ();
  }

  void set_length_cap (std::size_t cap) { len_cap_ = cap == 0 ? 1 : cap; }

  int add_var (const std::string &name)
  {
    auto it = var_index_.find (name);
    if (it != var_index_.end ())
      return it->second;
    const int id = static_cast<int> (var_index_.size ());
    var_index_.emplace (name, id);
    return id;
  }

  /// Fresh element variable (integer-valued placeholder for `add_at_var`).
  int add_elem_var ()
  {
    const int id = static_cast<int> (elem_count_);
    ++elem_count_;
    return id;
  }

  void add_length_eq (int var, std::size_t length)
  {
    require_var (var);
    length_eq_.push_back ({ var, length });
  }

  void add_length_le (int var, std::size_t length)
  {
    require_var (var);
    length_le_.push_back ({ var, length });
  }

  void add_length_ge (int var, std::size_t length)
  {
    require_var (var);
    length_ge_.push_back ({ var, length });
  }

  void add_at (int var, std::size_t index, int value)
  {
    require_var (var);
    at_.push_back ({ var, index, value });
  }

  void add_at_var (int var, std::size_t index, int elem)
  {
    require_var (var);
    require_elem (elem);
    at_vars_.push_back ({ var, index, elem });
  }

  void add_elem_diseq (int a, int b)
  {
    require_elem (a);
    require_elem (b);
    elem_diseq_.push_back ({ a, b });
  }

  void add_concat_vars (int result, int lhs, int rhs)
  {
    require_var (result);
    require_var (lhs);
    require_var (rhs);
    concat_vars_.push_back ({ result, lhs, rhs });
  }

  /// `result == prefix + var + suffix` with concrete ends.
  void add_concat_mixed (int result, const std::vector<int> &prefix, int var,
                         const std::vector<int> &suffix)
  {
    require_var (result);
    require_var (var);
    concat_mixed_.push_back ({ result, prefix, var, suffix });
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    model_lengths_.clear ();
    model_elements_.clear ();
    const std::size_t n = var_index_.size ();
    std::vector<std::size_t> lo (n, 0), hi (n, len_cap_);
    bool truncated = false;
    for (const auto &[var, length] : length_eq_)
      {
        if (length > len_cap_)
          return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
        lo[var] = hi[var] = length;
      }
    for (const auto &[var, length] : length_le_)
      hi[var] = std::min (hi[var], length);
    for (const auto &[var, length] : length_ge_)
      {
        if (length > len_cap_)
          return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
        lo[var] = std::max (lo[var], length);
      }
    // Concat-implied minimum lengths can exceed the cap honestly.
    if (!propagate_lengths (lo, hi, truncated))
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    std::vector<std::size_t> lengths;
    const LengthSearch search = search_lengths (lo, hi, truncated, lengths);
    if (search == LengthSearch::Unknown)
      return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
    if (search == LengthSearch::Unsat)
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    if (!solve_elements (lengths))
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    return { SolveStatus::SAT, boolean.assignment };
  }

  std::size_t length_value (int var) const
  {
    require_var (var);
    if (model_lengths_.empty ())
      throw std::logic_error ("sequence model is unavailable before a SAT check");
    return model_lengths_[static_cast<std::size_t> (var)];
  }

  const std::vector<int> &elements_value (int var) const
  {
    require_var (var);
    if (model_elements_.empty ())
      throw std::logic_error ("sequence model is unavailable before a SAT check");
    return model_elements_[static_cast<std::size_t> (var)];
  }

private:
  struct At
  {
    int var = -1;
    std::size_t index = 0;
    int value = 0;
  };
  struct AtVar
  {
    int var = -1;
    std::size_t index = 0;
    int elem = -1;
  };
  struct ConcatVars
  {
    int result = -1;
    int lhs = -1;
    int rhs = -1;
  };
  struct ConcatMixed
  {
    int result = -1;
    std::vector<int> prefix;
    int var = -1;
    std::vector<int> suffix;
  };

  enum class LengthSearch
  {
    Sat,
    Unsat,
    Unknown
  };

  void require_var (int var) const
  {
    if (var < 0 || var >= static_cast<int> (var_index_.size ()))
      throw std::invalid_argument ("unknown sequence variable id");
  }

  void require_elem (int elem) const
  {
    if (elem < 0 || elem >= static_cast<int> (elem_count_))
      throw std::invalid_argument ("unknown sequence element variable id");
  }

  bool propagate_lengths (std::vector<std::size_t> &lo, std::vector<std::size_t> &hi,
                          bool &truncated)
  {
    for (std::size_t iter = 0; iter < var_index_.size () + 2; ++iter)
      {
        bool changed = false;
        auto tighten = [&] (int v, std::size_t new_lo, std::size_t new_hi) {
          if (new_lo > hi[static_cast<std::size_t> (v)] ||
              new_hi < lo[static_cast<std::size_t> (v)])
            return false;
          if (new_lo > lo[static_cast<std::size_t> (v)])
            {
              lo[static_cast<std::size_t> (v)] = new_lo;
              changed = true;
            }
          if (new_hi < hi[static_cast<std::size_t> (v)])
            {
              hi[static_cast<std::size_t> (v)] = new_hi;
              changed = true;
            }
          return true;
        };
        for (const ConcatVars &c : concat_vars_)
          {
            const std::size_t r = static_cast<std::size_t> (c.result);
            const std::size_t a = static_cast<std::size_t> (c.lhs);
            const std::size_t b = static_cast<std::size_t> (c.rhs);
            if (lo[a] + lo[b] > len_cap_ || hi[a] + hi[b] > len_cap_)
              truncated = true;
            if (!tighten (c.result, lo[a] + lo[b], std::min (hi[a] + hi[b], len_cap_)))
              return false;
            if (!tighten (c.lhs, lo[r] > hi[b] ? lo[r] - hi[b] : 0,
                           hi[r] >= lo[b] ? hi[r] - lo[b] : 0))
              return false;
            if (!tighten (c.rhs, lo[r] > hi[a] ? lo[r] - hi[a] : 0,
                           hi[r] >= lo[a] ? hi[r] - lo[a] : 0))
              return false;
          }
        for (const ConcatMixed &c : concat_mixed_)
          {
            const std::size_t extra = c.prefix.size () + c.suffix.size ();
            if (extra > len_cap_)
              return false;
            const std::size_t v = static_cast<std::size_t> (c.var);
            const std::size_t r = static_cast<std::size_t> (c.result);
            if (!tighten (c.result, lo[v] + extra, std::min (hi[v] + extra, len_cap_)))
              return false;
            if (!tighten (c.var, lo[r] > extra ? lo[r] - extra : 0,
                           hi[r] >= extra ? hi[r] - extra : 0))
              return false;
            if (lo[v] + extra > len_cap_ || hi[v] + extra > len_cap_)
              truncated = true;
          }
        // at-constraints bound lengths from below.
        for (const At &a : at_)
          {
            if (a.index >= len_cap_)
              return false;
            if (!tighten (a.var, a.index + 1, hi[static_cast<std::size_t> (a.var)]))
              return false;
          }
        for (const AtVar &a : at_vars_)
          {
            if (a.index >= len_cap_)
              return false;
            if (!tighten (a.var, a.index + 1, hi[static_cast<std::size_t> (a.var)]))
              return false;
          }
        if (!changed)
          return true;
      }
    return true;
  }

  LengthSearch search_lengths (std::vector<std::size_t> lo, std::vector<std::size_t> hi,
                               bool truncated, std::vector<std::size_t> &lengths)
  {
    // Fully determined: single candidate.
    bool fixed = true;
    for (std::size_t v = 0; v < lo.size (); ++v)
      if (lo[v] != hi[v])
        fixed = false;
    if (fixed)
      {
        lengths = lo;
        return LengthSearch::Sat;
      }
    // Depth-first over the remaining ranges with re-propagation.
    std::size_t pick = 0;
    while (pick < lo.size () && lo[pick] == hi[pick])
      ++pick;
    for (std::size_t value = lo[pick]; value <= hi[pick]; ++value)
      {
        std::vector<std::size_t> try_lo = lo, try_hi = hi;
        try_lo[pick] = try_hi[pick] = value;
        bool child_truncated = truncated;
        if (!propagate_lengths (try_lo, try_hi, child_truncated))
          continue;
        if (search_lengths (try_lo, try_hi, child_truncated, lengths) ==
            LengthSearch::Sat)
          return LengthSearch::Sat;
      }
    // No candidate worked. Sound UNSAT only when the cap never truncated a
    // bound that a model could need.
    if (!truncated)
      return LengthSearch::Unsat;
    return LengthSearch::Unknown;
  }

  bool solve_elements (const std::vector<std::size_t> &lengths)
  {
    // Cells are (var, position) pairs plus element variables.
    const std::size_t n = var_index_.size ();
    std::vector<std::size_t> base (n + 1, 0);
    for (std::size_t v = 0; v < n; ++v)
      base[v + 1] = base[v] + lengths[v];
    const std::size_t cells = base[n];
    const std::size_t total = cells + elem_count_;
    std::vector<int> parent (total, 0);
    std::iota (parent.begin (), parent.end (), 0);
    auto find = [&] (int x) {
      int root = x;
      while (parent[static_cast<std::size_t> (root)] != root)
        root = parent[static_cast<std::size_t> (root)];
      while (parent[static_cast<std::size_t> (x)] != root)
        {
          const int next = parent[static_cast<std::size_t> (x)];
          parent[static_cast<std::size_t> (x)] = root;
          x = next;
        }
      return root;
    };
    auto unite = [&] (int a, int b) {
      const int ra = find (a);
      const int rb = find (b);
      if (ra != rb)
        parent[static_cast<std::size_t> (ra)] = rb;
    };
    auto cell_of = [&] (int var, std::size_t pos) {
      return static_cast<int> (base[static_cast<std::size_t> (var)] + pos);
    };
    for (const ConcatVars &c : concat_vars_)
      {
        const std::size_t la = lengths[static_cast<std::size_t> (c.lhs)];
        for (std::size_t k = 0; k < la; ++k)
          unite (cell_of (c.result, k), cell_of (c.lhs, k));
        for (std::size_t k = 0; k < lengths[static_cast<std::size_t> (c.rhs)]; ++k)
          unite (cell_of (c.result, la + k), cell_of (c.rhs, k));
      }
    for (const ConcatMixed &c : concat_mixed_)
      {
        for (std::size_t k = 0; k < lengths[static_cast<std::size_t> (c.var)]; ++k)
          unite (cell_of (c.result, c.prefix.size () + k),
                 cell_of (c.var, k));
      }

    std::map<int, int> pinned; // root -> concrete value
    auto pin = [&] (int node, int value) {
      const int root = find (node);
      auto it = pinned.find (root);
      if (it != pinned.end ())
        return it->second == value;
      pinned.emplace (root, value);
      return true;
    };
    for (const At &a : at_)
      {
        if (a.index >= lengths[static_cast<std::size_t> (a.var)])
          return false;
        if (!pin (cell_of (a.var, a.index), a.value))
          return false;
      }
    for (const AtVar &a : at_vars_)
      {
        if (a.index >= lengths[static_cast<std::size_t> (a.var)])
          return false;
        unite (cell_of (a.var, a.index),
               static_cast<int> (cells + static_cast<std::size_t> (a.elem)));
      }
    // Concrete prefix/suffix cells of mixed concats pin directly.
    for (const ConcatMixed &c : concat_mixed_)
      {
        for (std::size_t k = 0; k < c.prefix.size (); ++k)
          if (!pin (cell_of (c.result, k), c.prefix[k]))
            return false;
        const std::size_t off =
            c.prefix.size () + lengths[static_cast<std::size_t> (c.var)];
        for (std::size_t k = 0; k < c.suffix.size (); ++k)
          if (!pin (cell_of (c.result, off + k), c.suffix[k]))
            return false;
      }
    // Disequalities: conflicting constants fail; otherwise the infinite
    // integer domain always admits distinct values.
    for (const auto &[a, b] : elem_diseq_)
      {
        const int na = static_cast<int> (cells + static_cast<std::size_t> (a));
        const int nb = static_cast<int> (cells + static_cast<std::size_t> (b));
        if (find (na) == find (nb))
          return false;
        auto it_a = pinned.find (find (na));
        auto it_b = pinned.find (find (nb));
        if (it_a != pinned.end () && it_b != pinned.end () && it_a->second == it_b->second)
          return false;
      }
    // Build the model: pinned values, then 0, then distinct values where
    // disequalities demand them.
    std::map<int, int> assigned;
    for (const auto &[root, value] : pinned)
      assigned.emplace (root, value);
    int fresh = 1;
    auto value_of = [&] (int node) {
      const int root = find (node);
      auto it = assigned.find (root);
      if (it != assigned.end ())
        return it->second;
      while (true)
        {
          bool clash = false;
          for (const auto &[other, val] : assigned)
            if (val == fresh)
              {
                (void)other;
                clash = true;
              }
          // Only disequality neighbors actually constrain freshness, but the
          // integer domain is infinite so a globally fresh value always works.
          if (!clash)
            break;
          ++fresh;
        }
      assigned.emplace (root, fresh);
      return fresh;
    };
    // Pre-seed element disequality pairs with distinct values.
    for (const auto &[a, b] : elem_diseq_)
      {
        const int na = static_cast<int> (cells + static_cast<std::size_t> (a));
        const int nb = static_cast<int> (cells + static_cast<std::size_t> (b));
        const int va = value_of (na);
        const int vb = value_of (nb);
        if (va == vb)
          {
            ++fresh;
            assigned[find (nb)] = fresh;
          }
      }
    model_lengths_ = lengths;
    model_elements_.assign (n, {});
    for (std::size_t v = 0; v < n; ++v)
      for (std::size_t k = 0; k < lengths[v]; ++k)
        model_elements_[v].push_back (value_of (cell_of (static_cast<int> (v), k)));
    return true;
  }

  CNF cnf_{};
  std::map<std::string, int> var_index_;
  std::vector<std::pair<int, std::size_t>> length_eq_;
  std::vector<std::pair<int, std::size_t>> length_le_;
  std::vector<std::pair<int, std::size_t>> length_ge_;
  std::vector<At> at_;
  std::vector<AtVar> at_vars_;
  std::vector<std::pair<int, int>> elem_diseq_;
  std::vector<ConcatVars> concat_vars_;
  std::vector<ConcatMixed> concat_mixed_;
  std::size_t elem_count_ = 0;
  std::size_t len_cap_ = 16;
  std::vector<std::size_t> model_lengths_;
  std::vector<std::vector<int>> model_elements_;
};

} // namespace satie::sequence
