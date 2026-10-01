#pragma once

#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Common.hpp"
#include "SatieFrontendCommon.hpp"

namespace satie::frontend
{

/// Full Boolean formulas compiled to CNF by Tseitin encoding (the CNF
/// frontend). Operators by precedence (loosest first): `<->`, `->`
/// (right-associative), `|` (or), `^` (xor), `&` (and), unary `~`/`!`,
/// atoms (identifiers, `true`, `false`, parentheses). Identifiers match
/// `[A-Za-z_][A-Za-z0-9_']*`. Multi-arity `&`/`|` chains share one
/// auxiliary variable. The result is equisatisfiable with the formula; the
/// returned symbol table maps names to variables for model printing.
struct BoolFormula
{
  CNF cnf;
  SymbolTable symbols;
};

class BoolParser
{
public:
  explicit BoolParser (std::string input) : input_ (std::move (input)) {}

  BoolFormula parse ()
  {
    BoolFormula formula;
    Node root = parse_iff ();
    skip_ws ();
    if (!eof ())
      fail ("unexpected trailing input");
    std::map<std::string, Var> vars;
    ClauseList clauses;
    Var next = 1;
    for (const auto &[name, id] : collect_vars (root))
      {
        (void)id;
        vars.emplace (name, next++);
      }
    const Lit top = emit (root, vars, clauses, next);
    clauses.push_back ({ top });
    formula.cnf = CNF (std::move (clauses));
    for (const auto &[name, var] : vars)
      {
        const Var interned = formula.symbols.intern (name);
        (void)interned;
        (void)var;
      }
    return formula;
  }

private:
  struct Node
  {
    enum class Kind
    {
      Var,
      True,
      False,
      Not,
      And,
      Or,
      Xor,
      Imp,
      Iff
    };
    explicit Node (Kind k = Kind::False) : kind (k) {}
    Kind kind = Kind::False;
    std::string name;
    std::vector<Node> children;
  };

  Node parse_iff ()
  {
    Node left = parse_imp ();
    while (true)
      {
        skip_ws ();
        if (!consume_token ("<->"))
          return left;
        Node node;
        node.kind = Node::Kind::Iff;
        node.children.push_back (std::move (left));
        node.children.push_back (parse_imp ());
        left = std::move (node);
      }
  }

  Node parse_imp ()
  {
    Node left = parse_or ();
    skip_ws ();
    if (!consume_token ("->"))
      return left;
    // Right-associative; `<->` already consumed above so no ambiguity.
    Node node;
    node.kind = Node::Kind::Imp;
    node.children.push_back (std::move (left));
    node.children.push_back (parse_imp ());
    return node;
  }

  Node parse_or ()
  {
    std::vector<Node> parts;
    parts.push_back (parse_xor ());
    while (true)
      {
        skip_ws ();
        if (peek () != '|')
          break;
        consume ();
        if (peek () == '|')
          consume (); // Accept `||` as an alias for `|`.
        parts.push_back (parse_xor ());
      }
    if (parts.size () == 1)
      return std::move (parts.front ());
    Node node;
    node.kind = Node::Kind::Or;
    node.children = std::move (parts);
    return node;
  }

  Node parse_xor ()
  {
    Node left = parse_and ();
    while (true)
      {
        skip_ws ();
        if (peek () != '^')
          return left;
        consume ();
        Node node;
        node.kind = Node::Kind::Xor;
        node.children.push_back (std::move (left));
        node.children.push_back (parse_and ());
        left = std::move (node);
      }
  }

  Node parse_and ()
  {
    std::vector<Node> parts;
    parts.push_back (parse_unary ());
    while (true)
      {
        skip_ws ();
        if (peek () != '&')
          break;
        consume ();
        if (peek () == '&')
          consume (); // Accept `&&` as an alias for `&`.
        parts.push_back (parse_unary ());
      }
    if (parts.size () == 1)
      return std::move (parts.front ());
    Node node;
    node.kind = Node::Kind::And;
    node.children = std::move (parts);
    return node;
  }

  Node parse_unary ()
  {
    skip_ws ();
    if (peek () == '~' || peek () == '!')
      {
        consume ();
        Node node;
        node.kind = Node::Kind::Not;
        node.children.push_back (parse_unary ());
        return node;
      }
    return parse_primary ();
  }

  Node parse_primary ()
  {
    skip_ws ();
    if (peek () == '(')
      {
        consume ();
        Node inner = parse_iff ();
        skip_ws ();
        if (peek () != ')')
          fail ("expected ')'");
        consume ();
        return inner;
      }
    if (std::isalpha (static_cast<unsigned char> (peek ())) || peek () == '_')
      {
        std::string ident;
        while (!eof () && (std::isalnum (static_cast<unsigned char> (peek ())) ||
                           peek () == '_' || peek () == '\''))
          ident.push_back (consume ());
        if (ident == "true")
          return Node (Node::Kind::True);
        if (ident == "false")
          return Node (Node::Kind::False);
        Node node (Node::Kind::Var);
        node.name = std::move (ident);
        return node;
      }
    fail ("expected variable, constant, or '('");
    return Node (Node::Kind::False);
  }

  std::map<std::string, int> collect_vars (const Node &node)
  {
    std::map<std::string, int> out;
    collect_vars_into (node, out);
    return out;
  }

  static void collect_vars_into (const Node &node, std::map<std::string, int> &out)
  {
    if (node.kind == Node::Kind::Var)
      out.emplace (node.name, 0);
    for (const Node &child : node.children)
      collect_vars_into (child, out);
  }

  /// Emits Tseitin clauses, returning the literal for `node`. `next` is the
  /// first free variable id (advanced past auxiliaries).
  static Lit emit (const Node &node, const std::map<std::string, Var> &vars,
                   ClauseList &clauses, Var &next)
  {
    switch (node.kind)
      {
      case Node::Kind::Var:
        return vars.at (node.name);
      case Node::Kind::True:
        {
          const Lit lit = next++;
          clauses.push_back ({ lit });
          return lit;
        }
      case Node::Kind::False:
        {
          const Lit lit = next++;
          clauses.push_back ({ negate (lit) });
          return lit;
        }
      case Node::Kind::Not:
        return negate (emit (node.children.front (), vars, clauses, next));
      case Node::Kind::And:
        {
          std::vector<Lit> parts;
          for (const Node &child : node.children)
            parts.push_back (emit (child, vars, clauses, next));
          const Lit aux = next++;
          for (Lit part : parts)
            clauses.push_back ({ negate (aux), part });
          Clause back{ aux };
          for (Lit part : parts)
            back.push_back (negate (part));
          clauses.push_back (std::move (back));
          return aux;
        }
      case Node::Kind::Or:
        {
          std::vector<Lit> parts;
          for (const Node &child : node.children)
            parts.push_back (emit (child, vars, clauses, next));
          const Lit aux = next++;
          for (Lit part : parts)
            clauses.push_back ({ negate (part), aux });
          Clause back{ negate (aux) };
          for (Lit part : parts)
            back.push_back (part);
          clauses.push_back (std::move (back));
          return aux;
        }
      case Node::Kind::Xor:
        {
          const Lit a = emit (node.children[0], vars, clauses, next);
          const Lit b = emit (node.children[1], vars, clauses, next);
          const Lit aux = next++;
          clauses.push_back ({ negate (a), negate (b), negate (aux) });
          clauses.push_back ({ a, b, negate (aux) });
          clauses.push_back ({ negate (a), b, aux });
          clauses.push_back ({ a, negate (b), aux });
          return aux;
        }
      case Node::Kind::Imp:
        {
          const Lit a = emit (node.children[0], vars, clauses, next);
          const Lit b = emit (node.children[1], vars, clauses, next);
          const Lit aux = next++;
          clauses.push_back ({ negate (aux), negate (a), b });
          clauses.push_back ({ aux, a });
          clauses.push_back ({ aux, negate (b) });
          return aux;
        }
      case Node::Kind::Iff:
        {
          const Lit a = emit (node.children[0], vars, clauses, next);
          const Lit b = emit (node.children[1], vars, clauses, next);
          const Lit aux = next++;
          clauses.push_back ({ negate (aux), negate (a), b });
          clauses.push_back ({ negate (aux), a, negate (b) });
          clauses.push_back ({ aux, negate (a), negate (b) });
          clauses.push_back ({ aux, a, b });
          return aux;
        }
      }
    return 0;
  }

  void skip_ws ()
  {
    while (!eof () && std::isspace (static_cast<unsigned char> (peek ())))
      consume ();
  }
  bool eof () const { return pos_ >= input_.size (); }
  char peek () const { return eof () ? '\0' : input_[pos_]; }
  char peek_at (std::size_t ahead) const
  {
    return pos_ + ahead >= input_.size () ? '\0' : input_[pos_ + ahead];
  }
  char consume ()
  {
    const char c = input_[pos_++];
    if (c == '\n')
      {
        ++line_;
        column_ = 1;
      }
    else
      ++column_;
    return c;
  }
  bool consume_token (std::string_view token)
  {
    if (input_.compare (pos_, token.size (), token) != 0)
      return false;
    // `->` must not match the prefix of `<->`... callers try `<->` first.
    for (std::size_t i = 0; i < token.size (); ++i)
      consume ();
    return true;
  }
  [[noreturn]] void fail (const std::string &message) const
  {
    throw ParseError (line_, column_, message);
  }

  std::string input_;
  std::size_t pos_ = 0;
  std::size_t line_ = 1;
  std::size_t column_ = 1;
};

inline BoolFormula parse_bool_formula (const std::string &text)
{
  return BoolParser (text).parse ();
}

/// Version anchor defined in src/SatieFrontendCNF.cpp.
const char *frontend_cnf_component_version () noexcept;

} // namespace satie::frontend
