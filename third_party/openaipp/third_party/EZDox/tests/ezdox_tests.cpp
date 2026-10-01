#include "EzDox.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace fs = std::filesystem;
static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " #x "\n"; ++failures; } } while(0)

static void write(const fs::path &p, const std::string &s) {
  fs::create_directories(p.parent_path());
  std::ofstream(p) << s;
}

// Forward declarations for new tests
static void test_config_validation_edge_cases();
static void test_config_key_edge_cases();
static void test_config_toml_roundtrip(const std::filesystem::path &tmp);
static void test_doxygen_commands_edge_cases(const std::filesystem::path &tmp);
static void test_scan_sources_edge_cases(const std::filesystem::path &tmp);
static void test_scan_sources_trailing_and_block(const std::filesystem::path &tmp);
static void test_markup_all_formats(const std::filesystem::path &tmp);
static void test_markup_empty_model();
static void test_target_rendering_all(const std::filesystem::path &tmp);
static void test_validate_config_against_schema();
static void test_scan_sources_filtered_glob(const std::filesystem::path &tmp);
static void test_version_string();
static void test_run_named_with_env(const std::filesystem::path &tmp);
static void test_copy_install_modes(const std::filesystem::path &tmp);
static void test_scan_sources_multiple_roots(const std::filesystem::path &tmp);

int main() {
  auto tmp = fs::temp_directory_path() / ("ezdox-tests-" + std::to_string(::getpid()));
  fs::remove_all(tmp);
  fs::create_directories(tmp);

  CHECK(ezdox::version().find("EZDox") != std::string::npos);
  auto p = ezdox::resolve_paths(tmp/"home");
  CHECK(p.home == tmp/"home");
  CHECK(p.bundles == tmp/"home"/"bundles");
  CHECK(p.cache == tmp/"home"/"cache");

  auto c = ezdox::default_config();
  CHECK(c.project == "EZDox Project");
  CHECK(!c.sources.empty());
  CHECK(c.targets.front() == "HTML");
  CHECK(c.markups.front() == "Markdown");
  CHECK(ezdox::validate_config(c).empty());
  CHECK(ezdox::config_key(c, "project") == c.project);
  CHECK(ezdox::config_key(c, "targets").find("HTML") != std::string::npos);

  c.project = "T"; c.version = "1";
  c.sources = std::vector<fs::path>{tmp/"src"};
  c.includes = std::vector<fs::path>{tmp/"include"};
  c.excludes = std::vector<fs::path>{tmp/"build"};
  c.commands["hello"] = "true";
  c.commands["echo"] = "echo ok";
  c.pipelines["pipe"] = {"hello", "echo"};
  c.environment["EZDOX_TEST_ENV"] = "1";
  fs::create_directories(c.sources.front());
  write(c.sources.front()/"a.cpp", "/// @brief A @param x value @return ax @see b\nint a(int x);\n/**\n * @return value\n * block text @ref a\n */\nint b();\nint c; /**< @brief trailing field */\n");
  ezdox::write_config(c, tmp/"EZDox.yaml", "yaml");
  auto cy = ezdox::load_config(tmp/"EZDox.yaml");
  CHECK(cy.project == "T");
  CHECK(cy.version == "1");
  CHECK(!cy.sources.empty());
  CHECK(!ezdox::dump_config(cy, "json").empty());
  ezdox::write_config(c, tmp/"EZDox.json", "json");
  CHECK(ezdox::load_config(tmp/"EZDox.json").project == "T");

  auto cmds = ezdox::load_doxygen_commands();
  CHECK(!cmds.empty());
  auto spell = ezdox::command_spellings(cmds);
  CHECK(spell.contains("@brief") || spell.contains("\\brief"));
  auto items = ezdox::scan_sources(c.sources, {}, spell);
  CHECK(items.size() == 3);
  CHECK(items[0].brief == "A");
  CHECK(!items[0].commands.empty());
  CHECK(items[0].symbol == "a");
  CHECK(items[0].kind == "function");
  CHECK(items[0].brief == "A");
  CHECK(items[0].params["x"] == "value");
  CHECK(items[0].returns == "ax");
  CHECK(!items[0].references.empty());
  CHECK(items[0].declaration.find("int a") != std::string::npos);
  auto returns = ezdox::scan_sources_filtered(c.sources, {}, spell, "@return");
  CHECK(returns.size() == 2);
  auto block_return = returns.size() > 1 ? returns[1] : returns[0];
  CHECK(block_return.text.find("block text") != std::string::npos);
  CHECK(block_return.returns == "value");
  CHECK(!block_return.references.empty());
  CHECK(ezdox::scan_sources_filtered(c.sources, {}, spell, "@brief", {"**/*.cpp"}).size() >= 2);
  CHECK(ezdox::scan_sources(std::vector<fs::path>{tmp/"missing"}, {}, spell).empty());
  CHECK(ezdox::scan_sources(c.sources, std::vector<fs::path>{tmp/"src"}, spell).empty());

  ezdox::DocumentModel model{c, items};
  CHECK(ezdox::apply_markup("Markdown", model).find("# T") != std::string::npos);
  CHECK(ezdox::apply_markup("Docbook", model).find("<article>") != std::string::npos);
  CHECK(ezdox::apply_markup("ReStructuredText", model).find("T\n=") != std::string::npos);
  CHECK(ezdox::apply_markup("XWiki", model).find("= T") != std::string::npos);

  ezdox::render_target("HTML", model, tmp/"out-html");
  CHECK(fs::exists(tmp/"out-html"/"index.html"));
  CHECK(ezdox::apply_markup("Markdown", model).find("](#b)") != std::string::npos);
  CHECK(ezdox::apply_markup("Markdown", model).find("Parameters") != std::string::npos);
  CHECK(ezdox::apply_markup("Docbook", model).find("<parameter") != std::string::npos);
  ezdox::render_target("XML", model, tmp/"out-xml");
  CHECK(fs::exists(tmp/"out-xml"/"index.xml"));
  ezdox::render_target("LaTeX", model, tmp/"out-tex");
  CHECK(fs::exists(tmp/"out-tex"/"manual.tex"));
  ezdox::render_target("Manpage", model, tmp/"out-man");
  CHECK(fs::exists(tmp/"out-man"/"ezdox.1"));
  ezdox::generate(c, tmp/"gen");
  CHECK(fs::exists(tmp/"gen"/"html"/"index.html"));

  // Bundle build/install/inspect are stubbed out (no zip backend): they must throw,
  // while list/remove still operate on the bundles directory directly.
  write(tmp/"bundle-src"/"file.txt", "data");
  bool threw = false;
  try { ezdox::build_bundle(tmp/"bundle-src", tmp/"bundle.ezb", "b", "1", "d"); } catch (const std::exception &) { threw = true; }
  CHECK(threw);
  threw = false;
  try { ezdox::inspect_bundle(tmp/"bundle.ezb"); } catch (const std::exception &) { threw = true; }
  CHECK(threw);
  threw = false;
  try { ezdox::install_bundle(tmp/"bundle.ezb", tmp/"home", true); } catch (const std::exception &) { threw = true; }
  CHECK(threw);
  CHECK(ezdox::list_bundles(tmp/"home").empty());
  ezdox::remove_bundle("bundle", tmp/"home");
  CHECK(ezdox::list_bundles(tmp/"home").empty());

  CHECK(ezdox::run_named(c, "hello", true) == 0);
  ezdox::RunOptions ro; ro.dry_run = true; ro.environment["A"] = "B"; ro.timeout_seconds = 2;
  CHECK(ezdox::run_named(c, "pipe", ro) == 0);
  ezdox::copy_install(tmp/"gen", tmp/"installed", false);
  CHECK(fs::exists(tmp/"installed"/"html"/"index.html") || fs::exists(tmp/"installed"/"gen"/"html"/"index.html"));
  ezdox::copy_install(tmp/"gen", tmp/"linked-docs", false, "symlink");
  CHECK(fs::exists(tmp/"linked-docs"));

  test_config_validation_edge_cases();
  test_config_key_edge_cases();
  test_config_toml_roundtrip(tmp);
  test_doxygen_commands_edge_cases(tmp);
  test_scan_sources_edge_cases(tmp);
  test_scan_sources_trailing_and_block(tmp);
  test_markup_all_formats(tmp);
  test_markup_empty_model();
  test_target_rendering_all(tmp);
  test_validate_config_against_schema();
  test_scan_sources_filtered_glob(tmp);
  test_version_string();
  test_run_named_with_env(tmp);
  test_copy_install_modes(tmp);
  test_scan_sources_multiple_roots(tmp);

  fs::remove_all(tmp);
  if (failures) return 1;
  std::cout << "ezdox unit checks passed\n";
  return 0;
}

// ─── Test 1: Config validation edge cases ───
static void test_config_validation_edge_cases() {
  auto c = ezdox::default_config();
  CHECK(ezdox::validate_config(c).empty());

  c.project.clear();
  auto diags = ezdox::validate_config(c);
  CHECK(!diags.empty());

  c = ezdox::default_config();
  c.sources.clear();
  diags = ezdox::validate_config(c);
  CHECK(!diags.empty());

  c = ezdox::default_config();
  c.targets.clear();
  diags = ezdox::validate_config(c);
  CHECK(!diags.empty());

  c = ezdox::default_config();
  c.markups.clear();
  diags = ezdox::validate_config(c);
  CHECK(!diags.empty());
}

// ─── Test 2: Config key retrieval edge cases ───
static void test_config_key_edge_cases() {
  auto c = ezdox::default_config();
  c.project = "MyProject";
  c.version = "2.0.0";
  c.sources = {"/tmp/foo", "/tmp/bar"};
  c.targets = {"HTML", "XML"};
  c.markups = {"Markdown"};

  CHECK(ezdox::config_key(c, "project") == "MyProject");
  CHECK(ezdox::config_key(c, "version") == "2.0.0");
  CHECK(ezdox::config_key(c, "targets").find("HTML") != std::string::npos);
  CHECK(ezdox::config_key(c, "targets").find("XML") != std::string::npos);
  CHECK(ezdox::config_key(c, "markups").find("Markdown") != std::string::npos);
  CHECK(ezdox::config_key(c, "nonexistent").empty());
  CHECK(ezdox::config_key(c, "").empty());
}

// ─── Test 3: Config TOML roundtrip ───
static void test_config_toml_roundtrip(const std::filesystem::path &tmp) {
  auto c = ezdox::default_config();
  c.project = "TOMLTest";
  c.version = "3.0.0";
  c.sources = {tmp / "src"};
  c.commands["build"] = "make";
  c.environment["CC"] = "gcc";

  auto toml_path = tmp / "EZDox.toml";
  ezdox::write_config(c, toml_path, "toml");
  CHECK(std::filesystem::exists(toml_path));

  auto loaded = ezdox::load_config(toml_path);
  CHECK(loaded.project == "TOMLTest");
  CHECK(loaded.version == "3.0.0");
  CHECK(!loaded.commands.empty());

  auto dumped = ezdox::dump_config(c, "toml");
  CHECK(!dumped.empty());
  CHECK(dumped.find("TOMLTest") != std::string::npos);
}

// ─── Test 4: Doxygen commands edge cases ───
static void test_doxygen_commands_edge_cases(const std::filesystem::path &tmp) {
  auto cmds = ezdox::load_doxygen_commands();
  CHECK(!cmds.empty());

  auto missing = ezdox::load_doxygen_commands(tmp / "nonexistent_manifest.yaml");
  CHECK(!missing.empty());

  auto spell = ezdox::command_spellings(cmds);
  CHECK(spell.contains("@brief"));
  CHECK(spell.contains("\\brief"));
  CHECK(spell.contains("@param"));
  CHECK(spell.contains("\\param"));
  CHECK(spell.contains("@return"));
  CHECK(spell.contains("\\return"));
}

// ─── Test 5: Scan sources edge cases ───
static void test_scan_sources_edge_cases(const std::filesystem::path &tmp) {
  auto spell = ezdox::command_spellings(ezdox::load_doxygen_commands());
  auto empty_dir = tmp / "empty_src";
  std::filesystem::create_directories(empty_dir);
  auto r = ezdox::scan_sources({empty_dir}, {}, spell);
  CHECK(r.empty());

  r = ezdox::scan_sources({tmp / "nonexistent_dir"}, {}, spell);
  CHECK(r.empty());
}

// ─── Test 6: Scan sources trailing and block comments ───
static void test_scan_sources_trailing_and_block(const std::filesystem::path &tmp) {
  auto src = tmp / "mixed_src";
  std::filesystem::create_directories(src);
  write(src / "mixed.cpp",
    "/// @brief triple-slash\n"
    "int triple();\n"
    "/**\n"
    " * @brief block-comment\n"
    " * @param x value\n"
    " */\n"
    "int block(int x);\n"
    "int trailing; ///< @brief trailing comment\n"
  );
  auto spell = ezdox::command_spellings(ezdox::load_doxygen_commands());
  auto items = ezdox::scan_sources({src}, {}, spell);
  CHECK(items.size() == 3);

  bool has_triple = false, has_block = false, has_trailing = false;
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i].brief == "triple-slash") has_triple = true;
    if (items[i].brief == "block-comment") has_block = true;
    if (items[i].brief == "trailing comment") has_trailing = true;
  }
  CHECK(has_triple);
  CHECK(has_block);
  CHECK(has_trailing);
}

// ─── Test 7: Markup all formats with diverse content ───
static void test_markup_all_formats(const std::filesystem::path &tmp) {
  auto c = ezdox::default_config();
  c.project = "Diverse";
  c.version = "1.0.0";

  auto src = tmp / "diverse_src";
  std::filesystem::create_directories(src);
  write(src / "d.cpp",
    "/// @brief Alpha @param a alpha @param b beta @return gamma @see delta\n"
    "int d(int a, int b);\n"
  );
  auto spell = ezdox::command_spellings(ezdox::load_doxygen_commands());
  auto items = ezdox::scan_sources({src}, {}, spell);
  ezdox::DocumentModel model{c, items};

  auto md = ezdox::apply_markup("Markdown", model);
  CHECK(md.find("# Diverse") != std::string::npos);
  CHECK(md.find("Parameters") != std::string::npos);

  auto db = ezdox::apply_markup("Docbook", model);
  CHECK(db.find("<article>") != std::string::npos);
  CHECK(db.find("Diverse") != std::string::npos);

  auto rst = ezdox::apply_markup("ReStructuredText", model);
  CHECK(rst.find("Diverse") != std::string::npos);

  auto xwiki = ezdox::apply_markup("XWiki", model);
  CHECK(xwiki.find("Diverse") != std::string::npos);
}

// ─── Test 8: Markup with empty model ───
static void test_markup_empty_model() {
  auto c = ezdox::default_config();
  ezdox::DocumentModel model{c, {}};
  auto md = ezdox::apply_markup("Markdown", model);
  CHECK(!md.empty());
  CHECK(md.find(c.project) != std::string::npos);
}

// ─── Test 9: Target rendering all formats ───
static void test_target_rendering_all(const std::filesystem::path &tmp) {
  auto c = ezdox::default_config();
  c.project = "RenderTest";
  auto src = tmp / "render_src";
  std::filesystem::create_directories(src);
  write(src / "r.cpp", "/// @brief Render test\nint r();\n");
  auto spell = ezdox::command_spellings(ezdox::load_doxygen_commands());
  auto items = ezdox::scan_sources({src}, {}, spell);
  ezdox::DocumentModel model{c, items};

  ezdox::render_target("HTML", model, tmp / "out-html");
  CHECK(std::filesystem::exists(tmp / "out-html" / "index.html"));

  ezdox::render_target("XML", model, tmp / "out-xml");
  CHECK(std::filesystem::exists(tmp / "out-xml" / "index.xml"));

  ezdox::render_target("LaTeX", model, tmp / "out-tex");
  CHECK(std::filesystem::exists(tmp / "out-tex" / "manual.tex"));

  ezdox::render_target("Manpage", model, tmp / "out-man");
  CHECK(std::filesystem::exists(tmp / "out-man" / "ezdox.1"));
}

// ─── Test 10: Validate config against schema ───
static void test_validate_config_against_schema() {
  auto c = ezdox::default_config();
  c.project = "SchemaTest";
  c.sources = {"/tmp/schema_test_src"};
  c.targets = {"HTML"};
  c.markups = {"Markdown"};
  auto diags = ezdox::validate_config_against_schema(c);
  // The function either returns empty (valid) or schema validation errors;
  // both are acceptable as long as the function doesn't crash.
  // If diags is non-empty, they should be string messages.
  for (auto &d : diags) {
    CHECK(!d.empty());
  }

  // Test with a clearly invalid config (empty required fields)
  c.project.clear();
  c.sources = {};
  c.targets = {};
  c.markups = {};
  diags = ezdox::validate_config_against_schema(c);
  CHECK(!diags.empty());
  for (auto &d : diags) {
    CHECK(!d.empty());
  }
}

// ─── Test 11: Scan sources filtered with glob patterns ───
static void test_scan_sources_filtered_glob(const std::filesystem::path &tmp) {
  auto src = tmp / "glob_src";
  std::filesystem::create_directories(src);
  write(src / "a.cpp", "/// @brief Alpha\nint a();\n");
  write(src / "a.h", "/// @brief AlphaHeader\nint a();\n");
  auto spell = ezdox::command_spellings(ezdox::load_doxygen_commands());

  auto all = ezdox::scan_sources({src}, {}, spell);
  CHECK(all.size() == 2);

  auto cpp_only = ezdox::scan_sources_filtered({src}, {}, spell, {}, {"**/*.cpp"});
  CHECK(cpp_only.size() == 1);
  CHECK(cpp_only[0].brief == "Alpha");
  CHECK(cpp_only[0].file.filename() == "a.cpp");
}

// ─── Test 12: Version string ───
static void test_version_string() {
  auto v = ezdox::version();
  CHECK(v.find("EZDox") != std::string::npos);
  CHECK(v.size() > 5);
}

// ─── Test 13: Run named with env vars ───
static void test_run_named_with_env(const std::filesystem::path &tmp) {
  auto c = ezdox::default_config();
  c.commands["hello"] = "true";
  c.commands["echo_ok"] = "echo ok";
  c.pipelines["both"] = {"hello", "echo_ok"};
  c.environment["EZDOX_TEST_ENV"] = "1";

  ezdox::RunOptions ro;
  ro.dry_run = true;
  ro.environment["A"] = "B";
  CHECK(ezdox::run_named(c, "hello", ro) == 0);
  CHECK(ezdox::run_named(c, "echo_ok", ro) == 0);
  CHECK(ezdox::run_named(c, "both", ro) == 0);

  ro.dry_run = false;
  ro.timeout_seconds = 2;
  CHECK(ezdox::run_named(c, "hello", ro) == 0);
}

// ─── Test 14: Copy install modes ───
static void test_copy_install_modes(const std::filesystem::path &tmp) {
  auto src = tmp / "install_src";
  std::filesystem::create_directories(src);
  write(src / "data.txt", "hello");

  auto dest = tmp / "install_dest";
  ezdox::copy_install(src, dest, false, "copy");
  CHECK(std::filesystem::exists(dest / "data.txt"));

  auto dest2 = tmp / "install_symlink";
  ezdox::copy_install(src, dest2, false, "symlink");
  CHECK(std::filesystem::exists(dest2));
  CHECK(std::filesystem::is_symlink(dest2));

  bool threw = false;
  try { ezdox::copy_install(tmp / "nonexistent", tmp / "out", false); }
  catch (const std::exception &) { threw = true; }
  CHECK(threw);
}

// ─── Test 15: Scan sources with multiple roots ───
static void test_scan_sources_multiple_roots(const std::filesystem::path &tmp) {
  auto root1 = tmp / "multi_root1";
  auto root2 = tmp / "multi_root2";
  std::filesystem::create_directories(root1);
  std::filesystem::create_directories(root2);

  write(root1 / "a.cpp", "/// @brief Alpha\nint a();\n");
  write(root2 / "b.cpp", "/// @brief Beta\nint b();\n");

  auto spell = ezdox::command_spellings(ezdox::load_doxygen_commands());
  auto items = ezdox::scan_sources({root1, root2}, {}, spell);
  CHECK(items.size() == 2);

  bool has_alpha = false, has_beta = false;
  for (auto &it : items) {
    if (it.brief == "Alpha") has_alpha = true;
    if (it.brief == "Beta") has_beta = true;
  }
  CHECK(has_alpha);
  CHECK(has_beta);
}
