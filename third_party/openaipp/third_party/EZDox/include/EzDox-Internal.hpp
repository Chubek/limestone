#pragma once
/// Internal helpers shared across EZDox modules.
/// Not part of the public API.
#include "EzDox.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <stdexcept>

namespace ezdox {
namespace internal {

inline std::string anchor_id(std::string s) {
  std::string out;
  for (char c : s) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    else if (!out.empty() && out.back() != '-') out.push_back('-');
  }
  while (!out.empty() && out.back() == '-') out.pop_back();
  return out.empty() ? "id" : out;
}

inline std::string xml_escape(std::string_view s) {
  std::string out;
  for (char c : s) {
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out.push_back(c);
  }
  return out;
}

inline std::string latex_escape(std::string_view s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '\\': out += "\\textbackslash{}"; break;
      case '{': out += "\\{"; break;
      case '}': out += "\\}"; break;
      case '_': out += "\\_"; break;
      case '&': out += "\\&"; break;
      case '%': out += "\\%"; break;
      case '#': out += "\\#"; break;
      default: out.push_back(c); break;
    }
  }
  return out;
}

inline std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

inline std::string read_text(const std::filesystem::path &p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read: " + p.string());
  std::ostringstream s; s << in.rdbuf(); return s.str();
}

inline void write_text(const std::filesystem::path &p, std::string_view text) {
  if (!p.parent_path().empty()) std::filesystem::create_directories(p.parent_path());
  std::ofstream out(p, std::ios::binary);
  if (!out) throw std::runtime_error("cannot write: " + p.string());
  out << text;
}

} // namespace internal
} // namespace ezdox
