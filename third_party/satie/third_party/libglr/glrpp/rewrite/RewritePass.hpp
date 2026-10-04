#ifndef LIBGLR_GLRPP_REWRITE_PASS_HPP
#define LIBGLR_GLRPP_REWRITE_PASS_HPP

#include <functional>
#include <string>

#include "GrammarIR.hpp"

namespace glrpp::rewrite
{

struct RewriteStats
{
  bool changed = false;
  std::size_t iterations = 0;
};

class RewritePass
{
public:
  virtual ~RewritePass () = default;
  virtual std::string_view name () const = 0;
  virtual bool apply (GrammarIR &ir) const = 0;
};

class FunctionRewritePass final : public RewritePass
{
public:
  using Fn = std::function<bool (GrammarIR &)>;

  FunctionRewritePass (std::string pass_name, Fn fn)
      : pass_name_ (std::move (pass_name)), fn_ (std::move (fn))
  {
  }

  std::string_view name () const override { return pass_name_; }
  bool apply (GrammarIR &ir) const override { return fn_ ? fn_ (ir) : false; }

private:
  std::string pass_name_;
  Fn fn_;
};

} // namespace glrpp::rewrite

#endif
