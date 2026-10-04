#include "SatieFrontendCommon.hpp"

namespace satie::frontend
{

namespace
{
constexpr char kVersion[] = "0.1.0";
}

const char *frontend_common_component_version () noexcept { return kVersion; }

} // namespace satie::frontend
