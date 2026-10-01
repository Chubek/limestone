#include "EzDox-Markup.hpp"
#include "EzDox-Internal.hpp"
#include <sstream>

namespace ezdox {

std::string markup_markdown(const DocumentModel &model) {
  std::ostringstream out;
  out << "# " << model.config.project << "\n\n";
  if (!model.config.version.empty()) out << "_Version " << model.config.version << "_\n\n";
  for (auto &it : model.items) {
    auto title = it.symbol.empty() ? it.file.generic_string() : it.symbol;
    out << "## " << title << "\n\n";
    out << "- Location: `" << it.file.generic_string() << ":" << it.line;
    if (it.end_line && it.end_line != it.line) out << "-" << it.end_line;
    out << "`\n";
    if (!it.kind.empty()) out << "- Kind: `" << it.kind << "`\n";
    if (!it.commands.empty()) {
      out << "- Commands:";
      for (auto &c : it.commands) out << " `" << c << "`";
      out << "\n";
    }
    if (!it.declaration.empty()) out << "- Declaration: `" << it.declaration << "`\n";
    out << "\n" << (it.brief.empty() ? it.text : it.brief) << "\n\n";
    if (!it.params.empty()) {
      out << "### Parameters\n\n";
      for (auto &[k,v] : it.params) out << "- `" << k << "`: " << v << "\n";
      out << "\n";
    }
    if (!it.returns.empty()) out << "### Returns\n\n" << it.returns << "\n\n";
    if (!it.references.empty()) {
      out << "### References\n\n";
      for (auto &r : it.references)
        out << "- [" << r << "](#" << internal::anchor_id(r) << ")\n";
      out << "\n";
    }
  }
  return out.str();
}

std::string resolve_markup(std::string_view name, const DocumentModel &model) {
  auto n = internal::lower(std::string(name));
  if (n == "markdown") return markup_markdown(model);
  if (n == "restructuredtext") return markup_restructuredtext(model);
  if (n == "docbook") return markup_docbook(model);
  if (n == "xwiki") return markup_xwiki(model);
  return markup_markdown(model);
}

} // namespace ezdox
