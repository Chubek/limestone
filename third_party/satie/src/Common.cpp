#include "Common.hpp"

namespace satie
{

namespace
{
constexpr char kLibraryVersion[] = "0.1.0";
}

const char *satie_library_version () noexcept { return kLibraryVersion; }

std::string satie_library_build_info ()
{
  std::string info = std::string ("satie ") + kLibraryVersion;
#if defined(__clang__)
  info += " clang/" + std::string (__clang_version__);
#elif defined(__GNUC__)
  info += " gcc/" + std::to_string (__GNUC__) + "." + std::to_string (__GNUC_MINOR__);
#endif
  info += " cxx20";
  return info;
}

} // namespace satie
