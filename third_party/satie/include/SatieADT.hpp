#pragma once

#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"

namespace satie::adt
{

/// Algebraic datatypes: constructors, testers, and selectors over a single
/// sort (complete for constructor equalities; disequalities and testers are
/// decided exactly over finite (enumerated) domains and by bounded witness
/// search over infinite ones).
///
/// Terms are variables and constructor applications. `check` unifies the
/// asserted equalities with an occurs check (a cycle is UNSAT), applies
/// tester constraints, then discharges disequalities: identical normal
/// forms are UNSAT; over a finite domain (constructors all nullary) the
/// remaining variables are enumerated; over an infinite domain a greedy
/// witness search at increasing term depth decides, degrading to UNKNOWN
/// past `set_depth_cap`. The Boolean problem is checked first as a layered
/// filter.
class ADTSolver
{
public:
  ADTSolver () = default;
  explicit ADTSolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "ADT";
  }

  /// `load` replaces the Boolean problem; datatype constraints are preserved.
  /// Use `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    constructors_.clear ();
    terms_.clear ();
    equalities_.clear ();
    disequalities_.clear ();
    testers_.clear ();
    model_.clear ();
  }

  void set_depth_cap (std::size_t cap) { depth_cap_ = cap == 0 ? 1 : cap; }

  int add_constructor (const std::string &name, int arity)
  {
    if (arity < 0)
      throw std::invalid_argument ("ADT arity must be non-negative");
    if (constructors_.count (name) != 0)
      throw std::invalid_argument ("ADT constructor is already declared: " + name);
    const int id = static_cast<int> (constructors_.size ());
    constructors_.emplace (name, Constructor{ id, arity });
    return id;
  }

  int add_var (const std::string &name)
  {
    const int id = static_cast<int> (terms_.size ());
    terms_.push_back (Term{ -1, {}, name });
    return id;
  }

  int mk_app (int constructor, const std::vector<int> &args)
  {
    require_constructor (constructor);
    if (static_cast<int> (args.size ()) != arity_of (constructor))
      throw std::invalid_argument ("ADT application arity mismatch");
    for (int arg : args)
      require_term (arg);
    const int id = static_cast<int> (terms_.size ());
    terms_.push_back (Term{ constructor, args, {} });
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

  /// Asserts `is_constructor(term)`.
  void add_tester (int term, int constructor)
  {
    require_term (term);
    require_constructor (constructor);
    testers_.push_back ({ term, constructor });
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    model_.clear ();
    if (!unify ())
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    if (!apply_testers ())
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    const DiseqResult diseq = discharge_disequalities ();
    if (diseq == DiseqResult::Unsat)
      return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
    if (diseq == DiseqResult::Unknown)
      return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
    return { SolveStatus::SAT, boolean.assignment };
  }

  /// Ground constructor term assigned to `term` in the last model, in
  /// prefix notation (empty when the model stores only representatives).
  std::string value (int term) const
  {
    require_term (term);
    if (model_.empty ())
      throw std::logic_error ("ADT model is unavailable before a SAT check");
    return model_[static_cast<std::size_t> (term)];
  }

private:
  struct Constructor
  {
    int id = -1;
    int arity = 0;
  };
  struct Term
  {
    int constructor = -1; // -1 for free variables
    std::vector<int> args;
    std::string var_name;
  };

  enum class DiseqResult
  {
    Sat,
    Unsat,
    Unknown
  };

  void require_constructor (int constructor) const
  {
    if (constructor < 0 || constructor >= static_cast<int> (constructors_.size ()))
      throw std::invalid_argument ("unknown ADT constructor id");
  }

  int arity_of (int constructor) const
  {
    for (const auto &[name, c] : constructors_)
      if (c.id == constructor)
        return c.arity;
    throw std::invalid_argument ("unknown ADT constructor id");
  }

  const std::string &constructor_name (int constructor) const
  {
    for (const auto &[name, c] : constructors_)
      if (c.id == constructor)
        return name;
    throw std::invalid_argument ("unknown ADT constructor id");
  }

  void require_term (int term) const
  {
    if (term < 0 || term >= static_cast<int> (terms_.size ()))
      throw std::invalid_argument ("unknown ADT term id");
  }

  int find_root (int term) const
  {
    int root = term;
    while (parent_[static_cast<std::size_t> (root)] != root)
      root = parent_[static_cast<std::size_t> (root)];
    int cursor = term;
    while (parent_[static_cast<std::size_t> (cursor)] != root)
      {
        const int next = parent_[static_cast<std::size_t> (cursor)];
        parent_[static_cast<std::size_t> (cursor)] = root;
        cursor = next;
      }
    return root;
  }

  bool occurs (int var_root, int term)
  {
    const int root = find_root (term);
    if (root == var_root)
      return true;
    if (terms_[static_cast<std::size_t> (root)].constructor < 0)
      return false;
    for (int arg : terms_[static_cast<std::size_t> (root)].args)
      if (occurs (var_root, arg))
        return true;
    return false;
  }

  /// Unites two classes; constructor clashes fail, variable bindings check
  /// the occurs condition. Returns false on UNSAT.
  bool unite_terms (int lhs, int rhs)
  {
    const int left = find_root (lhs);
    const int right = find_root (rhs);
    if (left == right)
      return true;
    const Term &l = terms_[static_cast<std::size_t> (left)];
    const Term &r = terms_[static_cast<std::size_t> (right)];
    if (l.constructor < 0 && r.constructor < 0)
      {
        parent_[static_cast<std::size_t> (left)] = right;
        return true;
      }
    if (l.constructor < 0)
      {
        if (occurs (left, right))
          return false;
        parent_[static_cast<std::size_t> (left)] = right;
        return true;
      }
    if (r.constructor < 0)
      {
        if (occurs (right, left))
          return false;
        parent_[static_cast<std::size_t> (right)] = left;
        return true;
      }
    if (l.constructor != r.constructor)
      return false;
    parent_[static_cast<std::size_t> (left)] = right;
    for (std::size_t i = 0; i < l.args.size (); ++i)
      if (!unite_terms (l.args[i], r.args[i]))
        return false;
    return true;
  }

  bool unify ()
  {
    parent_.resize (terms_.size ());
    std::iota (parent_.begin (), parent_.end (), 0);
    for (const auto &[lhs, rhs] : equalities_)
      if (!unite_terms (lhs, rhs))
        return false;
    return true;
  }

  bool apply_testers ()
  {
    for (const auto &[term, constructor] : testers_)
      {
        const int root = find_root (term);
        const int current = terms_[static_cast<std::size_t> (root)].constructor;
        if (current < 0)
          {
            // Bind the variable to a fresh application of the constructor.
            std::vector<int> args;
            for (int i = 0; i < arity_of (constructor); ++i)
              {
                const int fresh = static_cast<int> (terms_.size ());
                terms_.push_back (Term{ -1, {}, "__tester_fresh" });
                parent_.push_back (fresh);
                args.push_back (fresh);
              }
            const int app = static_cast<int> (terms_.size ());
            terms_.push_back (Term{ constructor, args, {} });
            parent_.push_back (app);
            if (!unite_terms (root, app))
              return false;
          }
        else if (current != constructor)
          return false;
      }
    return true;
  }

  bool infinite_domain () const
  {
    for (const auto &[name, c] : constructors_)
      if (c.arity > 0)
        return true;
    // Only nullary constructors: infinite iff at least two constants... a
    // single constant is a singleton domain.
    return constructors_.size () > 1;
  }

  DiseqResult discharge_disequalities ()
  {
    // Identical normal forms fail immediately.
    for (const auto &[lhs, rhs] : disequalities_)
      if (find_root (lhs) == find_root (rhs))
        return DiseqResult::Unsat;
    if (disequalities_.empty ())
      {
        build_trivial_model ();
        return DiseqResult::Sat;
      }
    if (!infinite_domain ())
      return discharge_finite ();
    return discharge_infinite ();
  }

  DiseqResult discharge_finite ()
  {
    // Every constructor is nullary: enumerate ground assignments.
    std::vector<int> constants;
    for (const auto &[name, c] : constructors_)
      constants.push_back (c.id);
    if (constants.empty ())
      return DiseqResult::Unsat; // No ground terms at all.
    // Collect variable roots needing values.
    std::vector<int> roots;
    for (std::size_t t = 0; t < terms_.size (); ++t)
      if (terms_[t].constructor < 0 && find_root (static_cast<int> (t)) == static_cast<int> (t))
        roots.push_back (static_cast<int> (t));
    std::vector<std::size_t> choice (roots.size (), 0);
    const std::size_t total = [&] {
      std::size_t acc = 1;
      for (std::size_t i = 0; i < roots.size (); ++i)
        acc *= constants.size ();
      return acc;
    }();
    for (std::size_t step = 0; step < total; ++step)
      {
        bool ok = true;
        for (const auto &[lhs, rhs] : disequalities_)
          {
            const int vl = ground_constant (lhs, roots, choice, constants);
            const int vr = ground_constant (rhs, roots, choice, constants);
            if (vl < 0 || vr < 0 || vl == vr)
              {
                ok = false;
                break;
              }
          }
        if (ok)
          {
            build_finite_model (roots, choice, constants);
            return DiseqResult::Sat;
          }
        for (std::size_t i = 0; i < roots.size (); ++i)
          {
            if (++choice[i] < constants.size ())
              break;
            choice[i] = 0;
          }
      }
    return DiseqResult::Unsat;
  }

  int ground_constant (int term, const std::vector<int> &roots,
                       const std::vector<std::size_t> &choice,
                       const std::vector<int> &constants) const
  {
    int root = term;
    while (parent_[static_cast<std::size_t> (root)] != root)
      root = parent_[static_cast<std::size_t> (root)];
    const Term &node = terms_[static_cast<std::size_t> (root)];
    if (node.constructor >= 0)
      {
        if (!node.args.empty ())
          return -1; // Non-nullary: outside the finite fragment.
        return node.constructor;
      }
    for (std::size_t i = 0; i < roots.size (); ++i)
      if (roots[i] == root)
        return constants[choice[i]];
    return -1;
  }

  void build_finite_model (const std::vector<int> &roots,
                           const std::vector<std::size_t> &choice,
                           const std::vector<int> &constants)
  {
    model_.assign (terms_.size (), {});
    for (std::size_t t = 0; t < terms_.size (); ++t)
      {
        const int c = ground_constant (static_cast<int> (t), roots, choice, constants);
        model_[t] = c < 0 ? "?" : constructor_name (c);
      }
  }

  DiseqResult discharge_infinite ()
  {
    // Greedy witness search: assign each variable root a ground term of
    // increasing depth that avoids disequality neighbors' values and honors
    // tester constructors.
    std::map<int, std::string> assigned; // root -> ground string
    std::map<int, int> forced;           // root -> tester constructor
    for (const auto &[term, constructor] : testers_)
      forced[find_root (term)] = constructor;
    // Constructor-rooted classes already have fixed ground skeletons.
    for (std::size_t t = 0; t < terms_.size (); ++t)
      {
        const int root = find_root (static_cast<int> (t));
        if (terms_[static_cast<std::size_t> (root)].constructor >= 0 &&
            assigned.count (root) == 0)
          assigned.emplace (root, skeleton (root, 0));
      }
    for (std::size_t t = 0; t < terms_.size (); ++t)
      {
        const int root = find_root (static_cast<int> (t));
        if (terms_[static_cast<std::size_t> (root)].constructor >= 0)
          continue;
        if (assigned.count (root) != 0)
          continue;
        bool placed = false;
        for (std::size_t depth = 0; depth <= depth_cap_ && !placed; ++depth)
          {
            std::vector<std::string> candidates = witnesses_at_depth (root, depth, forced);
            for (const std::string &candidate : candidates)
              {
                if (conflicts_with_diseq (root, candidate, assigned))
                  continue;
                assigned.emplace (root, candidate);
                placed = true;
                break;
              }
          }
        if (!placed)
          return DiseqResult::Unknown;
      }
    // Verify all disequalities under the assignment.
    for (const auto &[lhs, rhs] : disequalities_)
      if (expand (find_root (lhs), assigned) == expand (find_root (rhs), assigned))
        return DiseqResult::Unknown;
    model_.assign (terms_.size (), {});
    for (std::size_t t = 0; t < terms_.size (); ++t)
      model_[t] = expand (find_root (static_cast<int> (t)), assigned);
    return DiseqResult::Sat;
  }

  /// Ground skeleton of a constructor-rooted class (variables inside become
  /// depth-0 witnesses later; here rendered as `#`).
  std::string skeleton (int root, std::size_t depth) const
  {
    const Term &node = terms_[static_cast<std::size_t> (root)];
    if (node.constructor < 0)
      return "#" + std::to_string (depth);
    std::string out = constructor_name (node.constructor);
    if (!node.args.empty ())
      {
        out += "(";
        for (std::size_t i = 0; i < node.args.size (); ++i)
          {
            if (i != 0)
              out += ",";
            int arg_root = find_root (node.args[i]);
            const Term &arg = terms_[static_cast<std::size_t> (arg_root)];
            out += arg.constructor < 0 ? "#" + std::to_string (depth + 1)
                                       : skeleton (arg_root, depth + 1);
          }
        out += ")";
      }
    return out;
  }

  std::string expand (int root, const std::map<int, std::string> &assigned) const
  {
    auto it = assigned.find (root);
    if (it != assigned.end ())
      return it->second;
    return skeleton (root, 0);
  }

  std::vector<std::string> witnesses_at_depth (int root, std::size_t depth,
                                               const std::map<int, int> &forced) const
  {
    std::vector<std::string> out;
    auto fit = forced.find (root);
    for (const auto &[name, c] : constructors_)
      {
        if (fit != forced.end () && fit->second != c.id)
          continue;
        if (depth == 0)
          {
            if (c.arity == 0)
              out.push_back (name);
            continue;
          }
        if (c.arity == 0)
          continue;
        // One witness per constructor at this depth (arguments recurse at
        // depth - 1 with the first constructor available).
        std::string witness = name + "(";
        for (int i = 0; i < c.arity; ++i)
          {
            if (i != 0)
              witness += ",";
            witness += simplest_ground (depth - 1);
          }
        witness += ")";
        out.push_back (witness);
      }
    return out;
  }

  std::string simplest_ground (std::size_t depth) const
  {
    for (const auto &[name, c] : constructors_)
      if (c.arity == 0)
        return name;
    // No constants: nest the first non-nullary constructor.
    for (const auto &[name, c] : constructors_)
      if (c.arity > 0 && depth > 0)
        {
          std::string witness = name + "(";
          for (int i = 0; i < c.arity; ++i)
            {
              if (i != 0)
                witness += ",";
              witness += simplest_ground (depth - 1);
            }
          return witness + ")";
        }
    return "?";
  }

  bool conflicts_with_diseq (int root, const std::string &candidate,
                             const std::map<int, std::string> &assigned) const
  {
    for (const auto &[lhs, rhs] : disequalities_)
      {
        const int rl = find_root (lhs);
        const int rr = find_root (rhs);
        const std::string vl = rl == root ? candidate : expand (rl, assigned);
        const std::string vr = rr == root ? candidate : expand (rr, assigned);
        // Only conclusive when both sides are fully ground (no `#`).
        if (vl.find ('#') == std::string::npos && vr.find ('#') == std::string::npos &&
            vl == vr)
          return true;
      }
    return false;
  }

  void build_trivial_model ()
  {
    model_.assign (terms_.size (), {});
    for (std::size_t t = 0; t < terms_.size (); ++t)
      model_[t] = skeleton (find_root (static_cast<int> (t)), 0);
  }

  CNF cnf_{};
  std::map<std::string, Constructor> constructors_;
  std::vector<Term> terms_;
  std::vector<std::pair<int, int>> equalities_;
  std::vector<std::pair<int, int>> disequalities_;
  std::vector<std::pair<int, int>> testers_;
  mutable std::vector<int> parent_;
  std::vector<std::string> model_;
  std::size_t depth_cap_ = 3;
};

} // namespace satie::adt
