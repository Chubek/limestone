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
    Node root = parse_boolean_syntax (input_);
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
  using Node = BooleanSyntax;

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

  std::string input_;
};

inline BoolFormula parse_bool_formula (const std::string &text)
{
  return BoolParser (text).parse ();
}

/// Version anchor defined in src/SatieFrontendCNF.cpp.
const char *frontend_cnf_component_version () noexcept;

} // namespace satie::frontend
