#pragma once

#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Common.hpp"
#include "Satie.hpp"
#include "SatieCDCL.hpp"
#include "SatieFrontendCNF.hpp"
#include "SatieFrontendCommon.hpp"

namespace satie::frontend
{

/// SatieLisp: parenthesized Boolean formulas (the SatieLisp frontend).
///
/// Grammar: `formula := atom | '(' head formula* ')'` with heads `and`,
/// `or`, `not` (exactly one argument), `xor`/`=>`/`<=>` (exactly two),
/// and `nand`/`nor` (one or more). Atoms are identifiers, `true`, `false`.
/// `;` starts a line comment. A top-level formula may also be a bare atom.
/// `parse_satielisp` returns the Tseitin CNF plus the symbol table;
/// `solve_satielisp` solves it and reports a named model.
struct LispFormula
{
  CNF cnf;
  SymbolTable symbols;
};

namespace lisp_detail
{

struct Node
{
  bool is_list = false;
  std::string atom;
  std::vector<Node> children;
};

class Tokenizer
{
public:
  explicit Tokenizer (const std::string &text) : text_ (text) {}

  std::vector<std::string> tokenize ()
  {
    std::vector<std::string> tokens;
    std::size_t i = 0;
    while (i < text_.size ())
      {
        const char c = text_[i];
        if (std::isspace (static_cast<unsigned char> (c)) != 0)
          {
            if (c == '\n')
              ++line_;
            ++i;
            continue;
          }
        if (c == ';')
          {
            while (i < text_.size () && text_[i] != '\n')
              ++i;
            continue;
          }
        if (c == '(' || c == ')')
          {
            tokens.emplace_back (1, c);
            ++i;
            continue;
          }
        std::size_t start = i;
        while (i < text_.size () && !std::isspace (static_cast<unsigned char> (text_[i])) &&
               text_[i] != '(' && text_[i] != ')' && text_[i] != ';')
          ++i;
        tokens.push_back (text_.substr (start, i - start));
      }
    return tokens;
  }

  std::size_t line () const { return line_; }

private:
  std::string text_;
  std::size_t line_ = 1;
};

inline Node parse_node (const std::vector<std::string> &tokens, std::size_t &pos,
                        std::size_t line)
{
  if (pos >= tokens.size ())
    throw ParseError (line, 1, "unexpected end of SatieLisp input");
  if (tokens[pos] == "(")
    {
      ++pos;
      Node node;
      node.is_list = true;
      while (pos < tokens.size () && tokens[pos] != ")")
        node.children.push_back (parse_node (tokens, pos, line));
      if (pos >= tokens.size ())
        throw ParseError (line, 1, "unterminated list");
      ++pos;
      return node;
    }
  if (tokens[pos] == ")")
    throw ParseError (line, 1, "unexpected ')'");
  Node node;
  node.atom = tokens[pos++];
  return node;
}

inline bool is_valid_atom (const std::string &atom)
{
  if (atom.empty () || atom == "true" || atom == "false")
    return true;
  if (!std::isalpha (static_cast<unsigned char> (atom[0])) && atom[0] != '_')
    return false;
  for (char c : atom)
    if (!std::isalnum (static_cast<unsigned char> (c)) && c != '_' && c != '\'')
      return false;
  return true;
}

inline std::string to_dsl (const Node &node, std::size_t line)
{
  if (!node.is_list)
    {
      if (!is_valid_atom (node.atom))
        throw ParseError (line, 1, "invalid SatieLisp atom '" + node.atom + "'");
      return node.atom;
    }
  if (node.children.empty () || node.children[0].is_list)
    throw ParseError (line, 1, "malformed SatieLisp form");
  const std::string head = node.children[0].atom;
  std::vector<std::string> args;
  for (std::size_t i = 1; i < node.children.size (); ++i)
    args.push_back (to_dsl (node.children[i], line));
  auto join = [&] (const std::string &sep) {
    std::string out = "(";
    for (std::size_t i = 0; i < args.size (); ++i)
      {
        if (i != 0)
          out += sep;
        out += args[i];
      }
    return out + ")";
  };
  if (head == "and")
    return args.empty () ? std::string ("true") : join (" & ");
  if (head == "or")
    return args.empty () ? std::string ("false") : join (" | ");
  if (head == "not" && args.size () == 1)
    return "(~" + args.front () + ")";
  if (head == "xor" && args.size () == 2)
    return "(" + args[0] + " ^ " + args[1] + ")";
  if ((head == "=>" || head == "implies") && args.size () == 2)
    return "(" + args[0] + " -> " + args[1] + ")";
  if ((head == "<=>" || head == "iff") && args.size () == 2)
    return "(" + args[0] + " <-> " + args[1] + ")";
  if (head == "nand" && !args.empty ())
    return "(~" + join (" & ") + ")";
  if (head == "nor" && !args.empty ())
    return "(~" + join (" | ") + ")";
  throw ParseError (line, 1, "unsupported SatieLisp head '" + head + "'");
}

} // namespace lisp_detail

inline LispFormula parse_satielisp (const std::string &text)
{
  lisp_detail::Tokenizer tokenizer (text);
  std::vector<std::string> tokens = tokenizer.tokenize ();
  std::size_t pos = 0;
  lisp_detail::Node root =
      lisp_detail::parse_node (tokens, pos, tokenizer.line ());
  if (pos != tokens.size ())
    throw ParseError (tokenizer.line (), 1, "unexpected trailing SatieLisp input");
  BoolFormula boolean = parse_bool_formula (lisp_detail::to_dsl (root, 1));
  return LispFormula{ std::move (boolean.cnf), std::move (boolean.symbols) };
}

struct LispResult
{
  SolveStatus status = SolveStatus::UNKNOWN;
  std::map<std::string, bool> model;
};

inline LispResult solve_satielisp (const std::string &text,
                                   Engine engine = Engine::CDCL)
{
  LispFormula formula = parse_satielisp (text);
  SolveResult solved = solve (formula.cnf, engine);
  LispResult result;
  result.status = solved.status;
  if (solved.satisfiable ())
    {
      // Only user-named variables are reported (Tseitin auxiliaries follow).
      const std::size_t named = formula.symbols.size ();
      for (Var v = first_variable;
           v <= static_cast<Var> (formula.cnf.variable_count ()); ++v)
        {
          if (static_cast<std::size_t> (v) > named)
            continue;
          const Value value = solved.assignment.get_var (v);
          if (value == Value::UNKNOWN)
            continue;
          result.model.emplace (formula.symbols.name (v), value == Value::TRUE);
        }
    }
  return result;
}

/// Version anchor defined in src/SatieFrontendSatieLisp.cpp.
const char *frontend_satielisp_component_version () noexcept;

} // namespace satie::frontend
