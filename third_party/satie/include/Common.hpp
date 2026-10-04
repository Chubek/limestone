#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "DSLtk.hpp"

namespace satie
{

// ============================================================
// Public API
// ============================================================

using Var = std::int32_t;
using Lit = std::int32_t;
using Clause = std::vector<Lit>;
using ClauseList = std::vector<Clause>;

constexpr Lit make_literal (Var var, bool negated = false)
{
  return negated ? -var : var;
}
constexpr Var literal_var (Lit lit) { return lit < 0 ? -lit : lit; }
constexpr bool is_negated (Lit lit) { return lit < 0; }
constexpr Lit negate (Lit lit) { return -lit; }
constexpr Lit abs_literal (Lit lit) { return lit < 0 ? -lit : lit; }

/// Variables are 1-based; zero is the DIMACS terminator and never a variable.
constexpr Var first_variable = 1;

/// Clause utilities ------------------------------------------------------

/// A clause is a tautology when it contains some literal and its complement.
/// Such a clause can never be falsified, so solvers may drop it.
inline bool is_tautology (const Clause &clause)
{
  for (std::size_t i = 0; i < clause.size (); ++i)
    for (std::size_t j = i + 1; j < clause.size (); ++j)
      if (clause[i] == negate (clause[j]))
        return true;
  return false;
}

/// True when every literal of `a` also occurs in `b`.  Assumes normalized input
/// (no zeros, sorted, duplicate free); a tautological clause subsumes nothing
/// because it is never falsified, so callers should filter those out first.
inline bool clause_subsumes (const Clause &a, const Clause &b)
{
  if (a.size () > b.size ())
    return false;
  // CNF normalization orders by absolute variable, not signed value.
  for (Lit lit : a)
    if (std::find (b.begin (), b.end (), lit) == b.end ())
      return false;
  return true;
}

inline std::size_t count_positive_literals (const Clause &clause)
{
  return static_cast<std::size_t> (
      std::count_if (clause.begin (), clause.end (), [] (Lit lit) { return lit > 0; }));
}

inline std::size_t count_negative_literals (const Clause &clause)
{
  return clause.size () - count_positive_literals (clause);
}

/// Largest variable index used by `clause`, or 0 for the empty clause.
inline Var max_var (const Clause &clause)
{
  Var result = 0;
  for (Lit lit : clause)
    result = std::max (result, literal_var (lit));
  return result;
}

/// Removes duplicate clauses and clauses subsumed by another clause.  Input is
/// expected to be normalized; output preserves first-occurrence order.
inline ClauseList deduplicated (const ClauseList &clauses)
{
  ClauseList out;
  out.reserve (clauses.size ());
  for (const Clause &clause : clauses)
    {
      const bool duplicate = std::any_of (out.begin (), out.end (), [&clause] (const Clause &seen) {
        return seen == clause || clause_subsumes (seen, clause);
      });
      if (!duplicate)
        out.push_back (clause);
    }
  return out;
}

enum class Value : std::uint8_t
{
  UNKNOWN = 0,
  TRUE = 1,
  FALSE = 2
};

inline std::string value_name (Value v)
{
  switch (v)
    {
    case Value::TRUE:
      return "TRUE";
    case Value::FALSE:
      return "FALSE";
    default:
      return "UNKNOWN";
    }
}

class Assignment
{
public:
  Assignment () = default;
  explicit Assignment (std::size_t variable_count)
      : values_ (variable_count + 1, Value::UNKNOWN)
  {
  }

  void resize (std::size_t variable_count)
  {
    // Grow-only: shrinking would silently discard assignments that the caller
    // still expects to be readable through get_var().
    if (variable_count + 1 > values_.size ())
      values_.resize (variable_count + 1, Value::UNKNOWN);
  }
  std::size_t size () const { return values_.empty () ? 0 : values_.size () - 1; }

  Value get_var (Var v) const
  {
    if (v <= 0 || static_cast<std::size_t> (v) >= values_.size ())
      return Value::UNKNOWN;
    return values_[static_cast<std::size_t> (v)];
  }

  Value get_literal (Lit lit) const
  {
    Value v = get_var (literal_var (lit));
    if (v == Value::UNKNOWN)
      return Value::UNKNOWN;
    return lit > 0 ? v : (v == Value::TRUE ? Value::FALSE : Value::TRUE);
  }

  bool assign (Var v, bool value)
  {
    if (v <= 0)
      return false;
    if (static_cast<std::size_t> (v) >= values_.size ())
      resize (static_cast<std::size_t> (v));
    Value next = value ? Value::TRUE : Value::FALSE;
    Value current = get_var (v);
    if (current != Value::UNKNOWN)
      return current == next;
    values_[static_cast<std::size_t> (v)] = next;
    return true;
  }

  bool assign_literal (Lit lit) { return assign (literal_var (lit), lit > 0); }
  void unassign (Var v)
  {
    if (v > 0 && static_cast<std::size_t> (v) < values_.size ())
      values_[static_cast<std::size_t> (v)] = Value::UNKNOWN;
  }
  bool is_assigned (Var v) const { return get_var (v) != Value::UNKNOWN; }

  /// Number of variables that currently carry a definite value.
  std::size_t assigned_count () const
  {
    std::size_t total = 0;
    for (std::size_t i = first_variable; i < values_.size (); ++i)
      if (values_[i] != Value::UNKNOWN)
        ++total;
    return total;
  }

  /// True when every variable in `1 .. size()` has a definite value.
  bool fully_assigned () const { return assigned_count () == size (); }

  void assign_all (bool value)
  {
    for (std::size_t i = first_variable; i < values_.size (); ++i)
      values_[i] = value ? Value::TRUE : Value::FALSE;
  }

  void clear ()
  {
    for (std::size_t i = first_variable; i < values_.size (); ++i)
      values_[i] = Value::UNKNOWN;
  }

  /// Calls `fn(Var, Value)` for every variable with a definite value.
  template <typename Fn> void for_each_assigned (Fn &&fn) const
  {
    for (std::size_t i = first_variable; i < values_.size (); ++i)
      if (values_[i] != Value::UNKNOWN)
        fn (static_cast<Var> (i), values_[i]);
  }

  /// A total assignment extended with `value` for still-unknown variables is
  /// useful for models of formulas with don't-care variables.
  Assignment completed (bool value = true) const
  {
    Assignment out = *this;
    for (std::size_t i = first_variable; i <= out.size (); ++i)
      if (!out.is_assigned (static_cast<Var> (i)))
        out.assign (static_cast<Var> (i), value);
    return out;
  }

  friend bool operator== (const Assignment &lhs, const Assignment &rhs)
  {
    if (lhs.size () != rhs.size ())
      return false;
    for (std::size_t i = first_variable; i <= lhs.size (); ++i)
      if (lhs.get_var (static_cast<Var> (i)) != rhs.get_var (static_cast<Var> (i)))
        return false;
    return true;
  }
  friend bool operator!= (const Assignment &lhs, const Assignment &rhs)
  {
    return !(lhs == rhs);
  }

  std::string dump () const
  {
    std::ostringstream oss;
    oss << "Assignment{";
    bool first = true;
    for (std::size_t i = 1; i < values_.size (); ++i)
      {
        if (values_[i] == Value::UNKNOWN)
          continue;
        if (!first)
          oss << ", ";
        first = false;
        oss << "x" << i << "=" << (values_[i] == Value::TRUE ? "true" : "false");
      }
    oss << "}";
    return oss.str ();
  }

private:
  std::vector<Value> values_;
};

struct ParseDiagnostic
{
  std::size_t line = 1;
  std::size_t column = 1;
  std::string message;
};

class ParseError : public std::runtime_error
{
public:
  ParseError (std::size_t line, std::size_t column, const std::string &message)
      : std::runtime_error (format (line, column, message)), diagnostic{ line, column, message }
  {
  }
  ParseDiagnostic diagnostic;

private:
  static std::string format (std::size_t line, std::size_t column, const std::string &message)
  {
    std::ostringstream oss;
    oss << "parse error at " << line << ':' << column << ": " << message;
    return oss.str ();
  }
};

class CNF
{
public:
  CNF () = default;
  explicit CNF (ClauseList clauses) : clauses_ (std::move (clauses)) { normalize (); }

  void add_clause (Clause clause)
  {
    normalize_clause (clause);
    if (clause.empty ())
      has_empty_clause_ = true;
    else if (!has_tautology_ && is_tautology (clause))
      has_tautology_ = true;
    for (Lit lit : clause)
      variable_count_ = std::max<std::size_t> (variable_count_, literal_var (lit));
    clauses_.push_back (std::move (clause));
  }

  void set_declared_variable_count (std::size_t n)
  {
    declared_variable_count_ = n;
    variable_count_ = std::max (variable_count_, n);
  }

  const ClauseList &clauses () const { return clauses_; }
  std::size_t variable_count () const { return variable_count_; }
  std::size_t declared_variable_count () const { return declared_variable_count_; }
  std::size_t clause_count () const { return clauses_.size (); }
  bool empty () const { return clauses_.empty (); }
  bool has_empty_clause () const { return has_empty_clause_; }

  /// Tautologies are kept verbatim by `add_clause` so that the container is a
  /// faithful record of its input; solvers drop them while preprocessing.
  bool has_tautology () const { return has_tautology_; }

  /// Clauses after dropping tautologies.  Semantics preserving: a tautological
  /// clause is satisfied by every total assignment.
  ClauseList clauses_without_tautologies () const
  {
    ClauseList out;
    out.reserve (clauses_.size ());
    for (const Clause &clause : clauses_)
      if (!is_tautology (clause))
        out.push_back (clause);
    return out;
  }

  /// Semantics-preserving simplification: drop tautologies and duplicate or
  /// subsumed clauses.  Never changes satisfiability.
  CNF simplified () const
  {
    CNF out (deduplicated (clauses_without_tautologies ()));
    out.set_declared_variable_count (variable_count_);
    return out;
  }

  void normalize ()
  {
    variable_count_ = declared_variable_count_;
    has_empty_clause_ = false;
    has_tautology_ = false;
    for (Clause &clause : clauses_)
      {
        normalize_clause (clause);
        if (clause.empty ())
          has_empty_clause_ = true;
        else if (!has_tautology_ && is_tautology (clause))
          has_tautology_ = true;
        for (Lit lit : clause)
          variable_count_ = std::max<std::size_t> (variable_count_, literal_var (lit));
      }
  }

  /// Literal occurrences of every variable; index 0 stays unused.
  std::vector<std::size_t> variable_occurrences () const
  {
    std::vector<std::size_t> counts (variable_count_ + 1, 0);
    for (const Clause &clause : clauses_)
      for (Lit lit : clause)
        ++counts[static_cast<std::size_t> (literal_var (lit))];
    return counts;
  }

  friend bool operator== (const CNF &lhs, const CNF &rhs)
  {
    return lhs.clauses_ == rhs.clauses_ && lhs.variable_count_ == rhs.variable_count_ &&
           lhs.declared_variable_count_ == rhs.declared_variable_count_ &&
           lhs.has_empty_clause_ == rhs.has_empty_clause_;
  }
  friend bool operator!= (const CNF &lhs, const CNF &rhs) { return !(lhs == rhs); }

  /// Disjoint union: `CNF` is a conjunction, so `&` concatenates clauses.
  friend CNF operator& (CNF lhs, const CNF &rhs)
  {
    lhs.set_declared_variable_count (std::max (lhs.variable_count (), rhs.variable_count ()));
    for (const Clause &clause : rhs.clauses_)
      lhs.add_clause (clause);
    return lhs;
  }

  std::string dump () const
  {
    std::ostringstream oss;
    oss << "CNF(";
    for (std::size_t i = 0; i < clauses_.size (); ++i)
      {
        if (i != 0)
          oss << " & ";
        oss << '(';
        for (std::size_t j = 0; j < clauses_[i].size (); ++j)
          {
            if (j != 0)
              oss << " | ";
            if (clauses_[i][j] < 0)
              oss << '~';
            oss << 'x' << literal_var (clauses_[i][j]);
          }
        oss << ')';
      }
    oss << ')';
    return oss.str ();
  }

private:
  static void normalize_clause (Clause &clause)
  {
    for (Lit lit : clause)
      if (lit == std::numeric_limits<Lit>::min ())
        throw std::invalid_argument ("literal magnitude exceeds variable range");
    clause.erase (std::remove (clause.begin (), clause.end (), 0), clause.end ());
    std::sort (clause.begin (), clause.end (), [] (Lit a, Lit b) {
      Var av = literal_var (a), bv = literal_var (b);
      return av == bv ? a < b : av < bv;
    });
    clause.erase (std::unique (clause.begin (), clause.end ()), clause.end ());
  }

  ClauseList clauses_;
  std::size_t variable_count_ = 0;
  std::size_t declared_variable_count_ = 0;
  bool has_empty_clause_ = false;
  bool has_tautology_ = false;
};

/// Semantics-preserving CNF simplification: tautologies, duplicates, subsumed
/// clauses.  Unit clauses are kept (they are not implied away).
inline CNF simplify (const CNF &cnf)
{
  return cnf.simplified ();
}

struct DIMACS
{
  std::size_t variables = 0;
  ClauseList clauses;
};

inline DIMACS cnf_to_dimacs (const CNF &cnf)
{
  return DIMACS{ cnf.variable_count (), cnf.clauses () };
}

inline CNF dimacs_to_cnf (DIMACS dimacs)
{
  CNF cnf (std::move (dimacs.clauses));
  cnf.set_declared_variable_count (dimacs.variables);
  return cnf;
}

/// Emits DIMACS CNF text.  Clause literals are written in the normalized
/// (sorted by variable, negatives first) order and terminated by `0`.
inline std::string to_dimacs_string (const CNF &cnf, bool with_header = true)
{
  std::ostringstream oss;
  if (with_header)
    oss << "p cnf " << cnf.variable_count () << ' ' << cnf.clause_count () << '\n';
  for (const Clause &clause : cnf.clauses ())
    {
      for (Lit lit : clause)
        oss << lit << ' ';
      oss << "0\n";
    }
  return oss.str ();
}

inline bool is_clause_satisfied (const Clause &clause, const Assignment &assignment)
{
  for (Lit lit : clause)
    if (assignment.get_literal (lit) == Value::TRUE)
      return true;
  return false;
}

inline bool is_clause_conflicting (const Clause &clause, const Assignment &assignment)
{
  for (Lit lit : clause)
    {
      Value v = assignment.get_literal (lit);
      if (v == Value::TRUE || v == Value::UNKNOWN)
        return false;
    }
  return true;
}

inline bool is_formula_satisfied (const CNF &cnf, const Assignment &assignment)
{
  for (const Clause &clause : cnf.clauses ())
    if (!is_clause_satisfied (clause, assignment))
      return false;
  return !cnf.has_empty_clause ();
}
inline bool has_conflict (const CNF &cnf, const Assignment &assignment)
{
  if (cnf.has_empty_clause ())
    return true;
  for (const Clause &clause : cnf.clauses ())
    if (is_clause_conflicting (clause, assignment))
      return true;
  return false;
}

enum class SolveStatus
{
  SAT,
  UNSAT,
  UNKNOWN
};

struct SolveResult
{
  SolveStatus status = SolveStatus::UNKNOWN;
  Assignment assignment{};
  [[nodiscard]] bool satisfiable () const { return status == SolveStatus::SAT; }
  [[nodiscard]] bool unsatisfiable () const { return status == SolveStatus::UNSAT; }
};

class DimacsParser
{
public:
  explicit DimacsParser (std::istream &input) : input_ (input) {}

  /// Parses a DIMACS CNF stream.  Comments (`c` lines), a `%` end-of-file
  /// marker (SATLIB convention), and CRLF line endings are accepted.  The
  /// problem line is optional, but when present the declared variable and
  /// clause counts are enforced.
  CNF parse ();

private:
  static std::size_t first_non_ws (const std::string &line)
  {
    for (std::size_t i = 0; i < line.size (); ++i)
      if (!std::isspace (static_cast<unsigned char> (line[i])))
        return i;
    return std::string::npos;
  }

  std::istream &input_;
};

inline CNF parse_dimacs (std::istream &input) { return DimacsParser (input).parse (); }
inline CNF parse_dimacs_file (const std::string &path)
{
  std::ifstream file (path);
  if (!file)
    throw std::runtime_error ("failed to open DIMACS file: " + path);
  return parse_dimacs (file);
}

inline Lit x (Var v) { return make_literal (v, false); }
inline Lit nx (Var v) { return make_literal (v, true); }
inline Clause clause () { return {}; }
template <typename... Ts> Clause clause (Ts... lits) { return Clause{ static_cast<Lit> (lits)... }; }
template <typename... Clauses> CNF cnf_of (Clauses &&...clauses)
{
  CNF cnf;
  (cnf.add_clause (std::forward<Clauses> (clauses)), ...);
  return cnf;
}

class SymbolTable
{
public:
  Var intern (const std::string &name)
  {
    if (auto it = vars_.find (name); it != vars_.end ())
      return it->second;
    Var id = static_cast<Var> (vars_.size () + 1);
    vars_[name] = id;
    names_[id] = name;
    return id;
  }
  std::optional<Var> lookup (const std::string &name) const
  {
    if (auto it = vars_.find (name); it != vars_.end ())
      return it->second;
    return std::nullopt;
  }
  std::string name (Var v) const
  {
    if (auto it = names_.find (v); it != names_.end ())
      return it->second;
    return "x" + std::to_string (v);
  }
  std::size_t size () const { return vars_.size (); }

private:
  std::unordered_map<std::string, Var> vars_;
  std::unordered_map<Var, std::string> names_;
};

class CNFParser
{
public:
  explicit CNFParser (std::string input) : input_ (std::move (input)) {}

  CNF parse ()
  {
    CNF cnf;
    skip_ws ();
    if (eof ())
      return cnf;
    while (true)
      {
        cnf.add_clause (parse_clause ());
        skip_ws ();
        if (eof ())
          break;
        expect ('&');
        skip_ws ();
        if (eof ())
          fail ("expected clause after '&'");
      }
    return dimacs_to_cnf (cnf_to_dimacs (cnf));
  }

private:
  Clause parse_clause ()
  {
    expect ('(');
    skip_ws ();
    Clause out;
    if (peek () == ')')
      fail ("empty clause must be written in DIMACS, not CNF DSL");
    while (true)
      {
        out.push_back (parse_literal ());
        skip_ws ();
        if (peek () != '|')
          break;
        consume ();
        skip_ws ();
      }
    expect (')');
    return out;
  }

  Lit parse_literal ()
  {
    bool neg = false;
    if (peek () == '~' || peek () == '!')
      {
        neg = true;
        consume ();
      }
    if (!std::isalpha (static_cast<unsigned char> (peek ())) && peek () != '_')
      fail ("expected identifier");
    std::string ident;
    while (!eof () && (std::isalnum (static_cast<unsigned char> (peek ())) || peek () == '_'))
      ident.push_back (consume ());
    Var v = symbols_.intern (ident);
    return neg ? -v : v;
  }

  void skip_ws ()
  {
    while (!eof () && std::isspace (static_cast<unsigned char> (peek ())))
      consume ();
  }
  bool eof () const { return pos_ >= input_.size (); }
  char peek () const { return eof () ? '\0' : input_[pos_]; }
  char consume ()
  {
    char c = input_[pos_++];
    if (c == '\n')
      {
        ++line_;
        column_ = 1;
      }
    else
      ++column_;
    return c;
  }
  void expect (char c)
  {
    if (peek () != c)
      fail (std::string ("expected '") + c + "'");
    consume ();
  }
  [[noreturn]] void fail (const std::string &message) const { throw ParseError (line_, column_, message); }

  std::string input_;
  std::size_t pos_ = 0;
  std::size_t line_ = 1;
  std::size_t column_ = 1;
  SymbolTable symbols_;
};

inline CNF parse_cnf (const std::string &text) { return CNFParser (text).parse (); }
inline CNF parse_cnf_file (const std::string &path)
{
  std::ifstream file (path);
  if (!file)
    throw std::runtime_error ("failed to open CNF file: " + path);
  std::ostringstream buffer;
  buffer << file.rdbuf ();
  return parse_cnf (buffer.str ());
}

struct ProblemDAG
{
  dsl::ASTNode root;
};

inline ProblemDAG cnf_to_dag (const CNF &cnf)
{
  std::vector<dsl::ASTNode> clauses;
  clauses.reserve (cnf.clause_count ());
  for (std::size_t i = 0; i < cnf.clauses ().size (); ++i)
    {
      std::vector<dsl::ASTNode> lits;
      for (Lit lit : cnf.clauses ()[i])
        lits.push_back (dsl::node<"literal"> (dsl::leaf<"var"> (literal_var (lit)),
                                               dsl::leaf<"sign"> (lit < 0 ? "neg" : "pos")));
      clauses.push_back (dsl::ASTNode{ "clause", std::move (lits) });
    }
  return ProblemDAG{ dsl::ASTNode{ "cnf", std::move (clauses) } };
}

inline std::string graphviz_escape (std::string_view s)
{
  std::string out;
  for (char c : s)
    {
      if (c == '"' || c == '\\')
        out.push_back ('\\');
      out.push_back (c);
    }
  return out;
}

inline std::string dag_to_dot (const ProblemDAG &dag)
{
  std::ostringstream dot;
  dot << "digraph SatieCNF {\n  rankdir=LR;\n  node [shape=box,fontname=monospace];\n";
  std::size_t next = 0;
  std::unordered_map<std::string, std::size_t> interned_literals;

  auto emit = [&] (auto &&self, const dsl::ASTNode &node) -> std::size_t {
    std::string key;
    if (node.tag () == "literal")
      key = node.dump ();
    if (!key.empty ())
      if (auto it = interned_literals.find (key); it != interned_literals.end ())
        return it->second;

    std::size_t id = next++;
    if (!key.empty ())
      interned_literals[key] = id;
    std::string label = std::string (node.tag ());
    if (node.is_leaf ())
      label += ":" + node.value ();
    dot << "  n" << id << " [label=\"" << graphviz_escape (label) << "\"];\n";
    for (const auto &child : node.children ())
      {
        std::size_t cid = self (self, child);
        dot << "  n" << id << " -> n" << cid << ";\n";
      }
    return id;
  };

  emit (emit, dag.root);
  dot << "}\n";
  return dot.str ();
}
inline std::string cnf_to_dot (const CNF &cnf) { return dag_to_dot (cnf_to_dag (cnf)); }

inline std::ostream &operator<< (std::ostream &os, const Clause &clause)
{
  os << '(';
  for (std::size_t i = 0; i < clause.size (); ++i)
    {
      if (i)
        os << " v ";
      if (clause[i] < 0)
        os << '~';
      os << 'x' << literal_var (clause[i]);
    }
  return os << ')';
}
inline std::ostream &operator<< (std::ostream &os, const CNF &cnf)
{
  for (std::size_t i = 0; i < cnf.clauses ().size (); ++i)
    {
      if (i)
        os << " ^ ";
      os << cnf.clauses ()[i];
    }
  return os;
}

inline std::ostream &operator<< (std::ostream &os, SolveStatus status)
{
  switch (status)
    {
    case SolveStatus::SAT:
      return os << "SAT";
    case SolveStatus::UNSAT:
      return os << "UNSAT";
    case SolveStatus::UNKNOWN:
      return os << "UNKNOWN";
    }
  return os;
}

/// DIMACS rendering of a clause: space separated literals plus the `0`
/// terminator, without a trailing space.
inline std::string to_dimacs_clause_string (const Clause &clause)
{
  std::ostringstream oss;
  for (std::size_t i = 0; i < clause.size (); ++i)
    {
      if (i)
        oss << ' ';
      oss << clause[i];
    }
  oss << " 0";
  return oss.str ();
}

inline std::string to_string (SolveStatus status)
{
  std::ostringstream oss;
  oss << status;
  return oss.str ();
}

/// Version of the linked `satie` static library (matches CMake project version).
const char *satie_library_version () noexcept;
/// Human-readable build description (version, standard, compiler).
std::string satie_library_build_info ();

} // namespace satie
