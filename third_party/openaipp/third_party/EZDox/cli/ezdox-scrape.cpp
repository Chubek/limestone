#include "EzDox.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace ezdox {
namespace {

std::string trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
  return value;
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::vector<std::string> split_words(std::string_view text) {
  std::vector<std::string> out;
  std::string current;
  for (char c : text) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.' || c == '/' || c == '*' || c == '@' || c == '\\') current.push_back(c);
    else if (!current.empty()) { out.push_back(current); current.clear(); }
  }
  if (!current.empty()) out.push_back(current);
  return out;
}

std::string strip_comment_prefix(std::string line) {
  line = trim(std::move(line));
  for (auto prefix : {"///", "//!", "//!<", "///<", "/**", "/*!", "/**<", "/*!<", "/*", "*", "//!<", "///<", "//!", "///", "//!<", "///<"}) {
    std::string pre = prefix;
    if (line.rfind(pre, 0) == 0) {
      line.erase(0, pre.size());
      break;
    }
  }
  if (line.size() >= 2 && line.substr(line.size() - 2) == "*/") line.resize(line.size() - 2);
  return trim(std::move(line));
}

std::string classify_symbol(const std::string &line) {
  std::string text = trim(line);
  if (text.empty()) return {};
  std::smatch match;
  if (std::regex_search(text, match, std::regex(R"(\b(class|struct|enum|namespace|concept|union)\s+([A-Za-z_][A-Za-z0-9_:]*))"))) return match[2].str();
  if (std::regex_search(text, match, std::regex(R"(([A-Za-z_][A-Za-z0-9_:~]*)\s*\([^;{}]*\)\s*(const)?\s*[;{])"))) return match[1].str();
  if (std::regex_search(text, match, std::regex(R"(#\s*define\s+([A-Za-z_][A-Za-z0-9_]*))"))) return match[1].str();
  if (std::regex_search(text, match, std::regex(R"(([A-Za-z_][A-Za-z0-9_]*)\s*(=|;|\[))"))) return match[1].str();
  return {};
}

std::string classify_kind(const std::string &line) {
  std::string text = trim(line);
  if (text.rfind("#define", 0) == 0) return "macro";
  if (text.find("class ") != std::string::npos) return "class";
  if (text.find("struct ") != std::string::npos) return "struct";
  if (text.find("enum ") != std::string::npos) return "enum";
  if (text.find("namespace ") != std::string::npos) return "namespace";
  if (text.find("union ") != std::string::npos) return "union";
  if (text.find('(') != std::string::npos) return "function";
  return text.empty() ? "comment" : "declaration";
}

std::string strip_known_commands(std::string_view text) {
  std::ostringstream out;
  std::istringstream in{std::string(text)};
  std::string line;
  while (std::getline(in, line)) {
    auto current = trim(line);
    if (current.rfind("@brief", 0) == 0 || current.rfind("\\brief", 0) == 0) out << trim(current.substr(6)) << "\n";
    else if (current.rfind("@details", 0) == 0 || current.rfind("\\details", 0) == 0) out << trim(current.substr(8)) << "\n";
    else if (current.rfind("@param", 0) == 0 || current.rfind("\\param", 0) == 0 ||
             current.rfind("@tparam", 0) == 0 || current.rfind("\\tparam", 0) == 0 ||
             current.rfind("@return", 0) == 0 || current.rfind("\\return", 0) == 0 ||
             current.rfind("@returns", 0) == 0 || current.rfind("\\returns", 0) == 0 ||
             current.rfind("@see", 0) == 0 || current.rfind("\\see", 0) == 0 ||
             current.rfind("@sa", 0) == 0 || current.rfind("\\sa", 0) == 0 ||
             current.rfind("@ref", 0) == 0 || current.rfind("\\ref", 0) == 0 ||
             current.rfind("@note", 0) == 0 || current.rfind("\\note", 0) == 0 ||
             current.rfind("@warning", 0) == 0 || current.rfind("\\warning", 0) == 0 ||
             current.rfind("@todo", 0) == 0 || current.rfind("\\todo", 0) == 0) continue;
    else out << line << "\n";
  }
  return trim(out.str());
}

void normalize_doc_item(DocItem &item) {
  std::istringstream in{item.text};
  std::ostringstream details;
  std::string line;
  while (std::getline(in, line)) {
    auto current = trim(line);
    std::smatch match;
    if (std::regex_match(current, match, std::regex(R"(([@\\]brief)\s+(.*))"))) item.brief = trim(match[2].str());
    else if (std::regex_match(current, match, std::regex(R"(([@\\]details)\s+(.*))"))) details << trim(match[2].str()) << "\n";
    else if (std::regex_match(current, match, std::regex(R"(([@\\](?:param|tparam))(?:\s+\[[^\]]+\])?\s+([A-Za-z_][A-Za-z0-9_]*)\s*(.*))"))) item.params[match[2].str()] = trim(match[3].str());
    else if (std::regex_match(current, match, std::regex(R"(([@\\]returns?)\s+(.*))"))) item.returns = trim(match[2].str());
    else if (std::regex_match(current, match, std::regex(R"(([@\\](?:see|sa|ref))\s+(.*))"))) item.references.push_back(trim(match[2].str()));
    else if (!current.empty()) details << current << "\n";

    std::regex inline_ref(R"(([@\\]ref)\s+([A-Za-z_][A-Za-z0-9_:]*))");
    for (std::sregex_iterator it(current.begin(), current.end(), inline_ref), end; it != end; ++it) item.references.push_back((*it)[2].str());
  }
  item.details = trim(details.str());
  if (item.brief.empty()) {
    auto newline = item.details.find('\n');
    item.brief = newline == std::string::npos ? item.details : item.details.substr(0, newline);
  }
  item.text = strip_known_commands(item.text);
  std::sort(item.references.begin(), item.references.end());
  item.references.erase(std::unique(item.references.begin(), item.references.end()), item.references.end());
}

void annotate_commands(DocItem &item, const std::set<std::string> &cmdset) {
  std::istringstream in{item.text};
  std::string line;
  while (std::getline(in, line)) {
    for (const auto &word : split_words(line)) {
      if (!cmdset.contains(word)) continue;
      item.commands.push_back(word);
      auto pos = line.find(word);
      auto rest = pos == std::string::npos ? std::string{} : trim(line.substr(pos + word.size()));
      if (!rest.empty()) item.command_args[word].push_back(rest);
    }
  }
  std::sort(item.commands.begin(), item.commands.end());
  item.commands.erase(std::unique(item.commands.begin(), item.commands.end()), item.commands.end());
  normalize_doc_item(item);
}

bool is_doc_line_comment(std::string_view stripped) {
  return stripped.rfind("///", 0) == 0 || stripped.rfind("//!", 0) == 0;
}

bool is_doc_block_start(std::string_view stripped) {
  if (stripped.rfind("/**", 0) == 0 || stripped.rfind("/*!", 0) == 0) return true;
  if (stripped.rfind("/*", 0) != 0) return false;
  return stripped.size() > 2 && stripped[2] == '*';
}

bool is_trailing_doc_comment(const std::string &line, std::size_t &pos) {
  pos = line.find("///<");
  if (pos != std::string::npos) return true;
  pos = line.find("//!<");
  return pos != std::string::npos;
}

std::string next_declaration(const std::vector<std::pair<std::size_t, std::string>> &lines, std::size_t index) {
  for (std::size_t i = index; i < lines.size(); ++i) {
    auto text = trim(lines[i].second);
    if (text.empty()) continue;
    if (text.rfind("//", 0) == 0 || text.rfind("/*", 0) == 0 || text.rfind("*", 0) == 0) continue;
    return text;
  }
  return {};
}

} // namespace

std::vector<DocItem> scrape_file_comments(const fs::path &file, const std::set<std::string> &commands) {
  std::vector<DocItem> out;
  if (!fs::is_regular_file(file)) return out;

  std::ifstream in(file);
  if (!in) return out;

  std::vector<std::pair<std::size_t, std::string>> lines;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(in, line)) lines.push_back({++line_number, line});

  const auto cmdset = commands.empty() ? command_spellings(load_doxygen_commands()) : commands;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    const auto &[current_line, raw] = lines[i];
    auto stripped = trim(raw);

    if (is_doc_line_comment(stripped)) {
      DocItem item; item.file = file; item.line = current_line; item.end_line = current_line; item.kind = "comment";
      std::ostringstream text;
      const auto comment_prefix = stripped.rfind("//!<", 0) == 0 || stripped.rfind("///<", 0) == 0;
      while (i < lines.size()) {
        auto current = trim(lines[i].second);
        if (!is_doc_line_comment(current)) break;
        if (comment_prefix && !(current.rfind("//!<", 0) == 0 || current.rfind("///<", 0) == 0)) break;
        if (!comment_prefix && (current.rfind("//!<", 0) == 0 || current.rfind("///<", 0) == 0)) break;
        text << strip_comment_prefix(current) << "\n";
        item.end_line = lines[i].first;
        ++i;
      }
      if (!comment_prefix) {
        item.declaration = next_declaration(lines, i);
        item.symbol = classify_symbol(item.declaration);
        item.kind = classify_kind(item.declaration);
      }
      --i;
      item.text = trim(text.str());
      annotate_commands(item, cmdset);
      out.push_back(std::move(item));
      continue;
    }

    if (is_doc_block_start(stripped)) {
      DocItem item; item.file = file; item.line = current_line; item.end_line = current_line; item.kind = "block";
      std::ostringstream text;
      const bool trailing = stripped.rfind("/**<", 0) == 0 || stripped.rfind("/*!<", 0) == 0;
      for (; i < lines.size(); ++i) {
        text << strip_comment_prefix(lines[i].second) << "\n";
        item.end_line = lines[i].first;
        if (lines[i].second.find("*/") != std::string::npos) break;
      }
      if (!trailing) {
        item.declaration = next_declaration(lines, i + 1);
        item.symbol = classify_symbol(item.declaration);
        item.kind = classify_kind(item.declaration);
      }
      item.text = trim(text.str());
      annotate_commands(item, cmdset);
      out.push_back(std::move(item));
      continue;
    }

    std::size_t trailing_pos = std::string::npos;
    if (is_trailing_doc_comment(raw, trailing_pos)) {
      DocItem item; item.file = file; item.line = current_line; item.end_line = current_line;
      item.declaration = trim(raw.substr(0, trailing_pos));
      item.symbol = classify_symbol(item.declaration);
      item.kind = classify_kind(item.declaration);
      item.text = strip_comment_prefix(raw.substr(trailing_pos));
      annotate_commands(item, cmdset);
      out.push_back(std::move(item));
    }
  }

  return out;
}

} // namespace ezdox
