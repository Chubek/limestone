#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SatieCDCL.hpp"

namespace satie::automata
{

/// Regular-language membership (complete, via Thompson NFA + subset DFA).
///
/// Character class: bytes 0..255. Regex operators: literal, byte range,
/// concatenation, union, Kleene star (plus epsilon/empty). Each string
/// variable carries a conjunction of membership constraints, optional
/// equalities with other variables, and optional length bounds. `check`
/// intersects the DFAs per variable (state cap, else UNKNOWN), applies the
/// length bounds, and extracts a shortest witness as the model. Variable
/// concatenation is not supported yet and degrades to UNKNOWN when present.
class AutomataSolver
{
public:
  AutomataSolver () = default;
  explicit AutomataSolver (CNF cnf) : cnf_ (std::move (cnf)) {}

  [[nodiscard]] static constexpr std::string_view theory_name () noexcept
  {
    return "Automata";
  }

  /// `load` replaces the Boolean problem; automata constraints are preserved.
  /// Use `clear_theory` to drop those as well.
  void load (CNF cnf) { cnf_ = std::move (cnf); }
  const CNF &problem () const noexcept { return cnf_; }
  void clear_theory ()
  {
    var_index_.clear ();
    matches_.clear ();
    equalities_.clear ();
    length_eq_.clear ();
    length_bounds_.clear ();
    concats_.clear ();
    model_.clear ();
  }

  void set_state_cap (std::size_t cap) { state_cap_ = cap == 0 ? 1 : cap; }

  // ---- regex AST --------------------------------------------------------
  struct Regex
  {
    enum class Kind
    {
      Empty,
      Epsilon,
      Char,
      Range,
      Concat,
      Union,
      Star
    };
    explicit Regex (Kind k = Kind::Empty) : kind (k) {}
    Kind kind = Kind::Empty;
    unsigned char lo = 0;
    unsigned char hi = 0;
    std::vector<Regex> children;
  };

  static Regex empty () { return Regex (Regex::Kind::Empty); }
  static Regex epsilon () { return Regex (Regex::Kind::Epsilon); }
  static Regex ch (unsigned char c)
  {
    Regex r (Regex::Kind::Char);
    r.lo = c;
    return r;
  }
  static Regex range (unsigned char lo, unsigned char hi)
  {
    if (hi < lo)
      throw std::invalid_argument ("automata range is inverted");
    Regex r (Regex::Kind::Range);
    r.lo = lo;
    r.hi = hi;
    return r;
  }
  static Regex concat (std::vector<Regex> parts)
  {
    Regex r (Regex::Kind::Concat);
    r.children = std::move (parts);
    return r;
  }
  static Regex union_of (std::vector<Regex> parts)
  {
    Regex r (Regex::Kind::Union);
    r.children = std::move (parts);
    return r;
  }
  static Regex star (Regex inner)
  {
    Regex r (Regex::Kind::Star);
    r.children.push_back (std::move (inner));
    return r;
  }
  static Regex string (const std::string &text)
  {
    std::vector<Regex> parts;
    for (unsigned char c : text)
      parts.push_back (ch (c));
    if (parts.empty ())
      return epsilon ();
    return concat (std::move (parts));
  }

  // ---- constraints ------------------------------------------------------
  int add_var (const std::string &name)
  {
    auto it = var_index_.find (name);
    if (it != var_index_.end ())
      return it->second;
    const int id = static_cast<int> (var_index_.size ());
    var_index_.emplace (name, id);
    return id;
  }

  void add_match (int var, Regex regex)
  {
    require_var (var);
    matches_.push_back ({ var, std::move (regex) });
  }

  void add_equal (int a, int b)
  {
    require_var (a);
    require_var (b);
    equalities_.push_back ({ a, b });
  }

  void add_length_eq (int var, std::size_t length)
  {
    require_var (var);
    length_eq_.push_back ({ var, length });
  }

  void add_length_le (int var, std::size_t length)
  {
    require_var (var);
    length_bounds_.push_back ({ var, length });
  }

  /// Variable concatenation (currently forces UNKNOWN; concrete strings
  /// should be spliced with `Regex::string` instead).
  void add_concat (int result, int lhs, int rhs)
  {
    require_var (result);
    require_var (lhs);
    require_var (rhs);
    concats_.push_back ({ result, lhs, rhs });
  }

  SolveResult check ()
  {
    SolveResult boolean = solve_cdcl (cnf_);
    if (boolean.unsatisfiable ())
      return boolean;
    model_.clear ();
    if (!concats_.empty ())
      return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };

    // Merge equalities: each class shares all member constraints.
    const std::size_t n = var_index_.size ();
    UnionFind groups (n);
    for (const auto &[a, b] : equalities_)
      groups.unite (a, b);

    // Gather per-class DFAs.
    std::map<int, std::vector<DFA>> automata;
    for (const auto &[var, regex] : matches_)
      {
        DFA dfa;
        if (!to_dfa (regex, dfa))
          return { SolveStatus::UNKNOWN, Assignment (cnf_.variable_count ()) };
        automata[groups.find (var)].push_back (std::move (dfa));
      }
    std::map<int, std::size_t> fixed_length;
    std::map<int, std::size_t> max_length;
    for (const auto &[var, length] : length_eq_)
      {
        const int root = groups.find (var);
        auto it = fixed_length.find (root);
        if (it != fixed_length.end () && it->second != length)
          return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
        fixed_length[root] = length;
      }
    for (const auto &[var, length] : length_bounds_)
      {
        const int root = groups.find (var);
        auto it = max_length.find (root);
        if (it == max_length.end () || length < it->second)
          max_length[root] = length;
      }

    std::vector<std::string> witnesses (n);
    for (std::size_t v = 0; v < n; ++v)
      if (groups.find (static_cast<int> (v)) == static_cast<int> (v))
        {
          DFA product = universal_dfa ();
          auto it = automata.find (static_cast<int> (v));
          if (it != automata.end ())
            for (const DFA &dfa : it->second)
              {
                product = intersect (product, dfa);
                if (product.states.empty ())
                  return { SolveStatus::UNKNOWN,
                           Assignment (cnf_.variable_count ()) };
              }
          std::size_t hi = std::numeric_limits<std::size_t>::max ();
          if (auto f = fixed_length.find (static_cast<int> (v)); f != fixed_length.end ())
            hi = f->second;
          if (auto m = max_length.find (static_cast<int> (v)); m != max_length.end ())
            hi = std::min (hi, m->second);
          const bool fixed =
              fixed_length.find (static_cast<int> (v)) != fixed_length.end ();
          std::string witness;
          if (!witness_in (product, fixed ? hi : 0, hi, witness))
            return { SolveStatus::UNSAT, Assignment (cnf_.variable_count ()) };
          // Every member of the class shares the witness.
          for (std::size_t u = 0; u < n; ++u)
            if (groups.find (static_cast<int> (u)) == static_cast<int> (v))
              witnesses[u] = witness;
        }
    model_ = std::move (witnesses);
    return { SolveStatus::SAT, boolean.assignment };
  }

  const std::string &value (int var) const
  {
    require_var (var);
    if (model_.empty ())
      throw std::logic_error ("automata model is unavailable before a SAT check");
    return model_[static_cast<std::size_t> (var)];
  }

private:
  struct UnionFind
  {
    explicit UnionFind (std::size_t n) : parent (n, 0)
    {
      for (std::size_t i = 0; i < n; ++i)
        parent[i] = static_cast<int> (i);
    }
    int find (int x)
    {
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
    }
    void unite (int a, int b) { parent[static_cast<std::size_t> (find (a))] = find (b); }
    std::vector<int> parent;
  };

  struct NFA
  {
    struct Edge
    {
      int from = -1;
      int to = -1;
      bool epsilon = true;
      unsigned char lo = 0;
      unsigned char hi = 0;
    };
    std::vector<Edge> edges;
    int start = 0;
    int accept = 0;
    int fresh ()
    {
      const int id = next++;
      return id;
    }
    int next = 0;
  };

  struct DFA
  {
    int start = 0;
    std::vector<bool> accept;
    // delta[state][byte] = next state, -1 for dead.
    std::vector<std::vector<int>> delta;
    std::vector<int> states; // live state ids (0..k-1 after build)
  };

  void require_var (int var) const
  {
    if (var < 0 || var >= static_cast<int> (var_index_.size ()))
      throw std::invalid_argument ("unknown automata variable id");
  }

  // ---- Thompson construction --------------------------------------------
  static std::pair<int, int> build (NFA &nfa, const Regex &regex)
  {
    switch (regex.kind)
      {
      case Regex::Kind::Empty:
        {
          const int s = nfa.fresh ();
          const int t = nfa.fresh ();
          return { s, t }; // No edges: accepts nothing.
        }
      case Regex::Kind::Epsilon:
        {
          const int s = nfa.fresh ();
          const int t = nfa.fresh ();
          nfa.edges.push_back ({ s, t, true, 0, 0 });
          return { s, t };
        }
      case Regex::Kind::Char:
      case Regex::Kind::Range:
        {
          const int s = nfa.fresh ();
          const int t = nfa.fresh ();
          const unsigned char hi =
              regex.kind == Regex::Kind::Char ? regex.lo : regex.hi;
          nfa.edges.push_back ({ s, t, false, regex.lo, hi });
          return { s, t };
        }
      case Regex::Kind::Concat:
        {
          if (regex.children.empty ())
            return build (nfa, epsilon ());
          auto [s, t] = build (nfa, regex.children.front ());
          for (std::size_t i = 1; i < regex.children.size (); ++i)
            {
              auto [ns, nt] = build (nfa, regex.children[i]);
              nfa.edges.push_back ({ t, ns, true, 0, 0 });
              t = nt;
            }
          return { s, t };
        }
      case Regex::Kind::Union:
        {
          const int s = nfa.fresh ();
          const int t = nfa.fresh ();
          for (const Regex &child : regex.children)
            {
              auto [cs, ct] = build (nfa, child);
              nfa.edges.push_back ({ s, cs, true, 0, 0 });
              nfa.edges.push_back ({ ct, t, true, 0, 0 });
            }
          return { s, t };
        }
      case Regex::Kind::Star:
        {
          const int s = nfa.fresh ();
          const int t = nfa.fresh ();
          auto [cs, ct] = build (nfa, regex.children.front ());
          nfa.edges.push_back ({ s, cs, true, 0, 0 });
          nfa.edges.push_back ({ s, t, true, 0, 0 });
          nfa.edges.push_back ({ ct, cs, true, 0, 0 });
          nfa.edges.push_back ({ ct, t, true, 0, 0 });
          return { s, t };
        }
      }
    return { 0, 0 };
  }

  static std::set<int> epsilon_closure (const NFA &nfa, const std::set<int> &states)
  {
    std::set<int> closure = states;
    std::vector<int> stack (states.begin (), states.end ());
    while (!stack.empty ())
      {
        const int s = stack.back ();
        stack.pop_back ();
        for (const auto &edge : nfa.edges)
          if (edge.from == s && edge.epsilon && closure.insert (edge.to).second)
            stack.push_back (edge.to);
      }
    return closure;
  }

  bool to_dfa (const Regex &regex, DFA &dfa)
  {
    NFA nfa;
    auto [start, accept] = build (nfa, regex);
    nfa.start = start;
    nfa.accept = accept;

    std::map<std::set<int>, int> ids;
    std::vector<std::set<int>> sets;
    std::queue<int> work;
    const std::set<int> first = epsilon_closure (nfa, { start });
    ids.emplace (first, 0);
    sets.push_back (first);
    work.push (0);
    dfa.delta.push_back (std::vector<int> (256, -1));
    dfa.accept.push_back (first.count (accept) != 0);

    while (!work.empty ())
      {
        const int id = work.front ();
        work.pop ();
        for (int byte = 0; byte < 256; ++byte)
          {
            std::set<int> moved;
            for (int state : sets[static_cast<std::size_t> (id)])
              for (const auto &edge : nfa.edges)
                if (edge.from == state && !edge.epsilon && byte >= edge.lo &&
                    byte <= edge.hi)
                  moved.insert (edge.to);
            if (moved.empty ())
              continue;
            const std::set<int> closure = epsilon_closure (nfa, moved);
            auto it = ids.find (closure);
            int next = 0;
            if (it == ids.end ())
              {
                next = static_cast<int> (sets.size ());
                ids.emplace (closure, next);
                sets.push_back (closure);
                dfa.delta.push_back (std::vector<int> (256, -1));
                dfa.accept.push_back (closure.count (accept) != 0);
                if (sets.size () > state_cap_)
                  return false;
                work.push (next);
              }
            else
              next = it->second;
            dfa.delta[static_cast<std::size_t> (id)][static_cast<std::size_t> (byte)] = next;
          }
      }
    dfa.start = 0;
    dfa.states.resize (sets.size ());
    for (std::size_t i = 0; i < sets.size (); ++i)
      dfa.states[i] = static_cast<int> (i);
    return true;
  }

  static DFA universal_dfa ()
  {
    DFA dfa;
    dfa.start = 0;
    dfa.accept = { true };
    dfa.delta = { std::vector<int> (256, 0) };
    dfa.states = { 0 };
    return dfa;
  }

  bool intersect (const DFA &a, const DFA &b, DFA &out) const
  {
    out = DFA{};
    std::map<std::pair<int, int>, int> ids;
    std::queue<std::pair<int, int>> work;
    ids.emplace (std::make_pair (a.start, b.start), 0);
    work.push ({ a.start, b.start });
    out.delta.push_back (std::vector<int> (256, -1));
    out.accept.push_back (a.accept[static_cast<std::size_t> (a.start)] &&
                          b.accept[static_cast<std::size_t> (b.start)]);
    while (!work.empty ())
      {
        const auto [x, y] = work.front ();
        work.pop ();
        const int id = ids[{ x, y }];
        for (int byte = 0; byte < 256; ++byte)
          {
            const int nx = a.delta[static_cast<std::size_t> (x)][static_cast<std::size_t> (byte)];
            const int ny = b.delta[static_cast<std::size_t> (y)][static_cast<std::size_t> (byte)];
            if (nx < 0 || ny < 0)
              continue;
            const auto key = std::make_pair (nx, ny);
            auto it = ids.find (key);
            int next = 0;
            if (it == ids.end ())
              {
                next = static_cast<int> (ids.size ());
                ids.emplace (key, next);
                out.delta.push_back (std::vector<int> (256, -1));
                out.accept.push_back (a.accept[static_cast<std::size_t> (nx)] &&
                                      b.accept[static_cast<std::size_t> (ny)]);
                if (ids.size () > state_cap_)
                  return false;
                work.push (key);
              }
            else
              next = it->second;
            out.delta[static_cast<std::size_t> (id)][static_cast<std::size_t> (byte)] = next;
          }
      }
    out.start = 0;
    out.states.resize (ids.size ());
    for (std::size_t i = 0; i < ids.size (); ++i)
      out.states[i] = static_cast<int> (i);
    return true;
  }

  DFA intersect (const DFA &a, const DFA &b)
  {
    DFA out;
    if (!intersect (a, b, out))
      out.states.clear (); // Empty `states` signals the cap failure.
    return out;
  }

  /// Shortest word with `lo <= length <= hi` accepted by `dfa`.
  static bool witness_in (const DFA &dfa, std::size_t lo, std::size_t hi,
                          std::string &witness)
  {
    if (dfa.states.empty ())
      return false;
    if (hi < lo)
      return false;
    struct Item
    {
      int state = 0;
      std::string word;
    };
    std::queue<Item> queue;
    // visited[state] = shortest length reaching it (BFS layers are implicit).
    std::map<int, std::size_t> visited;
    queue.push ({ dfa.start, {} });
    visited.emplace (dfa.start, 0);
    while (!queue.empty ())
      {
        Item item = std::move (queue.front ());
        queue.pop ();
        if (item.word.size () > hi)
          continue;
        if (item.word.size () >= lo &&
            dfa.accept[static_cast<std::size_t> (item.state)])
          {
            witness = std::move (item.word);
            return true;
          }
        if (item.word.size () == hi)
          continue;
        for (int byte = 0; byte < 256; ++byte)
          {
            const int next =
                dfa.delta[static_cast<std::size_t> (item.state)][static_cast<std::size_t> (byte)];
            if (next < 0)
              continue;
            const std::size_t length = item.word.size () + 1;
            auto it = visited.find (next);
            if (it != visited.end () && it->second <= length)
              continue;
            visited[next] = length;
            queue.push ({ next, item.word + static_cast<char> (byte) });
          }
      }
    return false;
  }

  CNF cnf_{};
  std::map<std::string, int> var_index_;
  std::vector<std::pair<int, Regex>> matches_;
  std::vector<std::pair<int, int>> equalities_;
  std::vector<std::pair<int, std::size_t>> length_eq_;
  std::vector<std::pair<int, std::size_t>> length_bounds_;
  struct Concat
  {
    int result = -1;
    int lhs = -1;
    int rhs = -1;
  };
  std::vector<Concat> concats_;
  std::vector<std::string> model_;
  std::size_t state_cap_ = 2048;
};

} // namespace satie::automata
