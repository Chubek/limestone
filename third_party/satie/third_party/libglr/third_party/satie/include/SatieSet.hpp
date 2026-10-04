#pragma once

#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"
#include "SatieTheoryUtils.hpp"

namespace satie::set
{

/// Finite-set constraints (complete, via bit-blasting over the universe).
///
/// The universe is exactly the set of declared elements. Each set variable
/// becomes one SAT literal per element; union/intersection/difference are
/// Boolean connectives and cardinalities use a sequential counter, so
/// `check` is a full decision procedure for the supported operators.
/// Supported: membership, subset, equality, union/intersection/difference of
/// two sets, and cardinality `==/<=/>=`.
class SetSolver
{
public:
  SetSolver () = default;
  explicit SetSolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "Set";
  }

  /// `load` replaces the Boolean problem; set constraints are preserved.
  /// Use `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    elements_.clear ();
    sets_.clear ();
    members_.clear ();
    subsets_.clear ();
    equalities_.clear ();
    unions_.clear ();
    intersections_.clear ();
    differences_.clear ();
    cardinalities_.clear ();
    model_.clear ();
  }

  int add_element (const std::string &name)
  {
    auto it = elements_.find (name);
    if (it != elements_.end ())
      return it->second;
    const int id = static_cast<int> (elements_.size ());
    elements_.emplace (name, id);
    return id;
  }

  int add_set (const std::string &name)
  {
    auto it = sets_.find (name);
    if (it != sets_.end ())
      return it->second;
    const int id = static_cast<int> (sets_.size ());
    sets_.emplace (name, id);
    return id;
  }

  void add_member (int element, int set)
  {
    require_element (element);
    require_set (set);
    members_.push_back ({ element, set });
  }

  void add_nonmember (int element, int set)
  {
    require_element (element);
    require_set (set);
    nonmembers_.push_back ({ element, set });
  }

  void add_subset (int a, int b)
  {
    require_set (a);
    require_set (b);
    subsets_.push_back ({ a, b });
  }

  void add_eq (int a, int b)
  {
    require_set (a);
    require_set (b);
    equalities_.push_back ({ a, b });
  }

  void add_union (int result, int lhs, int rhs)
  {
    require_set (result);
    require_set (lhs);
    require_set (rhs);
    unions_.push_back ({ result, lhs, rhs });
  }

  void add_intersection (int result, int lhs, int rhs)
  {
    require_set (result);
    require_set (lhs);
    require_set (rhs);
    intersections_.push_back ({ result, lhs, rhs });
  }

  void add_difference (int result, int lhs, int rhs)
  {
    require_set (result);
    require_set (lhs);
    require_set (rhs);
    differences_.push_back ({ result, lhs, rhs });
  }

  void add_card_eq (int set, int count) { add_card (set, CountOp::Eq, count); }
  void add_card_le (int set, int count) { add_card (set, CountOp::Le, count); }
  void add_card_ge (int set, int count) { add_card (set, CountOp::Ge, count); }

  SolveResult check ()
  {
    model_.clear ();
    const std::size_t universe = elements_.size ();
    const std::size_t sets = sets_.size ();
    theory::Encoder enc (cnf_.variable_count () + 1);
    std::vector<std::vector<Lit>> bits (sets, std::vector<Lit> (universe, 0));
    for (std::size_t s = 0; s < sets; ++s)
      for (std::size_t e = 0; e < universe; ++e)
        bits[s][e] = enc.fresh_lit ();

    for (const auto &[element, set] : members_)
      enc.add_clause ({ bits[static_cast<std::size_t> (set)][static_cast<std::size_t> (element)] });
    for (const auto &[element, set] : nonmembers_)
      enc.add_clause (
          { negate (bits[static_cast<std::size_t> (set)][static_cast<std::size_t> (element)]) });
    for (const auto &[a, b] : subsets_)
      for (std::size_t e = 0; e < universe; ++e)
        enc.add_clause ({ negate (bits[static_cast<std::size_t> (a)][e]),
                          bits[static_cast<std::size_t> (b)][e] });
    for (const auto &[a, b] : equalities_)
      enc.assert_eq_bits (bits[static_cast<std::size_t> (a)],
                          bits[static_cast<std::size_t> (b)]);
    for (const auto &[r, a, b] : unions_)
      for (std::size_t e = 0; e < universe; ++e)
        {
          const Lit lor =
              enc.lor2 (bits[static_cast<std::size_t> (a)][e],
                        bits[static_cast<std::size_t> (b)][e]);
          enc.add_clause ({ negate (bits[static_cast<std::size_t> (r)][e]), lor });
          enc.add_clause ({ bits[static_cast<std::size_t> (r)][e], negate (lor) });
        }
    for (const auto &[r, a, b] : intersections_)
      for (std::size_t e = 0; e < universe; ++e)
        {
          const Lit land =
              enc.land2 (bits[static_cast<std::size_t> (a)][e],
                         bits[static_cast<std::size_t> (b)][e]);
          enc.add_clause ({ negate (bits[static_cast<std::size_t> (r)][e]), land });
          enc.add_clause ({ bits[static_cast<std::size_t> (r)][e], negate (land) });
        }
    for (const auto &[r, a, b] : differences_)
      for (std::size_t e = 0; e < universe; ++e)
        {
          const Lit land = enc.land2 (bits[static_cast<std::size_t> (a)][e],
                                       negate (bits[static_cast<std::size_t> (b)][e]));
          enc.add_clause ({ negate (bits[static_cast<std::size_t> (r)][e]), land });
          enc.add_clause ({ bits[static_cast<std::size_t> (r)][e], negate (land) });
        }
    for (const auto &[set, op, count] : cardinalities_)
      {
        const int universe_size = static_cast<int> (universe);
        if (op == CountOp::Eq)
          {
            if (count < 0 || count > universe_size)
              {
                enc.add_clause ({});
                continue;
              }
            enc.at_most_k (bits[static_cast<std::size_t> (set)], count);
            enc.at_least_k (bits[static_cast<std::size_t> (set)], count);
          }
        else if (op == CountOp::Le)
          {
            if (count < 0)
              {
                enc.add_clause ({});
                continue;
              }
            if (count < universe_size)
              enc.at_most_k (bits[static_cast<std::size_t> (set)], count);
          }
        else
          {
            if (count <= 0)
              continue;
            if (count > universe_size)
              {
                enc.add_clause ({});
                continue;
              }
            enc.at_least_k (bits[static_cast<std::size_t> (set)], count);
          }
      }

    CNF combined = cnf_;
    for (Clause &clause : enc.take_clauses ())
      combined.add_clause (std::move (clause));
    SolveResult result = solve_cdcl (combined);
    if (result.unsatisfiable ())
      return result;
    model_.assign (sets, {});
    for (std::size_t s = 0; s < sets; ++s)
      for (std::size_t e = 0; e < universe; ++e)
        if (result.assignment.get_literal (bits[s][e]) == Value::TRUE)
          model_[s].insert (static_cast<int> (e));
    return { SolveStatus::SAT, result.assignment };
  }

  /// Element ids held by `set` in the last model.
  const std::set<int> &members_value (int set) const
  {
    require_set (set);
    if (model_.size () != sets_.size ())
      throw std::logic_error ("set model is unavailable before a SAT check");
    return model_[static_cast<std::size_t> (set)];
  }

  const std::string &element_name (int element) const
  {
    for (const auto &[name, id] : elements_)
      if (id == element)
        return name;
    throw std::invalid_argument ("unknown set element id");
  }

private:
  enum class CountOp
  {
    Eq,
    Le,
    Ge
  };
  struct Card
  {
    int set = -1;
    CountOp op = CountOp::Eq;
    int count = 0;
  };

  void require_element (int element) const
  {
    if (element < 0 || element >= static_cast<int> (elements_.size ()))
      throw std::invalid_argument ("unknown set element id");
  }

  void require_set (int set) const
  {
    if (set < 0 || set >= static_cast<int> (sets_.size ()))
      throw std::invalid_argument ("unknown set id");
  }

  void add_card (int set, CountOp op, int count)
  {
    require_set (set);
    cardinalities_.push_back ({ set, op, count });
  }

  CNF cnf_{};
  std::map<std::string, int> elements_;
  std::map<std::string, int> sets_;
  std::vector<std::pair<int, int>> members_;
  std::vector<std::pair<int, int>> nonmembers_;
  std::vector<std::pair<int, int>> subsets_;
  std::vector<std::pair<int, int>> equalities_;
  struct Triple
  {
    int result = -1;
    int lhs = -1;
    int rhs = -1;
  };
  std::vector<Triple> unions_;
  std::vector<Triple> intersections_;
  std::vector<Triple> differences_;
  std::vector<Card> cardinalities_;
  std::vector<std::set<int>> model_;
};

} // namespace satie::set
