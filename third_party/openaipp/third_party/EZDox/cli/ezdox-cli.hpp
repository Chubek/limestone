#pragma once
#include "EzDox.hpp"
#include <unistd.h>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <functional>

namespace ezdox::cli {

struct ParseResult {
  std::map<std::string, std::vector<std::string>> values;
  std::vector<std::string> positionals;
  bool ok = true;
  std::vector<std::string> diagnostics;
};

struct Pretty {
  enum class ColorMode { auto_mode, always, never };
  ColorMode color_mode = ColorMode::auto_mode;
  bool quiet = false;
  int verbose = 0;
  mutable std::ofstream log_file;
  bool has_log_file = false;
  bool use_color() const {
#ifdef _WIN32
    return color_mode == ColorMode::always;
#else
    if (color_mode == ColorMode::always) return true;
    if (color_mode == ColorMode::never) return false;
    return ::isatty(fileno(stdout)) && ::isatty(fileno(stderr));
#endif
  }
};

using CommandFn = std::function<int(const ParseResult&, Pretty&)>;

// CLI subcommand implementations
int cmd_paths(const ParseResult &parsed, Pretty &pretty);
int cmd_config_scaffold(const ParseResult &parsed, Pretty &pretty);
int cmd_config_validate(const ParseResult &parsed, Pretty &pretty);
int cmd_config_print(const ParseResult &parsed, Pretty &pretty);
int cmd_find(const ParseResult &parsed, Pretty &pretty);
int cmd_generate(const ParseResult &parsed, Pretty &pretty);
int cmd_install(const ParseResult &parsed, Pretty &pretty);
int cmd_bundle_build(const ParseResult &parsed, Pretty &pretty);
int cmd_bundle_install(const ParseResult &parsed, Pretty &pretty);
int cmd_bundle_list(const ParseResult &parsed, Pretty &pretty);
int cmd_bundle_remove(const ParseResult &parsed, Pretty &pretty);
int cmd_bundle_inspect(const ParseResult &parsed, Pretty &pretty);
int cmd_run(const ParseResult &parsed, Pretty &pretty);
int cmd_doctor(const ParseResult &parsed, Pretty &pretty);

} // namespace ezdox::cli
