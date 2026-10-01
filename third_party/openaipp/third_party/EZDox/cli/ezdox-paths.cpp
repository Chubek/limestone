#include "ezdox-cli.hpp"
#include <iostream>

namespace ezdox::cli {

int cmd_paths(const ParseResult &parsed, Pretty &pretty) {
  (void)pretty;
  auto resolved = ezdox::resolve_paths();
  bool all = parsed.values.count("all") > 0;
  if (all || parsed.values.count("home"))    std::cout << "home=" << resolved.home << "\n";
  if (all || parsed.values.count("bundles")) std::cout << "bundles=" << resolved.bundles << "\n";
  if (all || parsed.values.count("markups")) std::cout << "markups=" << resolved.markups << "\n";
  if (all || parsed.values.count("targets")) std::cout << "targets=" << resolved.targets << "\n";
  if (all || parsed.values.count("cache"))   std::cout << "cache=" << resolved.cache << "\n";
  if (all || parsed.values.count("config"))  std::cout << "config=" << ezdox::find_default_config() << "\n";
  if (!all && parsed.values.empty()) {
    std::cout << "home=" << resolved.home << "\n"
              << "config=" << ezdox::find_default_config() << "\n"
              << "cache=" << resolved.cache << "\n";
  }
  return 0;
}

} // namespace ezdox::cli
