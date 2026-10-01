#include "EzDox-Target.hpp"
#include "EzDox-Internal.hpp"
#include <sstream>

namespace ezdox {

void target_roff(const DocumentModel &model, const std::filesystem::path &output_dir) {
  std::filesystem::create_directories(output_dir);
  std::ostringstream out;
  out << ".TH EZDOX 1\n.SH NAME\n" << model.config.project << "\n.SH DOCUMENTATION\n";
  for (auto &it : model.items) {
    out << ".SS " << (it.symbol.empty() ? it.file.generic_string() : it.symbol) << "\n"
        << (it.brief.empty() ? it.text : it.brief) << "\n";
    if (!it.params.empty()) {
      out << ".TP\nParameters\n";
      for (auto &[k,v] : it.params) out << k << " - " << v << "\n";
    }
    if (!it.returns.empty()) out << ".TP\nReturns\n" << it.returns << "\n";
  }
  internal::write_text(output_dir / "ezdox.1", out.str());
}

} // namespace ezdox
