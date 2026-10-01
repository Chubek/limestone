#pragma once

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"

namespace satie::bag
{

/// Multiset (bag) constraints over a finite element universe (sound;
/// complete when multiplicities stay within the multiplicity bound).
///
/// Each (bag, element) pair carries a multiplicity in `0..bound`
/// (`set_multiplicity_bound`, default 4). Supported: per-element count
/// `==/<=/>=`, total cardinality `==/<=/>=`, subbag, and disjoint union
/// (`result == lhs + rhs` element-wise). `check` enumerates multiplicity
/// assignments inside the bound (cell cap, else UNKNOWN). An exhaustive
/// pass is a sound UNSAT certificate only when every bag's total is
/// explicitly bounded above within the multiplicity bound (then no model
/// can use a larger multiplicity); otherwise exhaustion yields UNKNOWN.
class BagSolver
{
public:
  BagSolver () = default;
  explicit BagSolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "Bag";
  }

  /// `load` replaces the Boolean problem; bag constraints are preserved.
  /// Use `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    elements_.clear ();
    bags_.clear ();
    counts_.clear ();
    cards_.clear ();
    subbags_.clear ();
    unions_.clear ();
    model_.clear ();
  }

  void set_multiplicity_bound (int bound)
  {
    if (bound < 0 || bound > 64)
      throw std::invalid_argument ("bag multiplicity bound must be in 0..64");
    bound_ = bound;
  }

  void set_search_cap (std::size_t cap) { search_cap_ = cap == 0 ? 1 : cap; }

  int add_element (const std::string &name)
  {
    auto it = elements_.find (name);
    if (it != elements_.end ())
      return it->second;
    const int id = static_cast<int> (elements_.size ());
    elements_.emplace (name, id);
    return id;
  }

  int add_bag (const std::string &name)
  {
    auto it = bags_.find (name);
    if (it != bags_.end ())
      return it->second;
    const int id = static_cast<int> (bags_.size ());
    bags_.emplace (name, id);
    return id;
  }

  void add_count_eq (int bag, int element, int count) { add_count (bag, element, CountOp::Eq, count); }
  void add_count_le (int bag, int element, int count) { add_count (bag, element, CountOp::Le, count); }
  void add_count_ge (int bag, int element, int count) { add_count (bag, element, CountOp::Ge, count); }

  void add_card_eq (int bag, int count) { add_card (bag, CountOp::Eq, count); }
  void add_card_le (int bag, int count) { add_card (bag, CountOp::Le, count); }
  void add_card_ge (int bag, int count) { add_card (bag, CountOp::Ge, count); }

  void add_subbag (int a, int b)
  {
    require_bag (a);
    require_bag (b);
    subbags_.push_back ({ a, b });
  }

  /// Disjoint-style union: `result[e] == lhs[e] + rhs[e]` for every element.
  void add_disjoint_union (int result, int lhs, int rhs)
  {
    require_bag (result);
    require_bag (lhs);
    require_bag (rhs);
    unions_.push_back ({ result, lhs, rhs });
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    model_.clear ();
    const std::size_t cells = bags_.size () * elements_.size ();
    // cells^…: total assignments are (bound+1)^cells; cap the product.
    std::size_t total = 1;
    for (std::size_t i = 0; i < cells; ++i)
      {
        if (total > search_cap_ / (static_cast<std::size_t> (bound_) + 1))
          return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
        total *= static_cast<std::size_t> (bound_) + 1;
      }
    std::vector<int> counts (cells, 0);
    for (std::size_t step = 0; step < total; ++step)
      {
        if (satisfied (counts))
          {
            model_ = counts;
            return { SolveStatus::SAT, boolean.assignment };
          }
        for (std::size_t i = 0; i < cells; ++i)
          {
            if (counts[i] < bound_)
              {
                ++counts[i];
                break;
              }
            counts[i] = 0;
          }
      }
    // Exhaustive inside the bound. Sound UNSAT only when every bag's total
    // is explicitly capped within the bound (no model can escape it).
    if (bounded_above ())
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
  }

  /// Multiplicity of `element` in `bag` in the last model.
  int count_value (int bag, int element) const
  {
    require_bag (bag);
    require_element (element);
    if (model_.size () != bags_.size () * elements_.size ())
      throw std::logic_error ("bag model is unavailable before a SAT check");
    return model_[index_of (bag, element)];
  }

private:
  enum class CountOp
  {
    Eq,
    Le,
    Ge
  };
  struct Count
  {
    int bag = -1;
    int element = -1;
    CountOp op = CountOp::Eq;
    int count = 0;
  };
  struct Card
  {
    int bag = -1;
    CountOp op = CountOp::Eq;
    int count = 0;
  };

  void require_bag (int bag) const
  {
    if (bag < 0 || bag >= static_cast<int> (bags_.size ()))
      throw std::invalid_argument ("unknown bag id");
  }

  void require_element (int element) const
  {
    if (element < 0 || element >= static_cast<int> (elements_.size ()))
      throw std::invalid_argument ("unknown bag element id");
  }

  void add_count (int bag, int element, CountOp op, int count)
  {
    require_bag (bag);
    require_element (element);
    counts_.push_back ({ bag, element, op, count });
  }

  void add_card (int bag, CountOp op, int count)
  {
    require_bag (bag);
    cards_.push_back ({ bag, op, count });
  }

  std::size_t index_of (int bag, int element) const
  {
    return static_cast<std::size_t> (bag) * elements_.size () +
           static_cast<std::size_t> (element);
  }

  static bool holds (int value, CountOp op, int count)
  {
    switch (op)
      {
      case CountOp::Eq:
        return value == count;
      case CountOp::Le:
        return value <= count;
      case CountOp::Ge:
        return value >= count;
      }
    return false;
  }

  bool satisfied (const std::vector<int> &counts) const
  {
    for (const Count &c : counts_)
      if (!holds (counts[index_of (c.bag, c.element)], c.op, c.count))
        return false;
    for (const Card &c : cards_)
      {
        int total = 0;
        for (std::size_t e = 0; e < elements_.size (); ++e)
          total += counts[static_cast<std::size_t> (c.bag) * elements_.size () + e];
        if (!holds (total, c.op, c.count))
          return false;
      }
    for (const auto &[a, b] : subbags_)
      for (std::size_t e = 0; e < elements_.size (); ++e)
        if (counts[static_cast<std::size_t> (a) * elements_.size () + e] >
            counts[static_cast<std::size_t> (b) * elements_.size () + e])
          return false;
    for (const auto &[r, a, b] : unions_)
      for (std::size_t e = 0; e < elements_.size (); ++e)
        if (counts[static_cast<std::size_t> (r) * elements_.size () + e] !=
            counts[static_cast<std::size_t> (a) * elements_.size () + e] +
                counts[static_cast<std::size_t> (b) * elements_.size () + e])
          return false;
    return true;
  }

  /// True when every bag's total is explicitly capped within the
  /// multiplicity bound, so no model can use multiplicities outside the
  /// enumerated box (a total of at most `bound_` forces every multiplicity
  /// into `0..bound_`).
  bool bounded_above () const
  {
    for (std::size_t b = 0; b < bags_.size (); ++b)
      {
        bool capped = false;
        for (const Card &c : cards_)
          if (c.bag == static_cast<int> (b) &&
              (c.op == CountOp::Le || c.op == CountOp::Eq) && c.count >= 0 &&
              c.count <= bound_)
            capped = true;
        if (!capped)
          return false;
      }
    return true;
  }

  CNF cnf_{};
  std::map<std::string, int> elements_;
  std::map<std::string, int> bags_;
  std::vector<Count> counts_;
  std::vector<Card> cards_;
  std::vector<std::pair<int, int>> subbags_;
  struct Triple
  {
    int result = -1;
    int lhs = -1;
    int rhs = -1;
  };
  std::vector<Triple> unions_;
  std::vector<int> model_;
  int bound_ = 4;
  std::size_t search_cap_ = 200000;
};

} // namespace satie::bag
