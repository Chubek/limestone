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

int cmd_config_scaffold(const ParseResult &parsed, Pretty &pretty) {
  (void)pretty;
  ezdox::Config cfg;
  cfg.project = opt(parsed, "project", "MyProject");
  cfg.version = opt(parsed, "version", "0.1.0");
  auto fmts = vals(parsed, "format");
  std::string format = fmts.empty() ? "yaml" : fmts.front();
  auto sources = vals(parsed, "source");
  if (!sources.empty()) { cfg.sources.clear(); for (auto &s : sources) cfg.sources.emplace_back(s); }
  auto targets = vals(parsed, "targets");
  if (!targets.empty()) cfg.targets = targets;
  auto markups = vals(parsed, "markups");
  if (!markups.empty()) cfg.markups = markups;
  auto out_path = opt(parsed, "output", "EZDox." + format);
  ezdox::write_config(cfg, out_path, format);
  std::cout << "wrote " << out_path << "\n";
  return 0;
}

int cmd_config_validate(const ParseResult &parsed, Pretty &pretty) {
  (void)pretty;
  auto cfg = ezdox::load_config(opt(parsed, "config", ezdox::find_default_config().string()));
  auto errors = ezdox::validate_config(cfg);
  auto schema_errors = ezdox::validate_config_against_schema(cfg);
  errors.insert(errors.end(), schema_errors.begin(), schema_errors.end());
  if (has(parsed, "json")) std::cout << "{\"errors\":" << errors.size() << "}\n";
  else for (const auto &e : errors) std::cout << e << "\n";
  return errors.empty() ? 0 : 2;
}

int cmd_config_print(const ParseResult &parsed, Pretty &pretty) {
  (void)pretty;
  auto cfg = ezdox::load_config(opt(parsed, "config", ezdox::find_default_config().string()));
  auto key = opt(parsed, "key");
  std::cout << (key.empty() ? ezdox::dump_config(cfg, "yaml") : ezdox::config_key(cfg, key));
  return 0;
}

} // namespace ezdox::cli
