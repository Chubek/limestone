#include "EzDox-Target.hpp"
#include "EzDox-Internal.hpp"
#include "EzDox-Markup.hpp"
#include <fstream>
#include <sstream>

#ifdef EZDOX_USE_INJA
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>
#endif

namespace ezdox {

void target_html(const DocumentModel &model, const std::filesystem::path &output_dir,
                 const std::filesystem::path &template_dir) {
  namespace fs = std::filesystem;
  fs::create_directories(output_dir);

#ifdef EZDOX_USE_INJA
  if (!template_dir.empty() && fs::exists(template_dir / "html" / "index.html")) {
    // Template-based HTML rendering
    inja::Environment env(template_dir / "html");
    auto tpl = env.parse_template("index.html");

    nlohmann::json data;
    data["project"] = model.config.project;
    data["version"] = model.config.version;
    auto items_arr = nlohmann::json::array();
    for (auto &it : model.items) {
      nlohmann::json j;
      j["file"] = it.file.generic_string();
      j["line"] = it.line;
      j["end_line"] = it.end_line;
      j["kind"] = it.kind;
      j["symbol"] = it.symbol;
      j["title"] = it.symbol.empty() ? (it.file.filename().string() + ":" + std::to_string(it.line)) : it.symbol;
      j["anchor"] = internal::anchor_id(j["title"].get<std::string>());
      j["declaration"] = it.declaration;
      j["text"] = it.text;
      j["brief"] = it.brief;
      j["summary"] = it.brief.empty() ? it.text : it.brief;
      j["details"] = it.details;
      j["returns"] = it.returns;
      auto params_obj = nlohmann::json::object();
      for (auto &[k,v] : it.params) params_obj[k] = v;
      j["params"] = params_obj;
      auto refs_arr = nlohmann::json::array();
      for (auto &r : it.references) refs_arr.push_back(r);
      j["references"] = refs_arr;
      auto cmds_arr = nlohmann::json::array();
      for (auto &c : it.commands) cmds_arr.push_back(c);
      j["commands"] = cmds_arr;
      items_arr.push_back(j);
    }
    data["items"] = items_arr;

    auto result = env.render(tpl, data);
    internal::write_text(output_dir / "index.html", result);

    // Copy static assets
    auto css_dir = template_dir / "html" / "csslib";
    auto js_dir = template_dir / "html" / "jslib";
    if (fs::exists(css_dir)) {
      fs::create_directories(output_dir / "csslib");
      for (auto &e : fs::directory_iterator(css_dir))
        fs::copy(e.path(), output_dir / "csslib" / e.path().filename(), fs::copy_options::overwrite_existing);
    }
    if (fs::exists(js_dir)) {
      fs::create_directories(output_dir / "jslib");
      for (auto &e : fs::directory_iterator(js_dir))
        fs::copy(e.path(), output_dir / "jslib" / e.path().filename(), fs::copy_options::overwrite_existing);
    }
    return;
  }
#endif

  // Fallback: simple HTML rendering
  std::ostringstream out;
  out << "<!doctype html><html><head><meta charset=\"utf-8\"><title>"
      << internal::xml_escape(model.config.project) << "</title></head><body>\n";
  out << "<h1>" << internal::xml_escape(model.config.project) << "</h1>\n";
  for (auto &it : model.items) {
    auto title = it.symbol.empty() ? it.file.generic_string() : it.symbol;
    out << "<section id=\"" << internal::xml_escape(internal::anchor_id(title))
        << "\" data-file=\"" << internal::xml_escape(it.file.generic_string())
        << "\" data-line=\"" << it.line << "\"><h2>"
        << internal::xml_escape(title) << "</h2><p><code>"
        << internal::xml_escape(it.kind) << "</code></p><pre>"
        << internal::xml_escape(it.declaration) << "</pre><p>"
        << internal::xml_escape(it.brief.empty() ? it.text : it.brief) << "</p>";
    if (!it.params.empty()) {
      out << "<h3>Parameters</h3><dl>";
      for (auto &[k,v] : it.params)
        out << "<dt><code>" << internal::xml_escape(k) << "</code></dt><dd>"
            << internal::xml_escape(v) << "</dd>";
      out << "</dl>";
    }
    if (!it.returns.empty())
      out << "<h3>Returns</h3><p>" << internal::xml_escape(it.returns) << "</p>";
    if (!it.references.empty()) {
      out << "<h3>References</h3><ul>";
      for (auto &r : it.references)
        out << "<li><a href=\"#" << internal::xml_escape(internal::anchor_id(r))
            << "\"><code>" << internal::xml_escape(r) << "</code></a></li>";
      out << "</ul>";
    }
    out << "</section>\n";
  }
  out << "</body></html>\n";
  internal::write_text(output_dir / "index.html", out.str());
}

} // namespace ezdox
