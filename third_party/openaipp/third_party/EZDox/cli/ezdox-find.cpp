#include "ezdox-cli.hpp"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace ezdox::cli {

static std::string opt(const ParseResult &p, const std::string &k, std::string fallback = {}) {
  auto it = p.values.find(k);
  return (it != p.values.end() && !it->second.empty()) ? it->second.front() : fallback;
}
static bool has(const ParseResult &p, const std::string &k) { return p.values.count(k) > 0; }
static std::vector<std::string> vals(const ParseResult &p, const std::string &k) {
  auto it = p.values.find(k);
  return (it != p.values.end()) ? it->second : std::vector<std::string>{};
}
static std::vector<fs::path> to_paths(const std::vector<std::string> &xs) {
  std::vector<fs::path> out;
  for (auto &x : xs) out.emplace_back(x);
  return out;
}
static std::string json_escape(const std::string &s) {
  std::string out;
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}

int cmd_find(const ParseResult &parsed, Pretty &pretty) {
  (void)pretty;
  auto items = ezdox::scan_sources_filtered(
    to_paths(vals(parsed, "source")),
    to_paths(vals(parsed, "exclude")),
    {},
    opt(parsed, "command"),
    vals(parsed, "glob"));
  if (has(parsed, "count")) {
    std::cout << items.size() << "\n";
  } else if (has(parsed, "summary")) {
    std::cout << "doc_items: " << items.size() << "\n";
  } else if (has(parsed, "json")) {
    std::cout << "[";
    for (std::size_t i = 0; i < items.size(); ++i) {
      const auto &it = items[i];
      std::cout << (i ? "," : "")
                << "{\"file\":\"" << json_escape(it.file.generic_string())
                << "\",\"line\":" << it.line
                << ",\"kind\":\"" << json_escape(it.kind)
                << "\",\"symbol\":\"" << json_escape(it.symbol)
                << "\",\"brief\":\"" << json_escape(it.brief) << "\"}";
    }
    std::cout << "]\n";
  } else {
    for (const auto &it : items) {
      std::cout << it.file << ":" << it.line << ": "
                << (it.symbol.empty() ? "" : it.symbol + ": ")
                << (it.brief.empty() ? it.text : it.brief) << "\n";
    }
  }
  return 0;
}

} // namespace ezdox::cli
