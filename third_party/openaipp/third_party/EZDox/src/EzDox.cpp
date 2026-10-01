#include "EzDox.hpp"

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>
#include <toml++/toml.h>
#include <valijson/adapters/nlohmann_json_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>
#include "EzDox-Internal.hpp"
#include "EzDox-Markup.hpp"
#include "EzDox-Target.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#ifdef EZDOX_USE_INJA
#include <inja/inja.hpp>
#endif

namespace fs = std::filesystem;

namespace ezdox {
namespace {
std::string anchor_id(std::string s);
std::string read_text(const fs::path &p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read: " + p.string());
  std::ostringstream s; s << in.rdbuf(); return s.str();
}
void write_text(const fs::path &p, std::string_view text) {
  if (!p.parent_path().empty()) fs::create_directories(p.parent_path());
  std::ofstream out(p, std::ios::binary);
  if (!out) throw std::runtime_error("cannot write: " + p.string());
  out << text;
}
std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
  return s;
}
std::string trim(std::string x) {
  while (!x.empty() && std::isspace(static_cast<unsigned char>(x.front()))) x.erase(x.begin());
  while (!x.empty() && std::isspace(static_cast<unsigned char>(x.back()))) x.pop_back();
  return x;
}
fs::path repo_root() {
  auto probe = fs::current_path();
  while (!probe.empty()) {
    if (fs::exists(probe / "AGENTS.md") && fs::exists(probe / "manifests")) return probe;
    if (probe == probe.root_path()) break;
    probe = probe.parent_path();
  }
  return fs::current_path();
}
fs::path resolve_repo_path(const fs::path &path) {
  if (path.empty() || path.is_absolute()) return path;
  auto direct = fs::current_path() / path;
  if (fs::exists(direct)) return direct;
  auto rooted = repo_root() / path;
  if (fs::exists(rooted)) return rooted;
  return path;
}
std::string markdown_title(const std::string &text, const fs::path &fallback) {
  std::istringstream input(text);
  std::string line;
  while (std::getline(input, line)) {
    auto t = trim(line);
    if (t.rfind("# ", 0) == 0) return trim(t.substr(2));
  }
  return fallback.stem().string();
}
std::vector<ManualChapter> load_manual_chapters(const Config &config) {
  std::vector<ManualChapter> chapters;
  if (config.manual.empty()) return chapters;
  auto manual_dir = resolve_repo_path(config.manual);
  if (!fs::exists(manual_dir) || !fs::is_directory(manual_dir)) return chapters;
  for (const auto &entry : fs::directory_iterator(manual_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".md") continue;
    auto content = read_text(entry.path());
    chapters.push_back({entry.path(), markdown_title(content, entry.path()), content});
  }
  std::sort(chapters.begin(), chapters.end(), [](const ManualChapter &lhs, const ManualChapter &rhs) {
    return lhs.path.filename().string() < rhs.path.filename().string();
  });
  return chapters;
}
std::string shell_quote(const std::string &s) {
  std::string out = "'";
  for (char c : s) out += c == '\'' ? "'\\''" : std::string(1, c);
  out += "'";
  return out;
}
std::string xml_escape(std::string_view s) {
  std::string out;
  for (char c : s) {
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out.push_back(c);
  }
  return out;
}
std::string latex_escape(std::string_view s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '\\': out += "\\textbackslash{}"; break;
      case '{': out += "\\{"; break;
      case '}': out += "\\}"; break;
      case '_': out += "\\_"; break;
      case '&': out += "\\&"; break;
      case '%': out += "\\%"; break;
      case '#': out += "\\#"; break;
      default: out.push_back(c); break;
    }
  }
  return out;
}
#ifdef EZDOX_USE_INJA
static std::string read_file_text(const fs::path &p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) throw std::runtime_error("cannot read template: " + p.string());
  std::ostringstream s; s << in.rdbuf(); return s.str();
}

static void copy_dir(const fs::path &src, const fs::path &dst) {
  if (!fs::exists(dst)) fs::create_directories(dst);
  for (auto &e : fs::directory_iterator(src)) {
    auto dest = dst / e.path().filename();
    if (fs::is_directory(e)) { copy_dir(e, dest); }
    else if (!fs::exists(dest) || fs::last_write_time(e) > fs::last_write_time(dest)) {
      fs::copy(e, dest, fs::copy_options::overwrite_existing);
    }
  }
}

static nlohmann::json item_to_json(const DocItem &it) {
  nlohmann::json j;
  auto fallback_title = it.symbol.empty() ? (it.file.filename().string() + ":" + std::to_string(it.line)) : it.symbol;
  j["file"] = it.file.generic_string();
  j["line"] = it.line;
  j["end_line"] = it.end_line;
  j["kind"] = it.kind;
  j["symbol"] = it.symbol;
  j["title"] = fallback_title;
  j["anchor"] = anchor_id(fallback_title);
  j["declaration"] = it.declaration;
  j["text"] = it.text;
  j["brief"] = it.brief;
  j["summary"] = it.brief.empty() ? it.text : it.brief;
  j["details"] = it.details;
  j["returns"] = it.returns;
  j["references"] = it.references;
  j["commands"] = it.commands;
  nlohmann::json params = nlohmann::json::object();
  for (auto &[k,v] : it.params) params[k] = v;
  j["params"] = params;
  nlohmann::json cmd_args = nlohmann::json::object();
  for (auto &[k,v] : it.command_args) cmd_args[k] = v;
  j["command_args"] = cmd_args;
  return j;
}

static nlohmann::json model_to_json(const DocumentModel &model) {
  nlohmann::json j;
  j["project"] = model.config.project;
  j["version"] = model.config.version;
  j["frontpage"] = model.config.frontpage.generic_string();
  j["manual"] = model.config.manual.generic_string();
  nlohmann::json sources = nlohmann::json::array();
  for (auto &s : model.config.sources) sources.push_back(s.generic_string());
  j["sources"] = sources;
  if (!model.config.frontpage.empty()) {
    auto frontpage_path = resolve_repo_path(model.config.frontpage);
    if (fs::exists(frontpage_path)) {
      auto frontpage_content = read_text(frontpage_path);
      j["frontpage_content"] = frontpage_content;
      j["frontpage_content_html"] = xml_escape(frontpage_content);
    }
  }
  nlohmann::json chapters = nlohmann::json::array();
  for (const auto &chapter : load_manual_chapters(model.config)) {
    chapters.push_back({
      {"path", chapter.path.generic_string()},
      {"title", chapter.title},
      {"content", chapter.content},
      {"content_html", xml_escape(chapter.content)},
    });
  }
  j["chapters"] = chapters;
  nlohmann::json items = nlohmann::json::array();
  for (auto &it : model.items) items.push_back(item_to_json(it));
  j["items"] = items;
  return j;
}




#endif
std::string anchor_id(std::string s) {
  if (s.empty()) return "item";
  for (char &c : s) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    else c = '-';
  }
  while (s.find("--") != std::string::npos) s = std::regex_replace(s, std::regex("--+"), "-");
  if (!s.empty() && s.front() == '-') s.erase(s.begin());
  if (!s.empty() && s.back() == '-') s.pop_back();
  return s.empty() ? "item" : s;
}
std::vector<std::string> split_words(std::string_view s) {
  std::vector<std::string> out; std::string cur;
  for (char c : s) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c=='_' || c=='-' || c=='.' || c=='/' || c=='*' || c=='@' || c=='\\') cur.push_back(c);
    else if (!cur.empty()) { out.push_back(cur); cur.clear(); }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}
enum class ConfigFormat { yaml, json, toml };

ConfigFormat config_format_for(std::string ext_or_name) {
  ext_or_name = lower(ext_or_name);
  if (!ext_or_name.empty() && ext_or_name.front() == '.') ext_or_name.erase(0, 1);
  if (ext_or_name == "yam" || ext_or_name == "yaml" || ext_or_name == "yml") return ConfigFormat::yaml;
  if (ext_or_name == "json") return ConfigFormat::json;
  if (ext_or_name == "toml") return ConfigFormat::toml;
  throw std::runtime_error("unsupported config format '" + ext_or_name + "'; use yaml, json, or toml");
}

// --- nlohmann (JSON) helpers, used by config_from_json ---
std::vector<std::string> json_value_strings(const nlohmann::json &v) {
  std::vector<std::string> out;
  if (v.is_string()) out.push_back(v.get<std::string>());
  else if (v.is_array()) for (const auto &x : v) if (x.is_string()) out.push_back(x.get<std::string>());
  return out;
}
std::string json_scalar(const nlohmann::json &o, const std::string &k, std::string fallback = {}) {
  if (!o.is_object() || !o.contains(k)) return fallback;
  const auto &v = o.at(k);
  if (v.is_string()) return v.get<std::string>();
  if (v.is_number_unsigned()) return std::to_string(v.get<std::uint64_t>());
  if (v.is_number_integer()) return std::to_string(v.get<std::int64_t>());
  if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
  return fallback;
}
std::vector<fs::path> json_path_list(const nlohmann::json &o, std::initializer_list<const char *> keys) {
  std::vector<fs::path> out;
  for (auto *k : keys) if (o.contains(k)) for (auto &s : json_value_strings(o.at(k))) out.emplace_back(s);
  return out;
}
std::vector<std::string> json_string_list(const nlohmann::json &o, std::initializer_list<const char *> keys) {
  std::vector<std::string> out;
  for (auto *k : keys) if (o.contains(k)) for (auto &s : json_value_strings(o.at(k))) out.push_back(s);
  return out;
}

// --- yaml-cpp helpers, used by config_from_yaml ---
std::vector<std::string> yaml_value_strings(const YAML::Node &v) {
  std::vector<std::string> out;
  if (v) {
    if (v.IsScalar()) out.push_back(v.as<std::string>());
    else if (v.IsSequence()) for (const auto &x : v) if (x.IsScalar()) out.push_back(x.as<std::string>());
  }
  return out;
}
std::string yaml_scalar(const YAML::Node &o, const std::string &k, std::string fallback = {}) {
  if (!o || !o[k]) return fallback;
  const auto &v = o[k];
  return v.IsScalar() ? v.as<std::string>() : fallback;
}
std::vector<fs::path> yaml_path_list(const YAML::Node &o, std::initializer_list<const char *> keys) {
  std::vector<fs::path> out;
  for (auto *k : keys) if (o[k]) for (auto &s : yaml_value_strings(o[k])) out.emplace_back(s);
  return out;
}
std::vector<std::string> yaml_string_list(const YAML::Node &o, std::initializer_list<const char *> keys) {
  std::vector<std::string> out;
  for (auto *k : keys) if (o[k]) for (auto &s : yaml_value_strings(o[k])) out.push_back(s);
  return out;
}

nlohmann::ordered_json config_to_json(const Config &c) {
  nlohmann::ordered_json root;
  root["project"] = c.project;
  root["version"] = c.version;
  if (!c.frontpage.empty()) root["frontpage"] = c.frontpage.generic_string();
  if (!c.manual.empty()) root["manual"] = c.manual.generic_string();
  auto path_array = [](const std::vector<fs::path> &xs) {
    nlohmann::ordered_json a = nlohmann::ordered_json::array();
    for (const auto &x : xs) a.push_back(x.string());
    return a;
  };
  root["sources"] = path_array(c.sources);
  root["includes"] = path_array(c.includes);
  root["excludes"] = path_array(c.excludes);
  root["targets"] = c.targets;
  root["markups"] = c.markups;
  nlohmann::ordered_json commands = nlohmann::ordered_json::object();
  for (auto &[k, v] : c.commands) commands[k] = v;
  root["commands"] = commands;
  nlohmann::ordered_json pipelines = nlohmann::ordered_json::object();
  for (auto &[k, v] : c.pipelines) pipelines[k] = v;
  root["pipelines"] = pipelines;
  nlohmann::ordered_json environment = nlohmann::ordered_json::object();
  for (auto &[k, v] : c.environment) environment[k] = v;
  root["environment"] = environment;
  return root;
}

YAML::Node config_to_yaml(const Config &c) {
  YAML::Node root;
  root["project"] = c.project;
  root["version"] = c.version;
  if (!c.frontpage.empty()) root["frontpage"] = c.frontpage.generic_string();
  if (!c.manual.empty()) root["manual"] = c.manual.generic_string();
  for (const auto &s : c.sources) root["sources"].push_back(s.string());
  for (const auto &s : c.includes) root["includes"].push_back(s.string());
  for (const auto &s : c.excludes) root["excludes"].push_back(s.string());
  for (const auto &t : c.targets) root["targets"].push_back(t);
  for (const auto &m : c.markups) root["markups"].push_back(m);
  for (auto &[k, v] : c.commands) root["commands"][k] = v;
  for (auto &[k, v] : c.pipelines) for (auto &step : v) root["pipelines"][k].push_back(step);
  for (auto &[k, v] : c.environment) root["environment"][k] = v;
  return root;
}

Config config_from_json(const nlohmann::json &o, const fs::path &config_dir) {
  Config c;
  if (!o.is_object()) throw std::runtime_error("config root must be an object");
  auto make_relative = [&](const std::vector<fs::path> &values) {
    std::vector<fs::path> out;
    out.reserve(values.size());
    for (const auto &value : values) out.push_back(value.is_absolute() ? value : config_dir / value);
    return out;
  };
  c.project = json_scalar(o, "project", json_scalar(o, "name", c.project));
  c.version = json_scalar(o, "version", c.version);
  auto sources = json_path_list(o, {"sources", "source"});
  if (!sources.empty()) c.sources = make_relative(sources);
  c.includes = make_relative(json_path_list(o, {"includes", "include"}));
  c.excludes = make_relative(json_path_list(o, {"excludes", "exclude"}));
  auto frontpage = json_scalar(o, "frontpage");
  c.frontpage = frontpage.empty() ? fs::path{} : config_dir / frontpage;
  auto manual = json_scalar(o, "manual");
  c.manual = manual.empty() ? fs::path{} : config_dir / manual;
  auto targets = json_string_list(o, {"targets", "target"});
  if (!targets.empty()) c.targets = targets;
  auto markups = json_string_list(o, {"markups", "markup"});
  if (!markups.empty()) c.markups = markups;
  if (o.contains("commands") && o.at("commands").is_object())
    for (auto it = o.at("commands").begin(); it != o.at("commands").end(); ++it)
      if (it.value().is_string()) c.commands[it.key()] = it.value().get<std::string>();
  if (o.contains("pipelines") && o.at("pipelines").is_object()) {
    for (auto it = o.at("pipelines").begin(); it != o.at("pipelines").end(); ++it) {
      if (it.value().is_array()) c.pipelines[it.key()] = json_value_strings(it.value());
      else if (it.value().is_string()) c.pipelines[it.key()] = {it.value().get<std::string>()};
    }
  }
  if (o.contains("environment") && o.at("environment").is_object())
    for (auto it = o.at("environment").begin(); it != o.at("environment").end(); ++it)
      if (it.value().is_string()) c.environment[it.key()] = it.value().get<std::string>();
  return c;
}

Config config_from_yaml(const YAML::Node &o, const fs::path &config_dir) {
  Config c;
  if (!o || !o.IsMap()) throw std::runtime_error("config root must be a mapping");
  auto make_relative = [&](const std::vector<fs::path> &values) {
    std::vector<fs::path> out;
    out.reserve(values.size());
    for (const auto &value : values) out.push_back(value.is_absolute() ? value : config_dir / value);
    return out;
  };
  c.project = yaml_scalar(o, "project", yaml_scalar(o, "name", c.project));
  c.version = yaml_scalar(o, "version", c.version);
  auto sources = yaml_path_list(o, {"sources", "source"});
  if (!sources.empty()) c.sources = make_relative(sources);
  c.includes = make_relative(yaml_path_list(o, {"includes", "include"}));
  c.excludes = make_relative(yaml_path_list(o, {"excludes", "exclude"}));
  auto frontpage = yaml_scalar(o, "frontpage");
  c.frontpage = frontpage.empty() ? fs::path{} : config_dir / frontpage;
  auto manual = yaml_scalar(o, "manual");
  c.manual = manual.empty() ? fs::path{} : config_dir / manual;
  auto targets = yaml_string_list(o, {"targets", "target"});
  if (!targets.empty()) c.targets = targets;
  auto markups = yaml_string_list(o, {"markups", "markup"});
  if (!markups.empty()) c.markups = markups;
  if (auto cmds = o["commands"]; cmds && cmds.IsMap())
    for (auto it = cmds.begin(); it != cmds.end(); ++it)
      if (it->second.IsScalar()) c.commands[it->first.as<std::string>()] = it->second.as<std::string>();
  if (auto pipes = o["pipelines"]; pipes && pipes.IsMap()) {
    for (auto it = pipes.begin(); it != pipes.end(); ++it) {
      if (it->second.IsSequence()) c.pipelines[it->first.as<std::string>()] = yaml_value_strings(it->second);
      else if (it->second.IsScalar()) c.pipelines[it->first.as<std::string>()] = {it->second.as<std::string>()};
    }
  }
  if (auto env = o["environment"]; env && env.IsMap())
    for (auto it = env.begin(); it != env.end(); ++it)
      if (it->second.IsScalar()) c.environment[it->first.as<std::string>()] = it->second.as<std::string>();
  return c;
}
// --- toml++ helpers, used by config_from_toml ---

std::string toml_scalar(const toml::table& t, const std::string& k, std::string fallback = {}) {
  auto* n = t.get(k);
  if (!n) return fallback;
  if (auto* s = n->as_string()) return std::string(s->get());
  if (auto* i = n->as_integer()) return std::to_string(i->get());
  if (auto* b = n->as_boolean()) return b->get() ? "true" : "false";
  return fallback;
}
std::vector<fs::path> toml_path_list(const toml::table& t, std::initializer_list<const char*> keys) {
  std::vector<fs::path> out;
  for (auto* k : keys) {
    auto* v = t.get(k);
    if (!v) continue;
    if (auto* s = v->as_string()) out.emplace_back(s->get());
    else if (auto* a = v->as_array()) for (auto& x : *a) if (auto* xs = x.as_string()) out.emplace_back(xs->get());
  }
  return out;
}
std::vector<std::string> toml_string_list(const toml::table& t, std::initializer_list<const char*> keys) {
  std::vector<std::string> out;
  for (auto* k : keys) {
    auto* v = t.get(k);
    if (!v) continue;
    if (auto* s = v->as_string()) out.push_back(std::string(s->get()));
    else if (auto* a = v->as_array()) for (auto& x : *a) if (auto* xs = x.as_string()) out.push_back(std::string(xs->get()));
  }
  return out;
}

std::string config_to_toml(const Config& c) {
  toml::table root;
  root.insert("project", c.project);
  root.insert("version", c.version);
  if (!c.frontpage.empty()) root.insert("frontpage", c.frontpage.generic_string());
  if (!c.manual.empty()) root.insert("manual", c.manual.generic_string());
  auto path_array = [](const std::vector<fs::path>& xs) {
    toml::array arr;
    for (const auto& x : xs) arr.push_back(x.string());
    return arr;
  };
  root.insert("sources", path_array(c.sources));
  root.insert("includes", path_array(c.includes));
  root.insert("excludes", path_array(c.excludes));
  toml::array targets;
  for (auto& t : c.targets) targets.push_back(t);
  root.insert("targets", targets);
  toml::array markups;
  for (auto& m : c.markups) markups.push_back(m);
  root.insert("markups", markups);
  toml::table cmds;
  for (auto& [k, v] : c.commands) cmds.insert(k, v);
  root.insert("commands", cmds);
  toml::table pipes;
  for (auto& [k, v] : c.pipelines) {
    toml::array steps;
    for (auto& s : v) steps.push_back(s);
    pipes.insert(k, steps);
  }
  root.insert("pipelines", pipes);
  toml::table env;
  for (auto& [k, v] : c.environment) env.insert(k, v);
  root.insert("environment", env);
  std::ostringstream ss;
  ss << root;
  return ss.str();
}

Config config_from_toml(std::string_view text, const fs::path& config_dir) {
  toml::table tbl;
  try { tbl = toml::parse(text); }
  catch (const toml::parse_error& ex) { throw std::runtime_error("invalid TOML config: " + std::string(ex.what())); }
  Config c;
  auto make_relative = [&](const std::vector<fs::path>& values) {
    std::vector<fs::path> out;
    out.reserve(values.size());
    for (const auto& value : values) out.push_back(value.is_absolute() ? value : config_dir / value);
    return out;
  };
  c.project = toml_scalar(tbl, "project", toml_scalar(tbl, "name", c.project));
  c.version = toml_scalar(tbl, "version", c.version);
  auto sources = toml_path_list(tbl, {"sources", "source"});
  if (!sources.empty()) c.sources = make_relative(sources);
  c.includes = make_relative(toml_path_list(tbl, {"includes", "include"}));
  c.excludes = make_relative(toml_path_list(tbl, {"excludes", "exclude"}));
  auto frontpage = toml_scalar(tbl, "frontpage");
  c.frontpage = frontpage.empty() ? fs::path{} : config_dir / frontpage;
  auto manual = toml_scalar(tbl, "manual");
  c.manual = manual.empty() ? fs::path{} : config_dir / manual;
  auto targets = toml_string_list(tbl, {"targets", "target"});
  if (!targets.empty()) c.targets = targets;
  auto markups = toml_string_list(tbl, {"markups", "markup"});
  if (!markups.empty()) c.markups = markups;
  if (auto* cmds = tbl.get("commands"); cmds && cmds->is_table()) {
    for (auto& [k, v] : *cmds->as_table()) if (auto* s = v.as_string()) c.commands[std::string(k.str())] = std::string(s->get());
  }
  if (auto* pipes = tbl.get("pipelines"); pipes && pipes->is_table()) {
    for (auto& [k, v] : *pipes->as_table()) {
      if (auto* a = v.as_array()) {
        for (auto& x : *a) if (auto* xs = x.as_string()) c.pipelines[std::string(k.str())].push_back(std::string(xs->get()));
      } else if (auto* s = v.as_string()) c.pipelines[std::string(k.str())] = {std::string(s->get())};
    }
  }
  if (auto* env = tbl.get("environment"); env && env->is_table()) {
    for (auto& [k, v] : *env->as_table()) if (auto* s = v.as_string()) c.environment[std::string(k.str())] = std::string(s->get());
  }
  return c;
}

bool excluded(const fs::path &p, const std::vector<fs::path> &excludes) {
  const auto s = p.generic_string();
  for (auto &e : excludes) if (!e.empty() && s.find(e.generic_string()) != std::string::npos) return true;
  return false;
}
std::string strip_comment(std::string line) {
  line = trim(line);
  for (auto p : {"///<", "//!<", "/**<", "/*!<", "///", "//!","/**","/*!","/*","*","//"}) {
    std::string pre = p;
    if (line.rfind(pre, 0) == 0) { line.erase(0, pre.size()); break; }
  }
  if (line.size() >= 2 && line.substr(line.size()-2) == "*/") line.resize(line.size()-2);
  return trim(line);
}
bool starts_doc_line(const std::string &line) {
  auto stripped = trim(line);
  return stripped.rfind("///", 0) == 0 || stripped.rfind("//!", 0) == 0;
}
bool starts_doc_block(const std::string &line) {
  auto stripped = trim(line);
  return stripped.rfind("/**", 0) == 0 || stripped.rfind("/*!", 0) == 0 || stripped.rfind("/**<", 0) == 0 || stripped.rfind("/*!<", 0) == 0;
}
bool inside_double_quotes(const std::string &line, std::size_t pos) {
  bool in_quotes = false;
  bool escaped = false;
  for (std::size_t index = 0; index < pos && index < line.size(); ++index) {
    char ch = line[index];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    if (ch == '"') in_quotes = !in_quotes;
  }
  return in_quotes;
}
std::size_t find_trailing_doc_marker(const std::string &line) {
  std::size_t best = std::string::npos;
  for (const std::string marker : {"///<", "//!<", "/**<", "/*!<"}) {
    auto pos = line.find(marker);
    while (pos != std::string::npos) {
      if (pos > 0 && !inside_double_quotes(line, pos)) {
        char prev = line[pos - 1];
        if (std::isspace(static_cast<unsigned char>(prev)) || prev == ';' || prev == ')' || prev == ']' || prev == '}') {
          best = std::min(best, pos);
          break;
        }
      }
      pos = line.find(marker, pos + 1);
    }
  }
  return best;
}
bool has_trailing_doc(const std::string &line) {
  return find_trailing_doc_marker(line) != std::string::npos;
}
std::string classify_symbol(const std::string &line) {
  std::string s = trim(line);
  if (s.empty()) return {};
  std::smatch m;
  if (std::regex_search(s, m, std::regex(R"(\b(class|struct|enum|namespace)\s+([A-Za-z_][A-Za-z0-9_:]*))"))) return m[2].str();
  if (std::regex_search(s, m, std::regex(R"(([A-Za-z_][A-Za-z0-9_:~]*)\s*\([^;{}]*\)\s*(const)?\s*[;{])"))) return m[1].str();
  if (std::regex_search(s, m, std::regex(R"(([A-Za-z_][A-Za-z0-9_]*)\s*(=|;))"))) return m[1].str();
  return {};
}
std::string classify_kind(const std::string &line) {
  std::string s = trim(line);
  if (s.find("class ") != std::string::npos) return "class";
  if (s.find("struct ") != std::string::npos) return "struct";
  if (s.find("enum ") != std::string::npos) return "enum";
  if (s.find("namespace ") != std::string::npos) return "namespace";
  if (s.find('(') != std::string::npos) return "function";
  return s.empty() ? "comment" : "declaration";
}
std::string command_name(std::string spelling) {
  if (!spelling.empty() && (spelling[0] == '@' || spelling[0] == '\\')) spelling.erase(spelling.begin());
  return lower(spelling);
}
std::string strip_known_commands(std::string_view text) {
  std::ostringstream out;
  std::istringstream in{std::string(text)};
  std::string line;
  while (std::getline(in, line)) {
    auto t = trim(line);
    if (t.rfind("@brief",0)==0 || t.rfind("\\brief",0)==0) out << trim(t.substr(6)) << "\n";
    else if (t.rfind("@details",0)==0 || t.rfind("\\details",0)==0) out << trim(t.substr(8)) << "\n";
    else if (t.rfind("@param",0)==0 || t.rfind("\\param",0)==0 ||
             t.rfind("@return",0)==0 || t.rfind("\\return",0)==0 ||
             t.rfind("@returns",0)==0 || t.rfind("\\returns",0)==0 ||
             t.rfind("@see",0)==0 || t.rfind("\\see",0)==0 ||
             t.rfind("@ref",0)==0 || t.rfind("\\ref",0)==0) continue;
    else out << line << "\n";
  }
  return trim(out.str());
}
void normalize_doc_item(DocItem &item) {
  std::istringstream in(item.text);
  std::ostringstream details;
  std::string line;
  while (std::getline(in, line)) {
    auto t = trim(line);
    std::smatch m;
    bool consumed = false;
    if (std::regex_search(t, m, std::regex(R"(([@\\]brief)\s+([^@\\]*))"))) { item.brief = trim(m[2].str()); consumed = true; }
    if (std::regex_search(t, m, std::regex(R"(([@\\]details)\s+([^@\\]*))"))) { details << trim(m[2].str()) << "\n"; consumed = true; }
    if (std::regex_search(t, m, std::regex(R"(([@\\]param)(?:\s+\[[^\]]+\])?\s+([A-Za-z_][A-Za-z0-9_]*)\s*([^@\\]*))"))) { item.params[m[2].str()] = trim(m[3].str()); consumed = true; }
    if (std::regex_search(t, m, std::regex(R"(([@\\]returns?)\s+([^@\\]*))"))) { item.returns = trim(m[2].str()); consumed = true; }
    if (std::regex_search(t, m, std::regex(R"(([@\\](see|ref|sa))\s+([^@\\]*))"))) { item.references.push_back(trim(m[3].str())); consumed = true; }
    if (!consumed && !t.empty()) details << t << "\n";

    std::regex inline_ref(R"(([@\\]ref)\s+([A-Za-z_][A-Za-z0-9_:]*))");
    for (std::sregex_iterator i(t.begin(), t.end(), inline_ref), e; i != e; ++i) item.references.push_back((*i)[2].str());
  }
  item.details = trim(details.str());
  if (item.brief.empty()) {
    auto first_nl = item.details.find('\n');
    item.brief = first_nl == std::string::npos ? item.details : item.details.substr(0, first_nl);
  }
  item.text = strip_known_commands(item.text);
  std::sort(item.references.begin(), item.references.end());
  item.references.erase(std::unique(item.references.begin(), item.references.end()), item.references.end());
}
void annotate_commands(DocItem &item, const std::set<std::string> &cmdset) {
  for (auto &line : std::vector<std::string>{item.text}) {
    std::istringstream in(line);
    std::string one;
    while (std::getline(in, one)) {
      for (auto &w : split_words(one)) {
        if (!cmdset.contains(w)) continue;
        item.commands.push_back(w);
        auto pos = one.find(w);
        auto rest = pos == std::string::npos ? std::string{} : trim(one.substr(pos + w.size()));
        if (!rest.empty()) item.command_args[w].push_back(rest);
      }
    }
  }
  std::sort(item.commands.begin(), item.commands.end());
  item.commands.erase(std::unique(item.commands.begin(), item.commands.end()), item.commands.end());
  normalize_doc_item(item);
}

bool glob_match(const fs::path &p, const std::vector<std::string> &filters) {
  if (filters.empty()) return true;
  const auto s = p.generic_string();
  for (auto g : filters) {
    if (g.rfind("**/*.", 0) == 0 && p.extension() == ("." + g.substr(5))) return true;
    if (g.rfind("*.", 0) == 0 && p.extension() == ("." + g.substr(2))) return true;
    g = std::regex_replace(g, std::regex(R"(\*\*/\*)"), ".*");
    g = std::regex_replace(g, std::regex(R"(\*)"), "[^/]*");
    g = std::regex_replace(g, std::regex(R"(\.)"), "\\.");
    try { if (std::regex_search(s, std::regex(g + "$"))) return true; } catch (...) {}
  }
  return false;
}
}

Paths resolve_paths(const fs::path &home_override) {
  fs::path home = home_override;
  if (home.empty()) {
    if (const char *e = std::getenv("EZDOX_HOME")) home = e;
    else if (const char *h = std::getenv("HOME")) home = fs::path(h) / ".ezdox";
    else home = ".ezdox";
  }
  return {home, home/"bundles", home/"markups", home/"targets", home/"cache",
          std::getenv("EZDOX_CONFIG") ? fs::path(std::getenv("EZDOX_CONFIG")) : fs::path("EZDox.yaml")};
}
fs::path find_default_config() {
  if (const char *e = std::getenv("EZDOX_CONFIG"); e && fs::exists(e)) return e;
  for (auto n : {"EZDox.yaml", "EZDox.yam", "EZDox.json", "EZDox.toml"})
    if (fs::exists(n)) return n;
  return "EZDox.yaml";
}
Config default_config() { return {}; }
Config load_config(const fs::path &path) {
  auto resolved = resolve_repo_path(path);
  auto config_dir = resolved.parent_path();
  std::string text = read_text(resolved);
  switch (config_format_for(resolved.extension().string())) {
    case ConfigFormat::json: {
      nlohmann::json parsed;
      try { parsed = nlohmann::json::parse(text); }
      catch (const std::exception &ex) { throw std::runtime_error("invalid JSON config " + resolved.string() + ": " + ex.what()); }
      return config_from_json(parsed, config_dir);
    }
    case ConfigFormat::yaml: {
      YAML::Node node;
      try { node = YAML::Load(text); }
      catch (const std::exception &ex) { throw std::runtime_error("invalid YAML config " + resolved.string() + ": " + ex.what()); }
      return config_from_yaml(node, config_dir);
    }
    case ConfigFormat::toml:
      return config_from_toml(text, config_dir);
  }
  return {};
}
void write_config(const Config &config, const fs::path &path, std::string_view format) {
  write_text(path, dump_config(config, format.empty() ? path.extension().string() : format));
}
std::string dump_config(const Config &config, std::string_view format) {
  switch (config_format_for(std::string(format))) {
    case ConfigFormat::json: return config_to_json(config).dump(2);
    case ConfigFormat::yaml: {
      YAML::Emitter out;
      out << config_to_yaml(config);
      return std::string(out.c_str());
    }
    case ConfigFormat::toml: return config_to_toml(config);
  }
  return {};
}
std::vector<std::string> validate_config(const Config &config) {
  std::vector<std::string> e;
  if (config.project.empty()) e.push_back("project is empty");
  if (config.sources.empty()) e.push_back("sources is empty");
  if (config.targets.empty()) e.push_back("targets is empty");
  if (config.markups.empty()) e.push_back("markups is empty");
  if (!config.frontpage.empty() && !fs::exists(config.frontpage)) e.push_back("frontpage does not exist");
  if (!config.manual.empty() && (!fs::exists(config.manual) || !fs::is_directory(config.manual))) e.push_back("manual directory does not exist");
  return e;
}
std::vector<std::string> validate_config_against_schema(const Config &config, const fs::path &schema_path) {
  std::vector<std::string> errors;
  fs::path schema_file = schema_path.empty() ? resolve_repo_path("manifests/ezdox-config.schema.json") : schema_path;
  if (!fs::exists(schema_file)) {
    errors.push_back("schema file not found: " + schema_file.string());
    return errors;
  }
  try {
    auto schema_text = read_text(schema_file);
    auto schema_json = nlohmann::json::parse(schema_text);
    auto config_json = config_to_json(config);
    
    valijson::Schema schema;
    valijson::SchemaParser parser;
    valijson::adapters::NlohmannJsonAdapter schema_adapter(schema_json);
    parser.populateSchema(schema_adapter, schema);
    
    valijson::Validator validator;
    valijson::adapters::NlohmannJsonAdapter target_adapter(config_json);
    valijson::ValidationResults results;
    if (!validator.validate(schema, target_adapter, &results)) {
      for (auto &err : results) {
        std::string desc;
        for (auto &ctx : err.context) desc += ctx + "/";
        desc += " \u2014 " + err.description;
        errors.push_back(desc);
      }
    }
  } catch (const std::exception &ex) {
    errors.push_back("schema validation error: " + std::string(ex.what()));
  }
  return errors;
}

std::string config_key(const Config &config, std::string_view key) {
  if (key == "project" || key == "name") return config.project;
  if (key == "version") return config.version;
  if (key == "targets") { std::ostringstream o; for (auto &x: config.targets) o << x << "\n"; return o.str(); }
  if (key == "markups") { std::ostringstream o; for (auto &x: config.markups) o << x << "\n"; return o.str(); }
  if (key == "sources") { std::ostringstream o; for (auto &x: config.sources) o << x.string() << "\n"; return o.str(); }
  if (key == "frontpage") return config.frontpage.string();
  if (key == "manual") return config.manual.string();
  if (key == "commands") { std::ostringstream o; for (auto &[k,v]: config.commands) o << k << ": " << v << "\n"; return o.str(); }
  if (key == "pipelines") { std::ostringstream o; for (auto &[k,v]: config.pipelines) { o << k << ":"; for (auto &s:v) o << " " << s; o << "\n"; } return o.str(); }
  return {};
}
std::vector<DoxygenCommand> load_doxygen_commands(const fs::path &manifest) {
  fs::path p = manifest.empty() ? resolve_repo_path("manifests/doxygen-commands.yaml") : resolve_repo_path(manifest);
  std::string text = fs::exists(p) ? read_text(p) : "";
  std::vector<DoxygenCommand> out;
  std::regex item(R"(\n\s*-\s+id:\s*([^\n]+).*?\n\s*title:\s*\"?([\\@][A-Za-z_][A-Za-z0-9_]*)[^\"\n]*)", std::regex::icase);
  for (std::sregex_iterator i(text.begin(), text.end(), item), e; i != e; ++i)
    out.push_back({(*i)[1].str(), (*i)[2].str(), (*i)[2].str()});
  if (out.empty()) for (auto s : {"@param","@return","@brief","\\param","\\return","\\brief"}) out.push_back({s,s,s});
  for (auto s : {"@param","@return","@brief","\\param","\\return","\\brief"}) out.push_back({s,s,s});
  return out;
}
std::set<std::string> command_spellings(const std::vector<DoxygenCommand> &commands) {
  std::set<std::string> s;
  for (auto &c : commands) { s.insert(c.spelling); if (!c.spelling.empty()) s.insert(std::string(c.spelling[0]=='\\'?"@":"\\")+c.spelling.substr(1)); }
  return s;
}
std::vector<DocItem> scan_sources(const std::vector<fs::path> &roots, const std::vector<fs::path> &excludes, const std::set<std::string> &commands) {
  return scan_sources_filtered(roots, excludes, commands, {}, {});
}

std::vector<DocItem> scan_sources_filtered(const std::vector<fs::path> &roots, const std::vector<fs::path> &excludes, const std::set<std::string> &commands, std::string_view command_filter, const std::vector<std::string> &glob_filters) {
  std::vector<DocItem> out;
  auto cmdset = commands.empty() ? command_spellings(load_doxygen_commands()) : commands;
  auto accept_item = [&](DocItem &item) {
    annotate_commands(item, cmdset);
    if (!command_filter.empty() && std::find(item.commands.begin(), item.commands.end(), command_filter) == item.commands.end()) return;
    out.push_back(std::move(item));
  };
  for (auto &root : roots) {
    if (!fs::exists(root)) continue;
    auto visit = [&](const fs::path &p) {
      if (excluded(p, excludes) || !fs::is_regular_file(p) || !glob_match(p, glob_filters)) return;
      std::ifstream in(p); if (!in) return;
      std::vector<std::pair<std::size_t,std::string>> lines;
      std::string line; std::size_t n=0;
      while (std::getline(in,line)) {
        ++n;
        lines.push_back({n,line});
      }
      for (std::size_t i=0; i<lines.size(); ++i) {
        const auto &[line_no, raw] = lines[i];
        auto stripped = trim(raw);
        if (starts_doc_line(raw)) {
          DocItem item; item.file=p; item.line=line_no; item.end_line=line_no; item.kind="comment";
          std::ostringstream text;
          while (i < lines.size()) {
            auto cur = lines[i].second;
            if (!starts_doc_line(cur)) break;
            text << strip_comment(cur) << "\n";
            item.end_line = lines[i].first;
            ++i;
          }
          if (i < lines.size()) { item.declaration = trim(lines[i].second); item.symbol = classify_symbol(item.declaration); item.kind = classify_kind(item.declaration); }
          --i;
          item.text = trim(text.str());
          accept_item(item);
        } else if (starts_doc_block(raw)) {
          DocItem item; item.file=p; item.line=line_no; item.end_line=line_no; item.kind="block";
          std::ostringstream text;
          bool done=false;
          while (i < lines.size()) {
            auto cur = lines[i].second;
            text << strip_comment(cur) << "\n";
            item.end_line = lines[i].first;
            if (cur.find("*/") != std::string::npos) { done=true; break; }
            ++i;
          }
          if (done && i + 1 < lines.size()) { item.declaration = trim(lines[i+1].second); item.symbol = classify_symbol(item.declaration); item.kind = classify_kind(item.declaration); }
          item.text = trim(text.str());
          accept_item(item);
        } else if (has_trailing_doc(raw)) {
          auto pos = find_trailing_doc_marker(raw);
          if (pos != std::string::npos) {
            DocItem item; item.file=p; item.line=line_no; item.end_line=line_no; item.declaration=trim(raw.substr(0,pos)); item.kind=classify_kind(item.declaration); item.symbol=classify_symbol(item.declaration);
            item.text=strip_comment(raw.substr(pos));
            accept_item(item);
          }
        }
      }
    };
    if (fs::is_directory(root)) for (auto &e : fs::recursive_directory_iterator(root)) visit(e.path());
    else visit(root);
  }
  return out;
}
std::string apply_markup(std::string_view name, const DocumentModel &model) {
  return resolve_markup(name, model);
}
void render_target(std::string_view name, const DocumentModel &model, const fs::path &output_dir) {
  render_target(name, model, output_dir, fs::path{});
}
void render_target(std::string_view name, const DocumentModel &model, const fs::path &output_dir, const fs::path &template_dir) {
  auto n = internal::lower(std::string(name));
  if (n == "html") target_html(model, output_dir, template_dir);
  else if (n == "latex") target_latex(model, output_dir, template_dir);
  else if (n == "xml") target_xml(model, output_dir);
  else if (n == "manpage") target_manpage(model, output_dir);
  else if (n == "roff") target_roff(model, output_dir);
  else {
    auto body = apply_markup(model.config.markups.empty() ? "Markdown" : model.config.markups.front(), model);
    write_text(output_dir / (std::string(name) + ".txt"), body);
  }
}
void generate(const Config &config, const fs::path &output_dir, const fs::path &template_dir) {
  DocumentModel model{config, scan_sources(config.sources, config.excludes, {})};
  for (auto &t : config.targets) render_target(t, model, output_dir / lower(t), template_dir);
}
void generate(const Config &config, const fs::path &output_dir) {
  DocumentModel model{config, scan_sources(config.sources, config.excludes, {})};
  for (auto &t : config.targets) render_target(t, model, output_dir / lower(t));
}
void build_bundle(const fs::path &source, const fs::path &output, const std::string &name, const std::string &version, const std::string &description) {
  (void)source; (void)output; (void)name; (void)version; (void)description;
  throw std::runtime_error("bundle build is not implemented: no zip backend is configured");
}
void install_bundle(const fs::path &bundle, const fs::path &home, bool force) {
  (void)bundle; (void)home; (void)force;
  throw std::runtime_error("bundle install is not implemented: no zip backend is configured");
}
std::vector<fs::path> list_bundles(const fs::path &home) {
  std::vector<fs::path> out; auto dir=(home.empty()?resolve_paths().home:home)/"bundles";
  if (fs::exists(dir)) for (auto &e: fs::directory_iterator(dir)) out.push_back(e.path());
  return out;
}
void remove_bundle(const std::string &name, const fs::path &home) {
  fs::remove_all((home.empty()?resolve_paths().home:home)/"bundles"/name);
}
std::vector<std::string> inspect_bundle(const fs::path &bundle) {
  (void)bundle;
  throw std::runtime_error("bundle inspect is not implemented: no zip backend is configured");
}
int run_named(const Config &config, const std::string &name, bool dry_run, const fs::path &workdir) {
  RunOptions options; options.dry_run = dry_run; options.workdir = workdir;
  return run_named(config, name, options);
}
int run_named(const Config &config, const std::string &name, const RunOptions &options) {
  std::vector<std::string> steps;
  if (auto it = config.commands.find(name); it != config.commands.end()) steps.push_back(it->second);
  else if (auto pi = config.pipelines.find(name); pi != config.pipelines.end()) {
    for (auto &step : pi->second) {
      if (auto ci = config.commands.find(step); ci != config.commands.end()) steps.push_back(ci->second);
      else steps.push_back(step);
    }
  } else throw std::runtime_error("unknown command or pipeline: "+name);

  int rc = 0;
  for (auto cmd : steps) {
    for (auto &arg : options.passthrough_args) cmd += " " + shell_quote(arg);
    std::string prefix;
    for (auto &[k,v] : config.environment) prefix += k + "=" + shell_quote(v) + " ";
    for (auto &[k,v] : options.environment) prefix += k + "=" + shell_quote(v) + " ";
    if (!options.timeout_seconds) {
      if (!options.workdir.empty()) cmd = "cd " + shell_quote(options.workdir.string()) + " && " + prefix + cmd;
      else cmd = prefix + cmd;
    } else {
      // Wrap in sh -c so that shell handles VAR=value prefixing correctly
      cmd = "timeout " + std::to_string(*options.timeout_seconds) + "s sh -c " + shell_quote(prefix + cmd);
      if (!options.workdir.empty()) cmd = "cd " + shell_quote(options.workdir.string()) + " && " + cmd;
    }
    if (options.dry_run) { std::cout << cmd << "\n"; continue; }
    rc = std::system(cmd.c_str());
    if (rc != 0) return rc;
  }
  return rc;
}
void copy_install(const fs::path &output, const fs::path &dest, bool update, std::string_view mode) {
  if (!fs::exists(output)) throw std::runtime_error("missing output: "+output.string());
  if (mode == "symlink") {
    if (fs::exists(dest)) {
      if (!update) fs::remove_all(dest);
      else return;
    }
    fs::create_directories(dest.parent_path());
    fs::create_directory_symlink(fs::absolute(output), dest);
    return;
  }
  if (mode != "copy" && mode != "rsync") throw std::runtime_error("unsupported install mode: "+std::string(mode));
  fs::create_directories(dest);
  auto opts = fs::copy_options::recursive | (update ? fs::copy_options::update_existing : fs::copy_options::overwrite_existing);
  fs::copy(output, dest, opts);
}
std::string version() { return "EZDox 0.2.0-pass2"; }
} // namespace ezdox
