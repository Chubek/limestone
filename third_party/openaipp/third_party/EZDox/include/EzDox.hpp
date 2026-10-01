#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ezdox {

struct Paths {
  std::filesystem::path home;
  std::filesystem::path bundles;
  std::filesystem::path markups;
  std::filesystem::path targets;
  std::filesystem::path cache;
  std::filesystem::path config;
};

struct Config {
  std::string project = "EZDox Project";
  std::string version = "0.1.0";
  std::vector<std::filesystem::path> sources{"."};
  std::filesystem::path frontpage{};
  std::filesystem::path manual{};
  std::vector<std::filesystem::path> includes{};
  std::vector<std::filesystem::path> excludes{};
  std::vector<std::string> targets{"HTML"};
  std::vector<std::string> markups{"Markdown"};
  std::map<std::string, std::string> commands{};
  std::map<std::string, std::vector<std::string>> pipelines{};
  std::map<std::string, std::string> environment{};
};

struct ManualChapter {
  std::filesystem::path path;
  std::string title;
  std::string content;
};

struct DoxygenCommand {
  std::string id;
  std::string spelling;
  std::string title;
};

struct DocItem {
  std::filesystem::path file;
  std::size_t line = 0;
  std::size_t end_line = 0;
  std::string kind;
  std::string symbol;
  std::string declaration;
  std::string text;
  std::string brief;
  std::string details;
  std::map<std::string, std::string> params;
  std::string returns;
  std::vector<std::string> references;
  std::vector<std::string> commands;
  std::map<std::string, std::vector<std::string>> command_args;
};

struct DocumentModel {
  Config config;
  std::vector<DocItem> items;
};

Paths resolve_paths(const std::filesystem::path &home_override = {});
std::filesystem::path find_default_config();

Config default_config();
Config load_config(const std::filesystem::path &path);
void write_config(const Config &config, const std::filesystem::path &path, std::string_view format);
std::string dump_config(const Config &config, std::string_view format);
std::vector<std::string> validate_config(const Config &config);
std::vector<std::string> validate_config_against_schema(const Config &config, const std::filesystem::path &schema_path = {});
std::string config_key(const Config &config, std::string_view key);

std::vector<DoxygenCommand> load_doxygen_commands(const std::filesystem::path &manifest = {});
std::set<std::string> command_spellings(const std::vector<DoxygenCommand> &commands);
std::vector<DocItem> scrape_file_comments(const std::filesystem::path &file,
                                          const std::set<std::string> &commands = {});
std::vector<DocItem> scan_sources(const std::vector<std::filesystem::path> &roots,
                                  const std::vector<std::filesystem::path> &excludes = {},
                                  const std::set<std::string> &commands = {});
std::vector<DocItem> scan_sources_filtered(const std::vector<std::filesystem::path> &roots,
                                           const std::vector<std::filesystem::path> &excludes,
                                           const std::set<std::string> &commands,
                                           std::string_view command_filter,
                                           const std::vector<std::string> &glob_filters = {});

std::string apply_markup(std::string_view name, const DocumentModel &model);
void render_target(std::string_view name, const DocumentModel &model,
                   const std::filesystem::path &output_dir);
void generate(const Config &config, const std::filesystem::path &output_dir);
void render_target(std::string_view name, const DocumentModel &model,
                   const std::filesystem::path &output_dir,
                   const std::filesystem::path &template_dir);
void generate(const Config &config, const std::filesystem::path &output_dir,
              const std::filesystem::path &template_dir);

void build_bundle(const std::filesystem::path &source, const std::filesystem::path &output,
                  const std::string &name, const std::string &version,
                  const std::string &description);
void install_bundle(const std::filesystem::path &bundle, const std::filesystem::path &home,
                    bool force);
std::vector<std::filesystem::path> list_bundles(const std::filesystem::path &home);
void remove_bundle(const std::string &name, const std::filesystem::path &home);
std::vector<std::string> inspect_bundle(const std::filesystem::path &bundle);

struct RunOptions {
  bool dry_run = false;
  std::filesystem::path workdir{};
  std::map<std::string, std::string> environment{};
  std::vector<std::string> passthrough_args{};
  std::optional<unsigned> timeout_seconds{};
};

int run_named(const Config &config, const std::string &name, bool dry_run,
              const std::filesystem::path &workdir = {});
int run_named(const Config &config, const std::string &name, const RunOptions &options);
void copy_install(const std::filesystem::path &output, const std::filesystem::path &dest,
                  bool update, std::string_view mode = "copy");

std::string version();

} // namespace ezdox
