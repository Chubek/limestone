#pragma once

#include <string>

namespace satie::stdlib::constants
{

inline constexpr double pi = 3.14159265358979323846;
inline constexpr double e = 2.71828182845904523536;
inline constexpr double golden_ratio = 1.61803398874989484820;
inline constexpr double sqrt2 = 1.41421356237309504880;
inline constexpr double ln2 = 0.69314718055994530942;

/// Library version shared with the CMake project version.
inline const char *version () noexcept { return "0.1.0"; }

} // namespace satie::stdlib::constants
