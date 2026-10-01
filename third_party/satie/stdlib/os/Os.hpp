#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

namespace satie::stdlib::os
{

inline std::string getenv_or (const std::string &name, const std::string &fallback = {})
{
  const char *value = std::getenv (name.c_str ());
  return value != nullptr ? value : fallback;
}

inline std::string current_dir ()
{
  std::error_code code;
  std::string dir = std::filesystem::current_path (code).string ();
  if (code)
    throw std::runtime_error ("os::current_dir failed: " + code.message ());
  return dir;
}

inline bool path_exists (const std::string &path)
{
  std::error_code code;
  return std::filesystem::exists (path, code);
}

inline std::string temp_dir ()
{
  std::error_code code;
  std::string dir = std::filesystem::temp_directory_path (code).string ();
  if (code)
    throw std::runtime_error ("os::temp_dir failed: " + code.message ());
  return dir;
}

inline unsigned cpu_count ()
{
  const unsigned count = std::thread::hardware_concurrency ();
  return count == 0 ? 1 : count;
}

} // namespace satie::stdlib::os
