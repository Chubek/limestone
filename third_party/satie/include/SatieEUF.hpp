#pragma once

#include <deque>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"

namespace satie::euf
{

/// Equality with uninterpreted functions (complete, via congruence closure).
///
/// Terms are variables, constants (0-arity functions), and function
/// applications. `check` runs congruence closure over the asserted
/// equalities and reports UNSAT when a disequality is violated. The Boolean
/// problem is checked first as a layered filter (no theory propagation back
/// into the SAT solver yet): with no theory constraints this reduces to the
/// Boolean abstraction, preserving the registry behavior.
class EUFSolver
{
public:
  EUFSolver () = default;
  explicit EUFSolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "EUF";
  }

  /// `load` replaces the Boolean problem; declared terms and asserted
  /// equalities are preserved. Use `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    funcs_.clear ();
    terms_.clear ();
    equalities_.clear ();
    disequalities_.clear ();
    parent_.clear ();
  }

  int add_func (const std::string &name, int arity)
  {
    if (arity < 0)
      throw std::invalid_argument ("EUF arity must be non-negative");
    if (funcs_.count (name) != 0)
      throw std::invalid_argument ("EUF function is already declared: " + name);
    const int id = static_cast<int> (funcs_.size ());
    funcs_.emplace (name, Func{ id, arity });
    return id;
  }

  int add_const (const std::string &name)
  {
    const int func = add_func (name, 0);
    const int term = static_cast<int> (terms_.size ());
    terms_.push_back (Term{ func, {}, name });
    return term;
  }

  int add_var (const std::string &name)
  {
    const int id = static_cast<int> (terms_.size ());
    terms_.push_back (Term{ -1, {}, name });
    return id;
  }

  int mk_app (int func, const std::vector<int> &args)
  {
    require_func (func);
    if (static_cast<int> (args.size ()) != arity_of (func))
      throw std::invalid_argument ("EUF application arity mismatch");
    for (int arg : args)
      require_term (arg);
    const int id = static_cast<int> (terms_.size ());
    terms_.push_back (Term{ func, args, {} });
    return id;
  }

  void add_eq (int lhs, int rhs)
  {
    require_term (lhs);
    require_term (rhs);
    equalities_.push_back ({ lhs, rhs });
  }

  void add_diseq (int lhs, int rhs)
  {
    require_term (lhs);
    require_term (rhs);
    disequalities_.push_back ({ lhs, rhs });
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    if (!close ())
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    return { SolveStatus::SAT, boolean.assignment };
  }

  /// Canonical representative of `term` after the last `check`.
  int representative (int term) const
  {
    require_term (term);
    int root = term;
    while (parent_[static_cast<std::size_t> (root)] != root)
      root = parent_[static_cast<std::size_t> (root)];
    return root;
  }

  bool equal_terms (int lhs, int rhs) const
  {
    return representative (lhs) == representative (rhs);
  }

private:
  struct Func
  {
    int id = -1;
    int arity = 0;
  };
  struct Term
  {
    int func = -1; // -1 for free variables
    std::vector<int> args;
    std::string var_name;
  };

  void require_func (int func) const
  {
    if (func < 0 || func >= static_cast<int> (funcs_.size ()))
      throw std::invalid_argument ("unknown EUF function id");
  }

  int arity_of (int func) const
  {
    for (const auto &[name, f] : funcs_)
      if (f.id == func)
        return f.arity;
    throw std::invalid_argument ("unknown EUF function id");
  }

  void require_term (int term) const
  {
    if (term < 0 || term >= static_cast<int> (terms_.size ()))
      throw std::invalid_argument ("unknown EUF term id");
  }

  int find_root (int term)
  {
    int root = term;
    while (parent_[root] != root)
      root = parent_[root];
    while (parent_[term] != root)
      {
        int next = parent_[term];
        parent_[term] = root;
        term = next;
      }
    return root;
  }

  std::vector<int> signature_of (int term)
  {
    const Term &node = terms_[static_cast<std::size_t> (term)];
    std::vector<int> key;
    key.reserve (node.args.size () + 1);
    key.push_back (node.func);
    for (int arg : node.args)
      key.push_back (find_root (arg));
    return key;
  }

  void merge_into (int child_root, int parent_root, std::deque<int> &pending,
                   std::vector<std::vector<int>> &uses)
  {
    if (child_root == parent_root)
      return;
    parent_[static_cast<std::size_t> (child_root)] = parent_root;
    for (int watcher : uses[static_cast<std::size_t> (child_root)])
      {
        uses[static_cast<std::size_t> (parent_root)].push_back (watcher);
        pending.push_back (watcher);
      }
    uses[static_cast<std::size_t> (child_root)].clear ();
  }

  /// Congruence closure. Returns false when the equalities are inconsistent
  /// with a disequality (or collapse a variable; variables are never merged
  /// with anything but themselves unless equated explicitly, which union
  /// handles uniformly).
  bool close ()
  {
    const std::size_t n = terms_.size ();
    parent_.resize (n);
    std::iota (parent_.begin (), parent_.end (), 0);

    std::vector<std::vector<int>> uses (n);
    for (std::size_t t = 0; t < n; ++t)
      for (int arg : terms_[t].args)
        uses[static_cast<std::size_t> (arg)].push_back (static_cast<int> (t));

    std::map<std::vector<int>, int> signatures;
    std::deque<int> pending;
    for (std::size_t t = 0; t < n; ++t)
      if (terms_[t].func >= 0)
        pending.push_back (static_cast<int> (t));

    auto process = [&] (int term) {
      std::vector<int> key = signature_of (term);
      auto it = signatures.find (key);
      if (it == signatures.end ())
        {
          signatures.emplace (std::move (key), term);
          return;
        }
      const int other = it->second;
      const int root = find_root (term);
      const int other_root = find_root (other);
      if (root == other_root)
        return;
      merge_into (other_root, root, pending, uses);
      signatures[key] = root;
    };

    while (!pending.empty ())
      {
        process (pending.front ());
        pending.pop_front ();
      }
    for (const auto &[lhs, rhs] : equalities_)
      {
        const int left = find_root (lhs);
        const int right = find_root (rhs);
        if (left != right)
          {
            merge_into (right, left, pending, uses);
            while (!pending.empty ())
              {
                process (pending.front ());
                pending.pop_front ();
              }
          }
      }
    for (const auto &[lhs, rhs] : disequalities_)
      if (find_root (lhs) == find_root (rhs))
        return false;
    return true;
  }

  CNF cnf_{};
  std::map<std::string, Func> funcs_;
  std::vector<Term> terms_;
  std::vector<std::pair<int, int>> equalities_;
  std::vector<std::pair<int, int>> disequalities_;
  std::vector<int> parent_;
};

} // namespace satie::euf
