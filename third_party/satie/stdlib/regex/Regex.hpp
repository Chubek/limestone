#pragma once

#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace satie::stdlib::regex
{

/// `std::regex` wrapper with exception-safe `try_` variants (bad patterns
/// report `std::nullopt`/false instead of throwing `std::regex_error`).
inline bool match_full (const std::string &text, const std::string &pattern)
{
  return std::regex_match (text, std::regex (pattern));
}

inline bool match_search (const std::string &text, const std::string &pattern)
{
  return std::regex_search (text, std::regex (pattern));
}

inline std::optional<bool> try_match_full (const std::string &text,
                                            const std::string &pattern) noexcept
{
  try
    {
      return std::regex_match (text, std::regex (pattern));
    }
  catch (const std::regex_error &)
    {
      return std::nullopt;
    }
}

inline std::optional<bool> try_match_search (const std::string &text,
                                             const std::string &pattern) noexcept
{
  try
    {
      return std::regex_search (text, std::regex (pattern));
    }
  catch (const std::regex_error &)
    {
      return std::nullopt;
    }
}

inline std::string replace_all (const std::string &text, const std::string &pattern,
                                const std::string &replacement)
{
  return std::regex_replace (text, std::regex (pattern), replacement);
}

inline std::vector<std::string> split_pattern (const std::string &text,
                                               const std::string &pattern)
{
  std::vector<std::string> parts;
  const std::regex expr (pattern);
  std::sregex_token_iterator it (text.begin (), text.end (), expr, -1);
  const std::sregex_token_iterator end;
  for (; it != end; ++it)
    parts.push_back (*it);
  return parts;
}

} // namespace satie::stdlib::regex
