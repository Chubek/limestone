#include "SatieNative.hpp"

namespace satie
{

namespace
{
constexpr char kNativeVersion[] = "0.1.0";
static_assert (sizeof (NaiveSolver::Statistics) >= 5 * sizeof (std::uint64_t),
               "native statistics must carry the five core counters");
}

const char *native_component_version () noexcept { return kNativeVersion; }

} // namespace satie
