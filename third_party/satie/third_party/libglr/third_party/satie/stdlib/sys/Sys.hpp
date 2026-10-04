#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include "Common.hpp"

namespace satie::stdlib::sys
{

inline const char *version () noexcept { return satie_library_version (); }

inline std::string build_info () { return satie_library_build_info (); }

inline std::int64_t unixtime ()
{
  return static_cast<std::int64_t> (
      std::chrono::duration_cast<std::chrono::seconds> (
          std::chrono::system_clock::now ().time_since_epoch ())
          .count ());
}

inline unsigned cpu_count ()
{
  const unsigned count = std::thread::hardware_concurrency ();
  return count == 0 ? 1 : count;
}

} // namespace satie::stdlib::sys
