#include "ezdox-cli.hpp"
#include <filesystem>

namespace fs = std::filesystem;

namespace ezdox::cli {

static std::string opt(const ParseResult &p, const std::string &k, std::string fallback = {}) {
  auto it = p.values.find(k);
  return (it != p.values.end() && !it->second.empty()) ? it->second.front() : fallback;
}
static bool has(const ParseResult &p, const std::string &k) { return p.values.count(k) > 0; }

int cmd_install(const ParseResult &parsed, Pretty &pretty) {
  (void)pretty;
  ezdox::copy_install(opt(parsed, "output"), opt(parsed, "dest"),
                      has(parsed, "update"), opt(parsed, "mode", "copy"));
  return 0;
}

} // namespace ezdox::cli
