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

int cmd_generate(const ParseResult &parsed, Pretty &pretty) {
  (void)pretty;
  ezdox::Config cfg = fs::exists(opt(parsed, "config", ""))
    ? ezdox::load_config(opt(parsed, "config"))
    : ezdox::default_config();
  if (!vals(parsed, "source").empty()) cfg.sources = to_paths(vals(parsed, "source"));
  if (!vals(parsed, "include").empty()) cfg.includes = to_paths(vals(parsed, "include"));
  if (!vals(parsed, "exclude").empty()) cfg.excludes = to_paths(vals(parsed, "exclude"));
  if (!vals(parsed, "targets").empty()) cfg.targets = vals(parsed, "targets");
  if (!vals(parsed, "markups").empty()) cfg.markups = vals(parsed, "markups");
  auto out = fs::path(opt(parsed, "output", "build/docs"));
  if (has(parsed, "clean") && fs::exists(out)) fs::remove_all(out);
  auto tpl = opt(parsed, "template");
  if (!tpl.empty()) ezdox::generate(cfg, out, fs::path(tpl));
  else ezdox::generate(cfg, out);
  if (has(parsed, "profile"))
    std::cout << "items=" << ezdox::scan_sources(cfg.sources, cfg.excludes, {}).size() << "\n";
  std::cout << out.string() << "\n";
  return 0;
}

} // namespace ezdox::cli
