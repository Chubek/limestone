#include "SatieDPLL.hpp"

namespace satie
{

namespace
{
constexpr char kDpllVersion[] = "0.1.0";
static_assert (sizeof (DPLLSolver::Statistics) >= 6 * sizeof (std::uint64_t),
               "DPLL statistics must carry the six core counters");
}

const char *dpll_component_version () noexcept { return kDpllVersion; }

} // namespace satie
