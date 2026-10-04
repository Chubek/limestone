#include "SatieFrontendCommon.hpp"
#include "SatieMemory.hpp"

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <memory_resource>

#include <glr/parser.h>

namespace satie::frontend
{
namespace
{
struct Token
{
  std::string text;
  std::size_t line;
  std::size_t column;
};
struct Input
{
  std::string encoded;
  std::vector<Token> tokens;
  std::size_t line;
  std::size_t column = 1;

  [[noreturn]] void fail (std::size_t position, const std::string &message) const
  {
    if (position < tokens.size ())
      throw ParseError (tokens[position].line, tokens[position].column, message);
    throw ParseError (line, column, message);
  }
};

Input tokenize (const std::string &text, bool boolean, std::size_t line_base)
{
  Input out{ {}, {}, line_base };
  std::size_t position = 0;
  std::size_t nesting = 0;
  auto advance = [&] {
    if (text[position++] == '\n') { ++out.line; out.column = 1; }
    else ++out.column;
  };
  while (position < text.size ())
    {
      const char c = text[position];
      if (std::isspace (static_cast<unsigned char> (c))) { advance (); continue; }
      if (c == ';')
        {
          while (position < text.size () && text[position] != '\n') advance ();
          continue;
        }
      const auto start = position;
      const auto line = out.line;
      const auto column = out.column;
      char code = 'a';
      if (c == '(' || c == ')')
        {
          code = c;
          if (c == '(' && ++nesting > 256)
            throw ParseError (line, column, "maximum parser nesting exceeded");
          if (c == ')' && nesting > 0) --nesting;
          advance ();
        }
      else if (boolean)
        {
          std::size_t width = 1;
          if (text.compare (position, 3, "<->") == 0) { code = 'e'; width = 3; }
          else if (text.compare (position, 2, "->") == 0) { code = 'i'; width = 2; }
          else if (c == '~' || c == '!') code = 'n';
          else if (c == '^') code = '^';
          else if (c == '&' || c == '|')
            {
              code = c;
              if (position + 1 < text.size () && text[position + 1] == c) width = 2;
            }
          else if (std::isalpha (static_cast<unsigned char> (c)) || c == '_')
            {
              while (position + width < text.size ())
                {
                  char next = text[position + width];
                  if (!std::isalnum (static_cast<unsigned char> (next)) &&
                      next != '_' && next != '\'') break;
                  ++width;
                }
            }
          else throw ParseError (line, column, "invalid Boolean token");
          for (std::size_t i = 0; i < width; ++i) advance ();
        }
      else
        {
          if (c == '"' || c == '|')
            throw ParseError (line, column, "quoted atoms and strings are not supported");
          while (position < text.size () &&
                 !std::isspace (static_cast<unsigned char> (text[position])) &&
                 text[position] != '(' && text[position] != ')' && text[position] != ';')
            {
              if (text[position] == '"' || text[position] == '\0')
                throw ParseError (out.line, out.column, "invalid S-expression atom");
              advance ();
            }
        }
      out.encoded.push_back (code);
      out.tokens.push_back ({ text.substr (start, position - start), line, column });
    }
  return out;
}

struct Grammar
{
  std::unique_ptr<glr_grammar_t, decltype (&glr_grammar_destroy)> grammar{
    glr_grammar_create (), glr_grammar_destroy };
  int symbol (const char *name, bool terminal = false)
  {
    if (!grammar) throw std::bad_alloc ();
    const int id = glr_grammar_add_symbol (grammar.get (),
        terminal ? GLR_SYMBOL_TERMINAL : GLR_SYMBOL_NONTERMINAL, name);
    if (id < 0) throw std::bad_alloc ();
    return id;
  }
  void rule (int head, std::initializer_list<int> body)
  {
    std::vector<glr_symbol_t *> symbols;
    for (int id : body) symbols.push_back (glr_grammar_get_symbol (grammar.get (), id));
    if (glr_grammar_add_production (grammar.get (), head, symbols.data (), symbols.size ()) < 0)
      throw std::bad_alloc ();
  }
  void finish (int start)
  {
    glr_grammar_set_start_symbol (grammar.get (), start);
    char error[256]{};
    auto *table = glr_grammar_build_parse_table (grammar.get (), error, sizeof error);
    if (!table) throw std::runtime_error (std::string ("libglr grammar: ") + error);
    if (glr_grammar_set_parse_table (grammar.get (), table, true) != 0)
      {
        glr_parse_table_destroy (table);
        throw std::runtime_error ("libglr could not attach parse table");
      }
  }
};

const Grammar &sexpr_grammar ()
{
  static const Grammar grammar = [] {
    Grammar g;
    const int document = g.symbol ("Document"), sequence = g.symbol ("Sequence"),
              expr = g.symbol ("Expr"), atom = g.symbol ("a", true),
              open = g.symbol ("(", true), close = g.symbol (")", true);
    g.rule (document, { sequence });
    g.rule (sequence, {});
    g.rule (sequence, { sequence, expr });
    g.rule (expr, { atom });
    g.rule (expr, { open, sequence, close });
    g.finish (document);
    return g;
  } ();
  return grammar;
}

const Grammar &boolean_grammar ()
{
  static const Grammar grammar = [] {
    Grammar g;
    const int iff = g.symbol ("Iff"), imp = g.symbol ("Imp"),
              disj = g.symbol ("Or"), xorr = g.symbol ("Xor"),
              conj = g.symbol ("And"), unary = g.symbol ("Unary"),
              atom = g.symbol ("a", true), e = g.symbol ("e", true),
              i = g.symbol ("i", true), o = g.symbol ("|", true),
              x = g.symbol ("^", true), a = g.symbol ("&", true),
              n = g.symbol ("n", true), open = g.symbol ("(", true),
              close = g.symbol (")", true);
    g.rule (iff, { imp }); g.rule (iff, { iff, e, imp });
    g.rule (imp, { disj }); g.rule (imp, { disj, i, imp });
    g.rule (disj, { xorr }); g.rule (disj, { disj, o, xorr });
    g.rule (xorr, { conj }); g.rule (xorr, { xorr, x, conj });
    g.rule (conj, { unary }); g.rule (conj, { conj, a, unary });
    g.rule (unary, { atom }); g.rule (unary, { n, unary });
    g.rule (unary, { open, iff, close });
    g.finish (iff);
    return g;
  } ();
  return grammar;
}

using Parser = std::unique_ptr<glr_parser_t, decltype (&glr_parser_destroy)>;
Parser parse (const Grammar &grammar, const Input &input)
{
  Parser parser (glr_parser_create (grammar.grammar.get ()), glr_parser_destroy);
  if (!parser) throw std::bad_alloc ();
  glr_parser_set_scannerless (parser.get (), true);
  const auto result = glr_parse (parser.get (), input.encoded.data (), input.encoded.size ());
  if (result.error == GLR_PARSE_ERROR_MEMORY) throw std::bad_alloc ();
  if (result.error != GLR_PARSE_SUCCESS || !result.forest || !result.forest->root)
    input.fail (result.position, "invalid syntax");
  return parser;
}

const glr_forest_node_t *constructor (const glr_forest_node_t *node)
{
  if (node->type == GLR_NODE_NONTERMINAL)
    {
      if (node->child_count != 1) throw std::runtime_error ("ambiguous frontend grammar");
      node = node->children[0];
    }
  return node;
}

SExpression expression (const glr_forest_node_t *, const Input &, std::size_t,
                         std::pmr::memory_resource *);
std::vector<SExpression> sequence (const glr_forest_node_t *node, const Input &input,
                                   std::size_t depth, std::pmr::memory_resource *memory)
{
  std::pmr::vector<const glr_forest_node_t *> reversed (memory);
  while (true)
    {
      node = constructor (node);
      if (node->child_count == 0) break;
      reversed.push_back (node->children[1]);
      node = node->children[0];
    }
  std::vector<SExpression> out;
  out.reserve (reversed.size ());
  for (auto it = reversed.rbegin (); it != reversed.rend (); ++it)
    out.push_back (expression (*it, input, depth, memory));
  return out;
}

SExpression expression (const glr_forest_node_t *node, const Input &input,
                         std::size_t depth, std::pmr::memory_resource *memory)
{
  if (depth > 256) input.fail (node->position, "maximum parser nesting exceeded");
  node = constructor (node);
  SExpression out;
  if (node->child_count == 1) out.atom = input.tokens.at (node->position).text;
  else
    {
      out.is_list = true;
      out.children = sequence (node->children[1], input, depth + 1, memory);
    }
  return out;
}

BooleanSyntax boolean_node (const glr_forest_node_t *node, const Input &input,
                            std::size_t depth)
{
  if (depth > 512) input.fail (node->position, "maximum Boolean expression depth exceeded");
  node = constructor (node);
  using Kind = BooleanSyntax::Kind;
  if (node->child_count == 1)
    {
      if (node->children[0]->type != GLR_NODE_TERMINAL)
        return boolean_node (node->children[0], input, depth);
      const auto &name = input.tokens.at (node->position).text;
      BooleanSyntax out (name == "true" ? Kind::True : name == "false" ? Kind::False : Kind::Var);
      if (out.kind == Kind::Var) out.name = name;
      return out;
    }
  if (node->child_count == 2)
    {
      BooleanSyntax out (Kind::Not);
      out.children.push_back (boolean_node (node->children[1], input, depth + 1));
      return out;
    }
  if (input.encoded.at (node->position) == '(' &&
      node->children[0]->type == GLR_NODE_TERMINAL)
    return boolean_node (node->children[1], input, depth + 1);
  const char op = input.encoded.at (node->children[1]->position);
  const auto kind = op == 'e' ? Kind::Iff : op == 'i' ? Kind::Imp :
                    op == '|' ? Kind::Or : op == '^' ? Kind::Xor : Kind::And;
  BooleanSyntax out (kind);
  auto left = boolean_node (node->children[0], input, depth + 1);
  if ((kind == Kind::And || kind == Kind::Or) && left.kind == kind)
    out.children = std::move (left.children);
  else out.children.push_back (std::move (left));
  out.children.push_back (boolean_node (node->children[2], input, depth + 1));
  return out;
}
} // namespace

std::vector<SExpression> parse_sexpressions (const std::string &text, std::size_t line_base)
{
  const Input input = tokenize (text, false, line_base);
  auto parser = parse (sexpr_grammar (), input);
  MemoryResource scratch (MemoryLifetime::Transient);
  const auto *root = constructor (glr_parser_get_forest (parser.get ())->root);
  return sequence (root->children[0], input, 0, &scratch);
}

SExpression parse_expression_tokens (const std::vector<std::string> &tokens,
                                     std::size_t &position, std::size_t line)
{
  if (position >= tokens.size ()) throw ParseError (line, 1, "unexpected end of input");
  std::string text;
  std::size_t end = position;
  int nesting = 0;
  do
    {
      const auto &token = tokens[end++];
      if (token == "(") ++nesting;
      else if (token == ")") --nesting;
      text += token + " ";
    }
  while (nesting > 0 && end < tokens.size ());
  auto parsed = parse_sexpressions (text, line);
  if (parsed.size () != 1) throw ParseError (line, 1, "expected one expression");
  position = end;
  return std::move (parsed.front ());
}

BooleanSyntax parse_boolean_syntax (const std::string &text)
{
  const Input input = tokenize (text, true, 1);
  auto parser = parse (boolean_grammar (), input);
  return boolean_node (glr_parser_get_forest (parser.get ())->root, input, 0);
}
} // namespace satie::frontend
