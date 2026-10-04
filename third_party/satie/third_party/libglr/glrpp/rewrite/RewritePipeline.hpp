#ifndef LIBGLR_GLRPP_REWRITE_PIPELINE_HPP
#define LIBGLR_GLRPP_REWRITE_PIPELINE_HPP

#include <memory>
#include <utility>
#include <vector>

#include "RewritePass.hpp"

namespace glrpp::rewrite
{

class RewritePipeline
{
public:
  enum class Mode
  {
    once,
    fixed_point,
  };

  struct Step
  {
    std::shared_ptr<const RewritePass> pass;
    Mode mode = Mode::once;
    std::size_t max_iterations = 1;
  };

  RewritePipeline &add_once (std::shared_ptr<const RewritePass> pass)
  {
    steps_.push_back ({ std::move (pass), Mode::once, 1 });
    return *this;
  }

  RewritePipeline &add_fixed_point (std::shared_ptr<const RewritePass> pass,
                                    std::size_t max_iterations = 32)
  {
    steps_.push_back ({ std::move (pass), Mode::fixed_point,
                        max_iterations == 0 ? 1 : max_iterations });
    return *this;
  }

  RewriteStats run (GrammarIR &ir) const
  {
    RewriteStats stats;
    for (const auto &step : steps_)
      {
        if (!step.pass)
          {
            continue;
          }

        if (step.mode == Mode::once)
          {
            const bool changed = step.pass->apply (ir);
            stats.changed = stats.changed || changed;
            ++stats.iterations;
            continue;
          }

        for (std::size_t i = 0; i < step.max_iterations; ++i)
          {
            const bool changed = step.pass->apply (ir);
            ++stats.iterations;
            stats.changed = stats.changed || changed;
            if (!changed)
              {
                break;
              }
          }
      }
    return stats;
  }

private:
  std::vector<Step> steps_;
};

} // namespace glrpp::rewrite

#endif
