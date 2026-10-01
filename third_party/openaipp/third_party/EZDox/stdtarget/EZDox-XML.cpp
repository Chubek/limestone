#include "EzDox-Target.hpp"
#include "EzDox-Internal.hpp"
#include <sstream>

namespace ezdox {

void target_xml(const DocumentModel &model, const std::filesystem::path &output_dir) {
  std::filesystem::create_directories(output_dir);
  std::ostringstream out;
  out << "<ezdox><project name=\"" << internal::xml_escape(model.config.project)
      << "\" version=\"" << internal::xml_escape(model.config.version) << "\">";
  for (auto &it : model.items) {
    out << "<item file=\"" << internal::xml_escape(it.file.generic_string())
        << "\" line=\"" << it.line << "\" kind=\"" << internal::xml_escape(it.kind)
        << "\" symbol=\"" << internal::xml_escape(it.symbol) << "\">";
    out << "<declaration>" << internal::xml_escape(it.declaration) << "</declaration>";
    out << "<brief>" << internal::xml_escape(it.brief) << "</brief>";
    out << "<details>" << internal::xml_escape(it.details) << "</details>";
    for (auto &[k,v] : it.params)
      out << "<param name=\"" << internal::xml_escape(k) << "\">"
          << internal::xml_escape(v) << "</param>";
    if (!it.returns.empty())
      out << "<returns>" << internal::xml_escape(it.returns) << "</returns>";
    for (auto &r : it.references)
      out << "<reference target=\"" << internal::xml_escape(internal::anchor_id(r))
          << "\">" << internal::xml_escape(r) << "</reference>";
    for (auto &c : it.commands)
      out << "<command>" << internal::xml_escape(c) << "</command>";
    out << "</item>";
  }
  out << "</project></ezdox>\n";
  internal::write_text(output_dir / "index.xml", out.str());
}

} // namespace ezdox
