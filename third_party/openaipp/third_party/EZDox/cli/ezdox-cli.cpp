#include "EzDox.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <cstdlib>
#include <cstdio>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

enum class ColorMode { auto_mode, always, never };

struct Pretty {
  ColorMode color_mode = ColorMode::auto_mode;
  bool quiet = false;
  int verbose = 0;
  // mutable: emit() receives Pretty by const-ref (it holds output *settings*),
  // but writing to the log stream is an implementation side effect, not a change
  // to the logical configuration the const-ref contract promises.
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

const char *color_reset = "\033[0m";
const char *color_red = "\033[31m";
const char *color_cyan = "\033[36m";

std::string colorize(std::string_view text, std::string_view style, bool enabled) {
  if (!enabled) return std::string(text);
  return std::string(style) + std::string(text) + color_reset;
}

void emit(std::ostream &out, const Pretty &pretty, const std::string &message, std::string_view style = {}, bool suppress_when_quiet = true) {
  if (suppress_when_quiet && pretty.quiet) return;
  if (style.empty()) out << message;
  else out << colorize(message, style, pretty.use_color());
  if (pretty.has_log_file) {
    pretty.log_file << message << "\n";
  }
}

void emit_line(std::ostream &out, const Pretty &pretty, const std::string &message, std::string_view style = {}, bool suppress_when_quiet = true) {
  if (suppress_when_quiet && pretty.quiet) return;
  emit(out, pretty, message, style, suppress_when_quiet);
  out << "\n";
}

void emit_error(const Pretty &pretty, const std::string &message) {
  emit_line(std::cerr, pretty, "error: " + message, color_red);
}

void emit_info(const Pretty &pretty, const std::string &message) {
  emit_line(std::cout, pretty, "info: " + message, color_cyan);
}

std::optional<ColorMode> parse_color_setting(const std::string &value) {
  if (value == "auto") return ColorMode::auto_mode;
  if (value == "always") return ColorMode::always;
  if (value == "never") return ColorMode::never;
  return std::nullopt;
}

}

// Minimal hand-rolled CLI argument parser, replacing the removed Klyspec dependency.
// Supports named commands with repeatable options and boolean flags, matching the
// contract the rest of this file relies on (ParseResult.values / .positionals / .ok /
// .diagnostics, consumed by the opt()/has()/vals()/kvs() helpers below).
namespace cliargs {
struct Command { std::string name; };
struct ArgumentSpec {
  std::string id;
  std::vector<std::string> names;
  bool is_flag = false;
  bool required = false;
};
struct ParseResult {
  std::map<std::string, std::vector<std::string>> values;
  std::vector<std::string> positionals;
  bool ok = true;
  std::vector<std::string> diagnostics;
};
struct Registry {
  std::map<std::string, std::vector<ArgumentSpec>> commands;
  void register_command(const Command &cmd) { commands[cmd.name]; }
  void register_argument(const std::string &cmd, const ArgumentSpec &arg) { commands[cmd].push_back(arg); }
};

ParseResult parse(const Registry &reg, const std::string &cmd, const std::vector<std::string> &args) {
  ParseResult result;
  auto it = reg.commands.find(cmd);
  if (it == reg.commands.end()) {
    result.ok = false;
    result.diagnostics.push_back("unknown command: " + cmd);
    return result;
  }
  std::map<std::string, const ArgumentSpec *> by_name;
  for (const auto &arg : it->second) for (const auto &name : arg.names) by_name[name] = &arg;

  bool positionals_only = false;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string &token = args[i];
    if (positionals_only) { result.positionals.push_back(token); continue; }
    if (token == "--") { positionals_only = true; continue; }
    if (token.size() >= 2 && token[0] == '-') {
      // Keep the leading dashes: by_name is keyed on the full option form
      // ("--dir", "-p", ...), so the lookup must use the token verbatim.
      std::string name = token, inline_value;
      bool has_inline = false;
      if (auto eq = token.find('='); eq != std::string::npos) { name = token.substr(0, eq); inline_value = token.substr(eq + 1); has_inline = true; }
      auto found = by_name.find(name);
      if (found == by_name.end()) { result.ok = false; result.diagnostics.push_back("unknown option: " + token); continue; }
      const ArgumentSpec *spec = found->second;
      if (spec->is_flag) {
        result.values[spec->id].push_back(has_inline ? inline_value : "");
      } else {
        std::string value;
        if (has_inline) value = inline_value;
        else if (i + 1 < args.size()) value = args[++i];
        else { result.ok = false; result.diagnostics.push_back("option requires a value: " + token); continue; }
        result.values[spec->id].push_back(value);
      }
    } else {
      result.positionals.push_back(token);
    }
  }
  for (const auto &arg : it->second)
    if (arg.required && !result.values.contains(arg.id)) {
      result.ok = false;
      result.diagnostics.push_back("missing required option: " + (arg.names.empty() ? arg.id : arg.names.front()));
    }
  return result;
}
} // namespace cliargs

namespace {
using PR = cliargs::ParseResult;

std::string opt(const PR &result, const std::string &key, const std::string &fallback = {}) {
  auto it = result.values.find(key);
  return it == result.values.end() || it->second.empty() ? fallback : it->second.back();
}

bool has(const PR &result, const std::string &key) {
  return result.values.contains(key);
}

std::vector<std::string> vals(const PR &result, const std::string &key) {
  auto it = result.values.find(key);
  return it == result.values.end() ? std::vector<std::string>{} : it->second;
}

int count(const PR &result, const std::string &key) {
  auto it = result.values.find(key);
  return it == result.values.end() ? 0 : static_cast<int>(it->second.size());
}

std::vector<fs::path> to_paths(const std::vector<std::string> &values) {
  std::vector<fs::path> out;
  out.reserve(values.size());
  for (const auto &value : values) out.emplace_back(value);
  return out;
}

std::map<std::string, std::string> kvs(const PR &result, const std::string &id) {
  std::map<std::string, std::string> out;
  for (const auto &entry : vals(result, id)) {
    auto split = entry.find('=');
    if (split != std::string::npos) out.emplace(entry.substr(0, split), entry.substr(split + 1));
  }
  return out;
}

std::optional<unsigned> parse_timeout(const std::string &text) {
  if (text.empty()) return std::nullopt;
  unsigned value = 0;
  for (char ch : text) {
    if (std::isdigit(static_cast<unsigned char>(ch))) value = value * 10u + static_cast<unsigned>(ch - '0');
  }
  if (value == 0) return std::nullopt;
  if (text.ends_with("ms")) return std::max(1u, value / 1000u);
  if (text.ends_with('m')) return value * 60u;
  return value;
}

std::string json_escape(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (char ch : text) {
    switch (ch) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      default: out.push_back(ch); break;
    }
  }
  return out;
}

fs::path repo_path(const fs::path &relative) {
  const fs::path cwd = fs::current_path();
  for (fs::path probe = cwd; !probe.empty(); probe = probe.parent_path()) {
    if (fs::exists(probe / "AGENTS.md") && fs::exists(probe / "manifests")) return probe / relative;
    if (probe == probe.root_path()) break;
  }
  return relative;
}

cliargs::ArgumentSpec make_flag(std::string id, std::vector<std::string> names) {
  return cliargs::ArgumentSpec{std::move(id), std::move(names), true, false};
}

cliargs::ArgumentSpec make_option(std::string id, std::vector<std::string> names, bool required = false) {
  return cliargs::ArgumentSpec{std::move(id), std::move(names), false, required};
}

cliargs::Registry build_registry() {
  cliargs::Registry registry;
  auto add_command = [&](const std::string &name, const std::vector<cliargs::ArgumentSpec> &args) {
    registry.register_command({name});
    for (const auto &arg : args) registry.register_argument(name, arg);
  };

  const auto global_config = make_option("config", {"-C", "--config"});
  const auto global_home = make_option("home", {"-H", "--home"});
  const auto global_help = make_flag("help", {"-h", "--help"});
  const auto global_version = make_flag("version", {"--version"});
  const auto global_dry_run = make_flag("dry-run", {"--dry-run"});
  const auto global_color = make_option("color", {"--color"});
  const auto global_jobs = make_option("jobs", {"-j", "--jobs"});
  const auto global_verbose = make_flag("verbose", {"-v", "--verbose"});
  const auto global_quiet = make_flag("quiet", {"-q", "--quiet"});
  const auto global_log_file = make_option("log-file", {"--log-file"});

  auto base_command = [&](bool include_config_home = false, bool include_logical_config = true) {
    std::vector<cliargs::ArgumentSpec> args;
    args.push_back(global_help);
    args.push_back(global_version);
    args.push_back(global_color);
    args.push_back(global_jobs);
    args.push_back(global_verbose);
    args.push_back(global_quiet);
    args.push_back(global_log_file);
    args.push_back(global_dry_run);
    if (include_config_home) {
      args.push_back(global_config);
      args.push_back(global_home);
    } else if (include_logical_config) {
      args.push_back(global_config);
    }
    return args;
  };
  auto add_with_common = [&](const std::string &name, const std::vector<cliargs::ArgumentSpec> &specific, bool include_config_home = false, bool include_logical_config = true) {
    auto args = base_command(include_config_home, include_logical_config);
    for (const auto &argument : specific) args.push_back(argument);
    add_command(name, args);
  };

  add_with_common("help", {}, false, false);
  add_with_common("version", {make_flag("long", {"--long"})}, false, false);
  add_with_common(
      "paths", {global_home, make_flag("all", {"--all"}),
                make_flag("home-path", {"--home"}), make_flag("bundles", {"--bundles"}), make_flag("markups", {"--markups"}),
                make_flag("targets", {"--targets"}), make_flag("cache", {"--cache"}), make_flag("config-path", {"--config"})},
      false, false);
  add_with_common("doctor", {make_flag("fix", {"--fix"})}, false, false);

  add_with_common("config-scaffold", {
    make_option("format", {"-f", "--format"}, true),
    make_option("out", {"-o", "--out"}, true),
    make_option("project", {"-p", "--project"}),
    make_option("project-version", {"-V", "--project-version"}),
    make_option("source", {"-S", "--source"}),
    make_option("include", {"-I", "--include"}),
    make_option("exclude", {"-E", "--exclude"}),
    make_option("targets", {"-t", "--targets"}),
    make_option("markups", {"-m", "--markups"}),
    make_flag("with-commands", {"--with-commands"}),
    make_flag("with-pipelines", {"--with-pipelines"}),
    make_flag("overwrite", {"--overwrite"}),
    make_flag("yes", {"-y", "--yes"})},
      false, false);
  add_with_common("config-validate", {make_flag("json", {"--json"})}, false, true);
  add_with_common("config-print", {make_option("key", {"--key"})}, false, true);
  add_with_common("config-run", {make_option("name", {"-n", "--name"}, true), make_option("env", {"-e", "--env"}), make_option("workdir", {"-w", "--workdir"}), make_option("timeout", {"-t", "--timeout"})}, false, true);

  add_with_common("bundle-build", {
    make_option("source", {"-s", "--source"}, true),
    make_option("output", {"-o", "--output"}, true),
    make_option("name", {"-n", "--name"}),
    make_option("version", {"-V", "--version"}),
    make_option("description", {"-d", "--description"})},
    false, false);
  add_with_common("bundle-install", {make_option("bundle", {"-b", "--bundle"}, true),
                                     make_option("home", {"-H", "--home"}),
                                     make_flag("force", {"--force"}),
                                     make_flag("replace", {"--replace"})},
                                     false, false);
  add_with_common("bundle-list", {make_option("home", {"-H", "--home"}), make_flag("long", {"--long"})}, false, false);
  add_with_common("bundle-remove", {make_option("name", {"-n", "--name"}, true), make_option("home", {"-H", "--home"}), make_flag("yes", {"-y", "--yes"})}, false, false);
  add_with_common("bundle-inspect", {make_option("bundle", {"-b", "--bundle"}, true), make_flag("json", {"--json"})}, false, false);

  add_with_common("find", {
    make_option("source", {"-S", "--source"}, true),
    make_option("exclude", {"-E", "--exclude"}),
    make_option("glob", {"-g", "--glob"}),
    make_option("command", {"-c", "--command"}),
    make_flag("json", {"-J", "--json"}),
    make_flag("summary", {"--summary"}),
    make_flag("count", {"--count"})},
    false, false);

  add_with_common("generate", {
    make_option("source", {"-S", "--source"}),
    make_option("include", {"-I", "--include"}),
    make_option("exclude", {"-E", "--exclude"}),
    make_option("output", {"-O", "--output"}),
    make_option("targets", {"-t", "--targets"}),
    make_option("markups", {"-m", "--markups"}),
    make_option("template", {"--template"}),
    make_flag("clean", {"--clean"}),
    make_flag("strict", {"--strict"}),
    make_flag("doxygen-compat", {"--doxygen-compat"}),
    make_flag("profile", {"--profile"}),
  }, false, true);

  add_with_common("install", {
    make_option("output", {"-O", "--output"}, true),
    make_option("dest", {"-d", "--dest"}, true),
    make_option("mode", {"--mode"}),
    make_flag("update", {"-u", "--update"})},
    false, false);

  add_with_common("run", {make_option("name", {"-n", "--name"}, true), make_option("env", {"-e", "--env"}), make_option("workdir", {"-w", "--workdir"}), make_option("timeout", {"-t", "--timeout"})}, false, true);

  add_with_common("scaffold", {
    make_option("dir", {"--dir"}, true),
    make_option("manifest", {"--manifest"}),
    make_flag("features", {"--features"}),
    make_option("project", {"-p", "--project"}),
    make_option("project-version", {"-V", "--project-version"}),
    make_flag("overwrite", {"--overwrite"}),
      make_flag("yes", {"-y", "--yes"})},
      false, false);
  return registry;
}

void apply_pretty_from_env(Pretty &pretty, const PR &parsed) {
  std::string mode = opt(parsed, "color");
  if (mode.empty()) {
    if (const char *env_mode = std::getenv("EZDOX_COLOR")) mode = env_mode;
  }
  if (const auto parsed_mode = parse_color_setting(mode); parsed_mode.has_value()) {
    pretty.color_mode = parsed_mode.value();
  } else if (!mode.empty()) {
    throw std::invalid_argument("invalid color mode, expected auto|always|never");
  }
  pretty.quiet = has(parsed, "quiet");
  pretty.verbose = count(parsed, "verbose");
  if (has(parsed, "log-file")) {
    pretty.log_file.open(opt(parsed, "log-file"), std::ios::app);
    if (!pretty.log_file) throw std::runtime_error("failed to open log file: " + opt(parsed, "log-file"));
    pretty.has_log_file = true;
  }
}

void print_help(std::string_view command = {}, const Pretty &pretty = {}) {
  if (command.empty()) {
    emit_line(std::cout, pretty, colorize("ezdox-cli", color_cyan, pretty.use_color()) + " <command> [options]");
    emit_line(std::cout, pretty, "commands: bundle config doctor find generate help install paths run scaffold version");
    emit_line(std::cout, pretty, "Tip: pass ", "", false); 
    emit_line(std::cout, pretty, "  --color always/auto/never", color_cyan, false);
    return;
  }
  emit_line(std::cout, pretty, colorize("ezdox-cli " + std::string(command), color_cyan, pretty.use_color()) + " [options]");
}

void print_error_diagnostics(const Pretty &pretty, const PR &parsed) {
  for (const auto &diagnostic : parsed.diagnostics) emit_error(pretty, diagnostic);
}

} // namespace

// Global options may appear before the subcommand (the canonical form shown in
// the docs, e.g. `ezdox-cli --color always config validate`). The per-command
// parser only ever sees tokens from the subcommand onward, so we pre-scan the
// leading globals here and merge them into the parse result afterwards.
// help/version are intentionally NOT consumed, so they can still act as the
// command token (`ezdox-cli --help`, `ezdox-cli --version`).
struct LeadingGlobals {
  std::map<std::string, std::vector<std::string>> values;
  int command_index = 0;  // argv index of the subcommand; argc when there is none
};

LeadingGlobals collect_leading_globals(int argc, char **argv) {
  const std::map<std::string, std::string> flags = {
      {"--dry-run", "dry-run"}, {"-v", "verbose"}, {"--verbose", "verbose"},
      {"-q", "quiet"}, {"--quiet", "quiet"},
  };
  const std::map<std::string, std::string> value_options = {
      {"--color", "color"}, {"-C", "config"}, {"--config", "config"},
      {"-H", "home"}, {"--home", "home"}, {"-j", "jobs"}, {"--jobs", "jobs"},
      {"--log-file", "log-file"},
  };

  LeadingGlobals out;
  out.command_index = argc;
  for (int index = 1; index < argc; ++index) {
    std::string token = argv[index];
    if (token == "--") { out.command_index = index + 1; return out; }
    if (token.empty() || token.front() != '-') { out.command_index = index; return out; }

    std::string name = token, inline_value;
    bool has_inline = false;
    if (auto eq = token.find('='); eq != std::string::npos) {
      name = token.substr(0, eq);
      inline_value = token.substr(eq + 1);
      has_inline = true;
    }

    if (auto it = flags.find(name); it != flags.end()) {
      out.values[it->second].push_back(has_inline ? inline_value : "");
      continue;
    }
    if (auto it = value_options.find(name); it != value_options.end()) {
      std::string value;
      if (has_inline) value = inline_value;
      else if (index + 1 < argc) value = argv[++index];
      else { out.command_index = argc; return out; }  // dangling value: nothing follows
      out.values[it->second].push_back(value);
      continue;
    }

    out.command_index = index;  // first non-global token is the subcommand
    return out;
  }
  return out;  // only globals seen, no subcommand
}

int main(int argc, char **argv) {
  try {
    auto leading = collect_leading_globals(argc, argv);
    if (leading.command_index >= argc) {
      // No subcommand: only leading globals (or no arguments at all).
      print_help();
      return 0;
    }

    std::string cmd = argv[leading.command_index];
    int start = leading.command_index + 1;
    if ((cmd == "config" || cmd == "bundle") && start < argc) {
      std::string sub = argv[start];
      if (sub.empty() || sub.front() != '-') {  // don't swallow an option as the subcommand
        cmd += "-" + sub;
        ++start;
      }
    }
    if (cmd == "--version") cmd = "version";
    if (cmd == "-h" || cmd == "--help") cmd = "help";

    std::vector<std::string> args;
    for (int index = start; index < argc; ++index) args.emplace_back(argv[index]);

    auto registry = build_registry();
    auto parsed = cliargs::parse(registry, cmd, args);
    for (const auto &[id, occurrences] : leading.values) {
      auto &slot = parsed.values[id];
      slot.insert(slot.end(), occurrences.begin(), occurrences.end());
    }
    Pretty pretty;
    apply_pretty_from_env(pretty, parsed);
    if (!parsed.ok) {
      print_error_diagnostics(pretty, parsed);
      return 3;
    }
    if (has(parsed, "help")) {
      print_help(cmd == "help" && !parsed.positionals.empty() ? parsed.positionals.front() : cmd, pretty);
      return 0;
    }
    if (has(parsed, "version") || cmd == "version") {
      std::cout << ezdox::version() << "\n";
      if (cmd == "version" && has(parsed, "long")) std::cout << "home: " << ezdox::resolve_paths().home << "\n";
      return 0;
    }
    if (cmd == "help") {
      print_help(parsed.positionals.empty() ? std::string{} : parsed.positionals.front(), pretty);
      return 0;
    }

    if (cmd == "paths") {
      auto resolved = ezdox::resolve_paths(opt(parsed, "home"));
      std::map<std::string, fs::path> path_map{{"home", resolved.home}, {"bundles", resolved.bundles}, {"markups", resolved.markups}, {"targets", resolved.targets}, {"cache", resolved.cache}, {"config", resolved.config}};
      bool printed = false;
      if (has(parsed, "home-path")) { std::cout << "home: " << resolved.home << "\n"; printed = true; }
      for (const auto &[key, value] : path_map) {
        if (key != "home" && key != "config" && has(parsed, key)) { std::cout << key << ": " << value << "\n"; printed = true; }
      }
      if (has(parsed, "config-path")) { std::cout << "config: " << resolved.config << "\n"; printed = true; }
      if (!printed || has(parsed, "all")) {
        for (const auto &[key, value] : path_map) std::cout << key << ": " << value << "\n";
      }
      return 0;
    }

    if (cmd == "config-scaffold") {
      ezdox::Config config = ezdox::default_config();
      config.project = opt(parsed, "project", config.project);
      config.version = opt(parsed, "project-version", config.version);
      if (!vals(parsed, "source").empty()) config.sources = to_paths(vals(parsed, "source"));
      config.includes = to_paths(vals(parsed, "include"));
      config.excludes = to_paths(vals(parsed, "exclude"));
      if (!vals(parsed, "targets").empty()) config.targets = vals(parsed, "targets");
      if (!vals(parsed, "markups").empty()) config.markups = vals(parsed, "markups");
      if (has(parsed, "with-commands")) config.commands["build"] = "ezdox-cli generate -C EZDox.yaml -O build/docs";
      if (has(parsed, "with-pipelines")) config.pipelines["publish"] = {"build", "echo publish"};
      auto out = fs::path(opt(parsed, "out"));
      if (out.empty()) throw std::runtime_error("--out required");
      if (fs::exists(out) && !(has(parsed, "overwrite") || has(parsed, "yes"))) throw std::runtime_error("output exists; use --overwrite");
      ezdox::write_config(config, out, opt(parsed, "format", "yaml"));
      emit_line(std::cout, pretty, out.string());
      return 0;
    }

    if (cmd == "scaffold") {
      auto dir = fs::path(opt(parsed, "dir"));
      if (dir.empty()) throw std::runtime_error("--dir required");
      std::string manifest = opt(parsed, "manifest", "yaml");
      if (manifest != "yaml" && manifest != "json" && manifest != "yml")
        throw std::runtime_error("--manifest must be yaml or json");
      std::string fmt = (manifest == "json") ? "json" : "yaml";
      ezdox::Config config = ezdox::default_config();
      config.project = opt(parsed, "project", config.project);
      config.version = opt(parsed, "project-version", config.version);
      if (has(parsed, "features")) {
        config.commands["build"] = "ezdox-cli generate -C " + (dir / ("EZDox." + fmt)).string() + " -O build/docs";
        config.pipelines["publish"] = {"build", "echo publish"};
      }
      fs::create_directories(dir);
      auto out = dir / ("EZDox." + fmt);
      if (fs::exists(out) && !(has(parsed, "overwrite") || has(parsed, "yes")))
        throw std::runtime_error("output exists; use --overwrite");
      ezdox::write_config(config, out, fmt);
      emit_line(std::cout, pretty, out.string());
      return 0;
    }

    if (cmd == "config-validate") {
      auto config = ezdox::load_config(opt(parsed, "config", ezdox::find_default_config().string()));
      auto errors = ezdox::validate_config(config);
      auto schema_errors = ezdox::validate_config_against_schema(config);
      errors.insert(errors.end(), schema_errors.begin(), schema_errors.end());
      if (has(parsed, "json")) std::cout << "{\"errors\":" << errors.size() << "}\n";
      else for (const auto &error : errors) std::cout << error << "\n";
      return errors.empty() ? 0 : 2;
    }

    if (cmd == "config-print") {
      auto config = ezdox::load_config(opt(parsed, "config", ezdox::find_default_config().string()));
      auto key = opt(parsed, "key");
      std::cout << (key.empty() ? ezdox::dump_config(config, "yaml") : ezdox::config_key(config, key));
      return 0;
    }

    if (cmd == "config-run" || cmd == "run") {
      auto config = ezdox::load_config(opt(parsed, "config", ezdox::find_default_config().string()));
      ezdox::RunOptions options;
      options.dry_run = has(parsed, "dry-run");
      options.workdir = opt(parsed, "workdir");
      options.environment = kvs(parsed, "env");
      options.timeout_seconds = parse_timeout(opt(parsed, "timeout"));
      options.passthrough_args = parsed.positionals;
      return ezdox::run_named(config, opt(parsed, "name"), options);
    }

    if (cmd == "bundle-build") {
      ezdox::build_bundle(opt(parsed, "source"), opt(parsed, "output"), opt(parsed, "name"), opt(parsed, "version"), opt(parsed, "description"));
      return 0;
    }
    if (cmd == "bundle-install") {
      ezdox::install_bundle(opt(parsed, "bundle"), opt(parsed, "home"), has(parsed, "force") || has(parsed, "replace"));
      return 0;
    }
    if (cmd == "bundle-list") {
      for (const auto &bundle : ezdox::list_bundles(opt(parsed, "home"))) {
        std::cout << bundle.filename().string();
        if (has(parsed, "long")) std::cout << "\t" << bundle.string();
        std::cout << "\n";
      }
      return 0;
    }
    if (cmd == "bundle-remove") {
      ezdox::remove_bundle(opt(parsed, "name"), opt(parsed, "home"));
      return 0;
    }
    if (cmd == "bundle-inspect") {
      auto entries = ezdox::inspect_bundle(opt(parsed, "bundle"));
      if (has(parsed, "json")) {
        std::cout << "[";
        for (std::size_t index = 0; index < entries.size(); ++index) std::cout << (index ? "," : "") << '"' << entries[index] << '"';
        std::cout << "]\n";
      } else {
        for (const auto &entry : entries) std::cout << entry << "\n";
      }
      return 0;
    }

    if (cmd == "find") {
      auto items = ezdox::scan_sources_filtered(to_paths(vals(parsed, "source")), to_paths(vals(parsed, "exclude")), {}, opt(parsed, "command"), vals(parsed, "glob"));
      if (has(parsed, "count")) {
        std::cout << items.size() << "\n";
      } else if (has(parsed, "summary")) {
        std::cout << "doc_items: " << items.size() << "\n";
      } else if (has(parsed, "json")) {
        std::cout << "[";
        for (std::size_t index = 0; index < items.size(); ++index) {
          const auto &item = items[index];
          std::cout << (index ? "," : "")
                    << "{\"file\":\"" << json_escape(item.file.generic_string())
                    << "\",\"line\":" << item.line
                    << ",\"end_line\":" << item.end_line
                    << ",\"kind\":\"" << json_escape(item.kind)
                    << "\",\"symbol\":\"" << json_escape(item.symbol)
                    << "\",\"brief\":\"" << json_escape(item.brief)
                    << "\",\"params\":{";
          std::size_t param_index = 0;
          for (const auto &[key, value] : item.params) {
            std::cout << (param_index++ ? "," : "") << '"' << json_escape(key) << "\":\"" << json_escape(value) << '"';
          }
          std::cout << "},\"returns\":\"" << json_escape(item.returns) << "\"}";
        }
        std::cout << "]\n";
      } else {
        for (const auto &item : items) {
          std::cout << item.file << ":" << item.line << ": "
                    << (item.symbol.empty() ? "" : item.symbol + ": ")
                    << (item.brief.empty() ? item.text : item.brief) << "\n";
        }
      }
      return 0;
    }

    if (cmd == "generate") {
      ezdox::Config config = fs::exists(opt(parsed, "config", "")) ? ezdox::load_config(opt(parsed, "config")) : ezdox::default_config();
      if (!vals(parsed, "source").empty()) config.sources = to_paths(vals(parsed, "source"));
      if (!vals(parsed, "include").empty()) config.includes = to_paths(vals(parsed, "include"));
      if (!vals(parsed, "exclude").empty()) config.excludes = to_paths(vals(parsed, "exclude"));
      if (!vals(parsed, "targets").empty()) config.targets = vals(parsed, "targets");
      if (!vals(parsed, "markups").empty()) config.markups = vals(parsed, "markups");
      auto out = fs::path(opt(parsed, "output", "build/docs"));
      if (has(parsed, "clean") && fs::exists(out)) fs::remove_all(out);
      auto tpl = opt(parsed, "template");
      if (!tpl.empty()) ezdox::generate(config, out, fs::path(tpl));
      else ezdox::generate(config, out);
      if (has(parsed, "profile")) std::cout << "items=" << ezdox::scan_sources(config.sources, config.excludes, {}).size() << "\n";
      emit_line(std::cout, pretty, out.string());
      return 0;
    }

    if (cmd == "install") {
      ezdox::copy_install(opt(parsed, "output"), opt(parsed, "dest"), has(parsed, "update"), opt(parsed, "mode", "copy"));
      return 0;
    }

    if (cmd == "doctor") {
      auto resolved = ezdox::resolve_paths();
      if (has(parsed, "fix")) {
        fs::create_directories(resolved.bundles);
        fs::create_directories(resolved.cache);
        fs::create_directories(resolved.markups);
        fs::create_directories(resolved.targets);
      }
      std::cout << "home=" << resolved.home << "\n"
                << "config=" << ezdox::find_default_config() << "\n"
                << "ok\n";
      return 0;
    }

    print_help({}, pretty);
    return 3;
  } catch (const std::exception &error) {
    std::cerr << "ezdox: " << error.what() << "\n";
    return 1;
  }
}
