#include "EzDox-Target.hpp"
#include "EzDox-Internal.hpp"
#include "EzDox-Markup.hpp"
#include <fstream>
#include <sstream>

namespace ezdox {

void target_latex(const DocumentModel &model, const std::filesystem::path &output_dir,
                  const std::filesystem::path &template_dir) {
  namespace fs = std::filesystem;
  fs::create_directories(output_dir);

  if (!template_dir.empty() && fs::exists(template_dir / "latex" / "header.ltx")) {
    // Template-based LaTeX rendering
    auto header = internal::read_text(template_dir / "latex" / "header.ltx");
    auto footer = fs::exists(template_dir / "latex" / "footer.ltx")
                    ? internal::read_text(template_dir / "latex" / "footer.ltx")
                    : "";
    auto cls = fs::exists(template_dir / "latex" / "ezdox.cls")
                 ? internal::read_text(template_dir / "latex" / "ezdox.cls")
                 : "";

    std::ostringstream out;
    out << header << "\n";
    if (!cls.empty()) internal::write_text(output_dir / "ezdox.cls", cls);
    out << "\\section*{" << internal::latex_escape(model.config.project) << "}\n";
    for (auto &it : model.items) {
      out << "\\subsection*{"
          << internal::latex_escape(it.symbol.empty() ? it.file.generic_string() : it.symbol)
          << "}\n"
          << internal::latex_escape(it.brief.empty() ? it.text : it.brief) << "\n";
      if (!it.params.empty()) {
        out << "\\paragraph{Parameters}\n\\begin{description}\n";
        for (auto &[k,v] : it.params)
          out << "\\item[" << internal::latex_escape(k) << "] "
              << internal::latex_escape(v) << "\n";
        out << "\\end{description}\n";
      }
      if (!it.returns.empty())
        out << "\\paragraph{Returns} " << internal::latex_escape(it.returns) << "\n";
      if (!it.references.empty()) {
        out << "\\paragraph{References}\n\\begin{itemize}\n";
        for (auto &r : it.references)
          out << "\\item \\texttt{" << internal::latex_escape(r) << "}\n";
        out << "\\end{itemize}\n";
      }
    }
    out << footer << "\n";
    internal::write_text(output_dir / "manual.tex", out.str());
    return;
  }

  // Fallback: simple LaTeX rendering
  std::ostringstream out;
  out << "\\section*{" << internal::latex_escape(model.config.project) << "}\n";
  for (auto &it : model.items) {
    out << "\\subsection*{"
        << internal::latex_escape(it.symbol.empty() ? it.file.generic_string() : it.symbol)
        << "}\n"
        << internal::latex_escape(it.brief.empty() ? it.text : it.brief) << "\n";
    if (!it.params.empty()) {
      out << "\\paragraph{Parameters}\n\\begin{description}\n";
      for (auto &[k,v] : it.params)
        out << "\\item[" << internal::latex_escape(k) << "] "
            << internal::latex_escape(v) << "\n";
      out << "\\end{description}\n";
    }
    if (!it.returns.empty())
      out << "\\paragraph{Returns} " << internal::latex_escape(it.returns) << "\n";
  }
  internal::write_text(output_dir / "manual.tex", out.str());
}

} // namespace ezdox
