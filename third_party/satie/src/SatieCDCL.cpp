#include "SatieCDCL.hpp"

namespace satie
{

namespace
{
constexpr char kCdclVersion[] = "0.1.0";
static_assert (sizeof (CDCLSolver::Statistics) >= 6 * sizeof (std::uint64_t),
               "CDCL statistics must carry the six core counters");
}

const char *cdcl_component_version () noexcept { return kCdclVersion; }

} // namespace satie
