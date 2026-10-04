#ifndef LIBGLR_GLRPP_REWRITE_NATIVE_PASSES_HPP
#define LIBGLR_GLRPP_REWRITE_NATIVE_PASSES_HPP

#include <algorithm>
#include <memory>
#include <unordered_set>

#include "../RewritePass.hpp"

namespace glrpp::rewrite::native
{

class RemoveDuplicateProductionsPass final : public RewritePass
{
public:
  std::string_view name () const override
  {
    return "native.remove_duplicate_productions";
  }

  bool apply (GrammarIR &ir) const override
  {
    bool changed = false;
    std::unordered_set<std::string> seen;
    std::vector<ProductionIR> filtered;
    filtered.reserve (ir.productions.size ());

    for (const auto &production : ir.productions)
      {
        std::string key = std::to_string (production.lhs) + ":";
        for (int rhs : production.rhs)
          {
            key.append (std::to_string (rhs));
            key.push_back (',');
          }
        if (!seen.insert (key).second)
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
};

class SortProductionsByIdPass final : public RewritePass
{
public:
  std::string_view name () const override { return "native.sort_productions"; }

  bool apply (GrammarIR &ir) const override
  {
    if (ir.productions.empty ())
      {
        return false;
      }
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
};

inline std::shared_ptr<const RewritePass>
remove_duplicate_productions ()
{
  return std::make_shared<RemoveDuplicateProductionsPass> ();
}

inline std::shared_ptr<const RewritePass>
sort_productions ()
{
  return std::make_shared<SortProductionsByIdPass> ();
}

} // namespace glrpp::rewrite::native

#endif
