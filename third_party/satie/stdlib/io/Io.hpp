#pragma once

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace satie::stdlib::io
{

inline std::string read_file (const std::string &path)
{
  std::ifstream in (path, std::ios::binary);
  if (!in)
    throw std::runtime_error ("io::read_file cannot open '" + path + "'");
  std::ostringstream buffer;
  buffer << in.rdbuf ();
  return buffer.str ();
}

inline std::vector<std::string> read_lines (const std::string &path)
{
  std::ifstream in (path);
  if (!in)
    throw std::runtime_error ("io::read_lines cannot open '" + path + "'");
  std::vector<std::string> lines;
  std::string line;
  while (std::getline (in, line))
    lines.push_back (line);
  return lines;
}

inline void write_file (const std::string &path, const std::string &content)
{
  std::ofstream out (path, std::ios::binary | std::ios::trunc);
  if (!out)
    throw std::runtime_error ("io::write_file cannot open '" + path + "'");
  out << content;
  if (!out)
    throw std::runtime_error ("io::write_file failed writing '" + path + "'");
}

inline void append_file (const std::string &path, const std::string &content)
{
  std::ofstream out (path, std::ios::binary | std::ios::app);
  if (!out)
    throw std::runtime_error ("io::append_file cannot open '" + path + "'");
  out << content;
}

inline bool file_exists (const std::string &path)
{
  std::ifstream in (path);
  return static_cast<bool> (in);
}

} // namespace satie::stdlib::io
