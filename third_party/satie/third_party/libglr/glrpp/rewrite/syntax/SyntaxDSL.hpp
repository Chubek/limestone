#ifndef LIBGLR_GLRPP_REWRITE_SYNTAX_DSL_HPP
#define LIBGLR_GLRPP_REWRITE_SYNTAX_DSL_HPP

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "../GrammarIR.hpp"
#include "../RewritePass.hpp"

namespace glrpp::rewrite::syntax
{

struct ParsedRule
{
  std::string name;
  std::string op;
  dsl::ASTNode ast;
};

inline dsl::Result<ParsedRule, std::string>
parse_rule (std::string_view text)
{
  std::string s (text);
  auto trim = [] (std::string &x)
  {
    while (!x.empty () && std::isspace (static_cast<unsigned char> (x.front ())))
      x.erase (x.begin ());
    while (!x.empty () && std::isspace (static_cast<unsigned char> (x.back ())))
      x.pop_back ();
  };

  trim (s);
  const auto colon = s.find (':');
  if (colon == std::string::npos)
    {
      return dsl::Result<ParsedRule, std::string>::from_err (
          "expected '<name>: <op>'");
    }

  ParsedRule out;
  out.name = s.substr (0, colon);
  trim (out.name);
  out.op = s.substr (colon + 1);
  trim (out.op);
  if (out.name.empty () || out.op.empty ())
    {
      return dsl::Result<ParsedRule, std::string>::from_err (
          "empty rule name or operation");
    }

  out.ast = dsl::ASTNode (
      "rule", { dsl::ASTNode ("name", out.name), dsl::ASTNode ("op", out.op) });
  return dsl::Result<ParsedRule, std::string>::from_ok (std::move (out));
}

class ParsedRulePass final : public RewritePass
{
public:
  explicit ParsedRulePass (ParsedRule rule) : rule_ (std::move (rule)) {}

  std::string_view name () const override { return rule_.name; }

  bool apply (GrammarIR &ir) const override
  {
    if (rule_.op == "dedup_productions")
      {
        return dedup_productions (ir);
      }
    if (rule_.op == "sort_productions")
      {
        return sort_productions (ir);
      }
    return false;
  }

private:
  static bool
  dedup_productions (GrammarIR &ir)
  {
    bool changed = false;
    std::vector<ProductionIR> filtered;
    filtered.reserve (ir.productions.size ());

    for (const auto &production : ir.productions)
      {
        bool duplicate = false;
        for (const auto &existing : filtered)
          {
            if (existing.lhs == production.lhs && existing.rhs == production.rhs)
              {
                duplicate = true;
                break;
              }
          }
        if (duplicate)
          {
            changed = true;
            continue;
          }
        filtered.push_back (production);
      }

    if (changed)
      {
        ir.productions = std::move (filtered);
      }

    return changed;
  }

  static bool
  sort_productions (GrammarIR &ir)
  {
    bool already_sorted = true;
    for (std::size_t i = 1; i < ir.productions.size (); ++i)
      {
        if (ir.productions[i - 1].id > ir.productions[i].id)
          {
            already_sorted = false;
            break;
          }
      }
    if (already_sorted)
      {
        return false;
      }
    std::stable_sort (ir.productions.begin (), ir.productions.end (),
                      [] (const ProductionIR &a, const ProductionIR &b)
                      { return a.id < b.id; });
    return true;
  }

  ParsedRule rule_;
};

inline dsl::Result<std::shared_ptr<const RewritePass>, std::string>
compile_rule (const ParsedRule &rule)
{
  if (rule.op != "dedup_productions" && rule.op != "sort_productions")
    {
      return dsl::Result<std::shared_ptr<const RewritePass>, std::string>::from_err (
          "unsupported operation" + std::string (": ") + rule.op);
    }
  return dsl::Result<std::shared_ptr<const RewritePass>, std::string>::from_ok (
      std::make_shared<ParsedRulePass> (rule));
}

} // namespace glrpp::rewrite::syntax

#endif
