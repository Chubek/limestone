#pragma once

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace satie::stdlib::fmt
{

template <typename T> std::string to_text (const T &value)
{
  std::ostringstream oss;
  oss << value;
  return oss.str ();
}

/// Minimal `{}` formatting: each `{}` consumes the next argument (`{{` and
/// `}}` escape to literal braces). Extra arguments are ignored; missing
/// arguments throw.
template <typename... Args> std::string format (const std::string &pattern, Args &&...args)
{
  const std::vector<std::string> values{ to_text (args)... };
  std::string out;
  std::size_t arg = 0;
  for (std::size_t i = 0; i < pattern.size ();)
    {
      if (pattern[i] == '{' && i + 1 < pattern.size () && pattern[i + 1] == '{')
        {
          out.push_back ('{');
          i += 2;
        }
      else if (pattern[i] == '}' && i + 1 < pattern.size () && pattern[i + 1] == '}')
        {
          out.push_back ('}');
          i += 2;
        }
      else if (pattern[i] == '{' && i + 1 < pattern.size () && pattern[i + 1] == '}')
        {
          if (arg >= values.size ())
            throw std::invalid_argument ("fmt::format missing argument");
          out += values[arg++];
          i += 2;
        }
      else
        out.push_back (pattern[i++]);
    }
  return out;
}

template <typename T> std::string join (const std::vector<T> &items, const std::string &sep)
{
  std::string out;
  for (std::size_t i = 0; i < items.size (); ++i)
    {
      if (i != 0)
        out += sep;
      out += to_text (items[i]);
    }
  return out;
}

inline std::string trim (const std::string &text)
{
  std::size_t begin = 0;
  while (begin < text.size () && std::isspace (static_cast<unsigned char> (text[begin])) != 0)
    ++begin;
  std::size_t end = text.size ();
  while (end > begin && std::isspace (static_cast<unsigned char> (text[end - 1])) != 0)
    --end;
  return text.substr (begin, end - begin);
}

inline std::string repeat_str (const std::string &text, std::size_t count)
{
  std::string out;
  for (std::size_t i = 0; i < count; ++i)
    out += text;
  return out;
}

inline std::string pad_left (const std::string &text, std::size_t width, char fill = ' ')
{
  if (text.size () >= width)
    return text;
  return std::string (width - text.size (), fill) + text;
}

inline std::string pad_right (const std::string &text, std::size_t width, char fill = ' ')
{
  if (text.size () >= width)
    return text;
  return text + std::string (width - text.size (), fill);
}

} // namespace satie::stdlib::fmt
