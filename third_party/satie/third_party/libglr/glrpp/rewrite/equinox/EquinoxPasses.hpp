#ifndef LIBGLR_GLRPP_REWRITE_EQUINOX_PASSES_HPP
#define LIBGLR_GLRPP_REWRITE_EQUINOX_PASSES_HPP

/* Stratified equinox integration.
 *
 * When the equinox-ng e-graph backend is vendored at
 * GLRpp/equinox-ng/EquinoxNG.hpp the pass below deduplicates productions
 * through e-class canonicalization. When the backend is absent (the
 * directory is empty in a fresh checkout) the same pass name and the
 * same observable behavior are provided by an exact (lhs, rhs) fallback
 * so GLRpp keeps building and the rewrite pipeline keeps working.
 */

#if __has_include("../../equinox-ng/EquinoxNG.hpp")
#define GLRPP_HAS_EQUINOXNG 1
#define EQUINOXNG_NO_DSLUTILS 1
#include "../../equinox-ng/EquinoxNG.hpp"
#undef EQUINOXNG_NO_DSLUTILS
#else
#define GLRPP_HAS_EQUINOXNG 0
#endif

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../RewritePass.hpp"

namespace glrpp::rewrite::equinox
{

#if GLRPP_HAS_EQUINOXNG

class EquivalentRhsDedupPass final : public RewritePass
{
public:
  std::string_view name () const override
  {
    return "equinox.equivalent_rhs_dedup";
  }

  std::string_view
  backend () const noexcept
  {
    return "egraph";
  }

  bool apply (GrammarIR &ir) const override
  {
    using namespace equinoxng;

    auto encode_production = [] (const ProductionIR &production) -> Term {
      std::vector<Term> rhs_terms;
      rhs_terms.reserve (production.rhs.size ());
      for (int sym : production.rhs)
        {
          rhs_terms.push_back (Term::lit (Literal (static_cast<std::int64_t> (sym))));
        }

      std::vector<Term> prod_terms;
      prod_terms.reserve (2 + rhs_terms.size ());
      prod_terms.push_back (
          Term::lit (Literal (static_cast<std::int64_t> (production.lhs))));
      prod_terms.push_back (Term::op ("rhs", std::move (rhs_terms)));
      return Term::op ("prod", std::move (prod_terms));
    };

    EGraph egraph;
    std::vector<EClassId> classes;
    classes.reserve (ir.productions.size ());
    for (const auto &production : ir.productions)
      {
        classes.push_back (egraph.add (encode_production (production)));
      }

    bool changed = false;
    std::vector<ProductionIR> filtered;
    filtered.reserve (ir.productions.size ());
    std::unordered_map<std::uint32_t, bool> seen_reps;

    for (std::size_t i = 0; i < ir.productions.size (); ++i)
      {
        const auto rep = egraph.find_const (classes[i]).value;
        if (!seen_reps.emplace (rep, true).second)
          {
            changed = true;
            continue;
          }
        filtered.push_back (ir.productions[i]);
      }

    if (changed)
      {
        ir.productions = std::move (filtered);
      }
    return changed;
  }
};

#else

class EquivalentRhsDedupPass final : public RewritePass
{
public:
  std::string_view name () const override
  {
    return "equinox.equivalent_rhs_dedup";
  }

  std::string_view
  backend () const noexcept
  {
    return "native-fallback";
  }

  bool apply (GrammarIR &ir) const override
  {
    bool changed = false;
    std::unordered_map<std::string, bool> seen;
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
        if (!seen.emplace (key, true).second)
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

#endif

inline std::shared_ptr<const RewritePass>
equivalent_rhs_dedup ()
{
  return std::make_shared<EquivalentRhsDedupPass> ();
}

} // namespace glrpp::rewrite::equinox

#endif
