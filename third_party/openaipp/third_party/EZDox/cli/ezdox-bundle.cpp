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

int cmd_bundle_build(const ParseResult &parsed, Pretty &pretty) {
  (void)pretty;
  ezdox::build_bundle(opt(parsed, "source"), opt(parsed, "output"),
                      opt(parsed, "name"), opt(parsed, "version"),
                      opt(parsed, "description"));
  return 0;
}

int cmd_bundle_install(const ParseResult &parsed, Pretty &pretty) {
  (void)pretty;
  ezdox::install_bundle(opt(parsed, "bundle"), opt(parsed, "home"),
                        has(parsed, "force") || has(parsed, "replace"));
  return 0;
}

int cmd_bundle_list(const ParseResult &parsed, Pretty &pretty) {
  (void)pretty;
  for (const auto &b : ezdox::list_bundles(opt(parsed, "home"))) {
    std::cout << b.filename().string();
    if (has(parsed, "long")) std::cout << "\t" << b.string();
    std::cout << "\n";
  }
  return 0;
}

int cmd_bundle_remove(const ParseResult &parsed, Pretty &pretty) {
  (void)pretty;
  ezdox::remove_bundle(opt(parsed, "name"), opt(parsed, "home"));
  return 0;
}

int cmd_bundle_inspect(const ParseResult &parsed, Pretty &pretty) {
  (void)pretty;
  auto entries = ezdox::inspect_bundle(opt(parsed, "bundle"));
  if (has(parsed, "json")) {
    std::cout << "[";
    for (std::size_t i = 0; i < entries.size(); ++i)
      std::cout << (i ? "," : "") << '"' << entries[i] << '"';
    std::cout << "]\n";
  } else {
    for (const auto &e : entries) std::cout << e << "\n";
  }
  return 0;
}

} // namespace ezdox::cli
