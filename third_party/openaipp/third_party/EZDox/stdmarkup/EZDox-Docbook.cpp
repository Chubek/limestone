#include "EzDox-Markup.hpp"
#include "EzDox-Internal.hpp"
#include <sstream>

namespace ezdox {

std::string markup_docbook(const DocumentModel &model) {
  std::ostringstream out;
  out << "<article><title>" << internal::xml_escape(model.config.project) << "</title>";
  for (auto &it : model.items) {
    auto title = it.symbol.empty() ? it.file.generic_string() : it.symbol;
    out << "<section xml:id=\"" << internal::xml_escape(internal::anchor_id(title))
        << "\"><title>" << internal::xml_escape(title)
        << "</title><para>" << internal::xml_escape(it.brief.empty() ? it.text : it.brief)
        << "</para>";
    for (auto &[k,v] : it.params)
      out << "<parameter name=\"" << internal::xml_escape(k) << "\">"
          << internal::xml_escape(v) << "</parameter>";
    if (!it.returns.empty())
      out << "<returns>" << internal::xml_escape(it.returns) << "</returns>";
    for (auto &r : it.references)
      out << "<xref linkend=\"" << internal::xml_escape(internal::anchor_id(r)) << "\"/>";
    out << "</section>";
  }
  out << "</article>\n";
  return out.str();
}

} // namespace ezdox
