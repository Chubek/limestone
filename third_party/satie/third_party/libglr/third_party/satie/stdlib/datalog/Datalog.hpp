#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace satie::stdlib::datalog
{

using Tuple = std::vector<std::string>;

/// A Horn rule: `head :- body`. Arguments are constants (`is_var` false)
/// or variables (`is_var` true, unified by name across the rule).
struct Atom
{
  std::string relation;
  std::vector<std::string> args;
  std::vector<bool> is_var;
};

struct Rule
{
  Atom head;
  std::vector<Atom> body;
};

inline Atom atom (const std::string &relation, const std::vector<std::string> &args,
                  const std::vector<bool> &is_var)
{
  if (args.size () != is_var.size ())
    throw std::invalid_argument ("datalog::atom arity mismatch");
  return { relation, args, is_var };
}

inline Atom ground_atom (const std::string &relation, const Tuple &values)
{
  return { relation, values, std::vector<bool> (values.size (), false) };
}

/// Naive bottom-up evaluation to fixpoint with an iteration cap.
class Database
{
public:
  void set_iteration_cap (std::size_t cap) { iteration_cap_ = cap == 0 ? 1 : cap; }

  void add_fact (const std::string &relation, const Tuple &values)
  {
    facts_[relation].insert (values);
  }

  void add_rule (Rule rule)
  {
    if (rule.body.empty ())
      throw std::invalid_argument ("datalog::Database rules need a non-empty body");
    rules_.push_back (std::move (rule));
  }

  void evaluate ()
  {
    for (std::size_t iter = 0; iter < iteration_cap_; ++iter)
      {
        bool changed = false;
        for (const Rule &rule : rules_)
          changed |= apply_rule (rule);
        if (!changed)
          return;
      }
  }

  bool query (const std::string &relation, const Tuple &values) const
  {
    auto it = facts_.find (relation);
    return it != facts_.end () && it->second.count (values) != 0;
  }

  std::set<Tuple> relation (const std::string &relation) const
  {
    auto it = facts_.find (relation);
    return it == facts_.end () ? std::set<Tuple>{} : it->second;
  }

  std::size_t fact_count () const
  {
    std::size_t total = 0;
    for (const auto &[relation, tuples] : facts_)
      {
        (void)relation;
        total += tuples.size ();
      }
    return total;
  }

private:
  using Binding = std::map<std::string, std::string>;

  static bool unify_atom (const Atom &pattern, const Tuple &fact, Binding &binding)
  {
    if (pattern.args.size () != fact.size ())
      return false;
    Binding trial = binding;
    for (std::size_t i = 0; i < pattern.args.size (); ++i)
      {
        if (!pattern.is_var[i])
          {
            if (pattern.args[i] != fact[i])
              return false;
            continue;
          }
        auto it = trial.find (pattern.args[i]);
        if (it == trial.end ())
          trial.emplace (pattern.args[i], fact[i]);
        else if (it->second != fact[i])
          return false;
      }
    binding = std::move (trial);
    return true;
  }

  static bool ground_head (const Atom &head, const Binding &binding, Tuple &out)
  {
    out.clear ();
    for (std::size_t i = 0; i < head.args.size (); ++i)
      {
        if (!head.is_var[i])
          {
            out.push_back (head.args[i]);
            continue;
          }
        auto it = binding.find (head.args[i]);
        if (it == binding.end ())
          return false; // Ungrounded variable: no derivation.
        out.push_back (it->second);
      }
    return true;
  }

  bool apply_rule (const Rule &rule)
  {
    std::vector<Binding> partials{ {} };
    for (const Atom &atom : rule.body)
      {
        std::vector<Binding> next;
        auto it = facts_.find (atom.relation);
        if (it == facts_.end ())
          return false;
        for (const Binding &binding : partials)
          for (const Tuple &fact : it->second)
            {
              Binding extended = binding;
              if (unify_atom (atom, fact, extended))
                next.push_back (std::move (extended));
            }
        partials = std::move (next);
        if (partials.empty ())
          return false;
      }
    bool changed = false;
    for (const Binding &binding : partials)
      {
        Tuple head;
        if (ground_head (rule.head, binding, head))
          changed |= facts_[rule.head.relation].insert (head).second;
      }
    return changed;
  }

  std::map<std::string, std::set<Tuple>> facts_;
  std::vector<Rule> rules_;
  std::size_t iteration_cap_ = 10000;
};

} // namespace satie::stdlib::datalog
