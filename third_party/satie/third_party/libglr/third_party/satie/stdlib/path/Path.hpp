#pragma once

#include <filesystem>
#include <string>

namespace satie::stdlib::path
{

inline std::string join (const std::string &left, const std::string &right)
{
  return (std::filesystem::path (left) / std::filesystem::path (right)).string ();
}

inline std::string basename (const std::string &path)
{
  return std::filesystem::path (path).filename ().string ();
}

inline std::string dirname (const std::string &path)
{
  std::string parent = std::filesystem::path (path).parent_path ().string ();
  return parent.empty () ? "." : parent;
}

inline std::string extension (const std::string &path)
{
  return std::filesystem::path (path).extension ().string ();
}

inline std::string stem (const std::string &path)
{
  return std::filesystem::path (path).stem ().string ();
}

inline std::string with_extension (const std::string &path, const std::string &ext)
{
  std::filesystem::path out (path);
  out.replace_extension (ext);
  return out.string ();
}

inline bool is_absolute (const std::string &path)
{
  return std::filesystem::path (path).is_absolute ();
}

inline std::string normalize (const std::string &path)
{
  return std::filesystem::path (path).lexically_normal ().string ();
}

} // namespace satie::stdlib::path
