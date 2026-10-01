#include "EzDox-Markup.hpp"
#include "EzDox-Internal.hpp"
#include <sstream>

namespace ezdox {

std::string markup_xwiki(const DocumentModel &model) {
  std::ostringstream out;
  out << "= " << model.config.project << " =\n\n";
  for (auto &it : model.items) {
    auto title = it.symbol.empty() ? it.file.generic_string() : it.symbol;
    out << "== " << title << " ==\n\n"
        << (it.brief.empty() ? it.text : it.brief) << "\n\n";
  }
  return out.str();
}

} // namespace ezdox
