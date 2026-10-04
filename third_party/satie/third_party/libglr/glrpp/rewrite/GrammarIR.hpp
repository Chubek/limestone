#ifndef LIBGLR_GLRPP_REWRITE_GRAMMAR_IR_HPP
#define LIBGLR_GLRPP_REWRITE_GRAMMAR_IR_HPP

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../DSLUtils.hpp"

#include <glr/disambiguate.h>

namespace glrpp::rewrite
{

struct SymbolIR
{
  int id = -1;
  std::string name;
  bool is_terminal = false;
  std::unordered_map<std::string, std::string> attributes;
  std::optional<std::string> provenance;
};

struct ProductionIR
{
  int id = -1;
  int lhs = -1;
  std::vector<int> rhs;
  std::optional<int> precedence;
  std::optional<glr_disambig_associativity_t> associativity;
  std::unordered_map<std::string, std::string> annotations;
  std::optional<std::string> provenance;
};

struct GrammarIR
{
  std::vector<SymbolIR> symbols;
  std::vector<ProductionIR> productions;
  std::optional<int> start_symbol_id;

  int next_symbol_id = 0;
  int next_production_id = 0;

  int add_symbol (std::string name, bool is_terminal)
  {
    const int id = next_symbol_id++;
    symbols.push_back ({ id, std::move (name), is_terminal, {}, {} });
    return id;
  }

  int add_production (int lhs, std::vector<int> rhs,
                      std::optional<int> precedence = std::nullopt,
                      std::optional<glr_disambig_associativity_t> assoc
                      = std::nullopt)
  {
    const int id = next_production_id++;
    productions.push_back ({ id, lhs, std::move (rhs), precedence, assoc, {},
                             {} });
    return id;
  }

  const SymbolIR *find_symbol (int id) const
  {
    const auto it = std::find_if (symbols.begin (), symbols.end (),
                                  [id] (const SymbolIR &s)
                                  { return s.id == id; });
    return it == symbols.end () ? nullptr : &*it;
  }

  bool validate (std::string *error = nullptr) const
  {
    std::unordered_map<int, bool> symbol_ids;
    for (const auto &symbol : symbols)
      {
        if (symbol.id < 0)
          {
            if (error)
              *error = "symbol id must be non-negative";
            return false;
          }
        if (symbol.name.empty ())
          {
            if (error)
              *error = "symbol name must not be empty";
            return false;
          }
        if (!symbol_ids.emplace (symbol.id, true).second)
          {
            if (error)
              *error = "duplicate symbol id";
            return false;
          }
      }

    if (start_symbol_id && !symbol_ids.contains (*start_symbol_id))
      {
        if (error)
          *error = "start symbol id not found";
        return false;
      }

    std::unordered_map<int, bool> production_ids;
    for (const auto &production : productions)
      {
        if (production.id < 0)
          {
            if (error)
              *error = "production id must be non-negative";
            return false;
          }
        if (!production_ids.emplace (production.id, true).second)
          {
            if (error)
              *error = "duplicate production id";
            return false;
          }
        if (!symbol_ids.contains (production.lhs))
          {
            if (error)
              *error = "production lhs id not found";
            return false;
          }
        for (int rhs_id : production.rhs)
          {
            if (!symbol_ids.contains (rhs_id))
              {
                if (error)
                  *error = "production rhs id not found";
                return false;
              }
          }
      }

    return true;
  }

  dsl::ASTNode to_ast () const
  {
    std::vector<dsl::ASTNode> symbol_nodes;
    symbol_nodes.reserve (symbols.size ());
    for (const auto &symbol : symbols)
      {
        symbol_nodes.push_back (dsl::ASTNode (
            "symbol",
            { dsl::ASTNode ("id", std::to_string (symbol.id)),
              dsl::ASTNode ("name", symbol.name),
              dsl::ASTNode ("terminal", symbol.is_terminal ? "1" : "0") }));
      }

    std::vector<dsl::ASTNode> production_nodes;
    production_nodes.reserve (productions.size ());
    for (const auto &production : productions)
      {
        std::vector<dsl::ASTNode> rhs_nodes;
        rhs_nodes.reserve (production.rhs.size ());
        for (int rhs_id : production.rhs)
          {
            rhs_nodes.emplace_back ("sym", std::to_string (rhs_id));
          }
        production_nodes.push_back (
            dsl::ASTNode ("production",
                          { dsl::ASTNode ("id", std::to_string (production.id)),
                            dsl::ASTNode ("lhs", std::to_string (production.lhs)),
                            dsl::ASTNode ("rhs", std::move (rhs_nodes)) }));
      }

    std::vector<dsl::ASTNode> root_children;
    root_children.emplace_back ("symbols", std::move (symbol_nodes));
    root_children.emplace_back ("productions", std::move (production_nodes));
    if (start_symbol_id)
      {
        root_children.emplace_back ("start", std::to_string (*start_symbol_id));
      }
    return dsl::ASTNode ("grammar", std::move (root_children));
  }
};

} // namespace glrpp::rewrite

#endif
