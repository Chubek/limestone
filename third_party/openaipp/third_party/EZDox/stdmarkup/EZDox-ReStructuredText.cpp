#include "EzDox-Markup.hpp"
#include "EzDox-Internal.hpp"
#include <sstream>

namespace ezdox {

std::string markup_restructuredtext(const DocumentModel &model) {
  std::ostringstream out;
  out << model.config.project << "\n"
      << std::string(model.config.project.size(), '=') << "\n\n";
  for (auto &it : model.items) {
    auto title = it.symbol.empty() ? it.file.generic_string() : it.symbol;
    out << title << "\n" << std::string(title.size(), '-') << "\n\n"
        << (it.brief.empty() ? it.text : it.brief) << "\n\n";
    if (!it.params.empty()) {
      out << "Parameters\n~~~~~~~~~~\n\n";
      for (auto &[k,v] : it.params) out << "* ``" << k << "`` -- " << v << "\n";
      out << "\n";
    }
    if (!it.returns.empty()) out << "Returns\n~~~~~~~\n\n" << it.returns << "\n\n";
  }
  return out.str();
}

} // namespace ezdox
