#pragma once
#include "EzDox.hpp"
#include <filesystem>
#include <string>
#include <string_view>

namespace ezdox {

/// Target module: renders a DocumentModel to a specific output format
/// and writes the result to the filesystem.

/// Render HTML documentation.
void target_html(const DocumentModel &model, const std::filesystem::path &output_dir,
                 const std::filesystem::path &template_dir = {});

/// Render LaTeX documentation.
void target_latex(const DocumentModel &model, const std::filesystem::path &output_dir,
                  const std::filesystem::path &template_dir = {});

/// Render Manpage / ROFF documentation.
void target_manpage(const DocumentModel &model, const std::filesystem::path &output_dir);

/// Render ROFF documentation (alias for manpage).
void target_roff(const DocumentModel &model, const std::filesystem::path &output_dir);

/// Render XML documentation.
void target_xml(const DocumentModel &model, const std::filesystem::path &output_dir);

/// Resolve a target name and render the model. Falls back to plain text
/// if the name is unrecognized.
void resolve_target(std::string_view name, const DocumentModel &model,
                    const std::filesystem::path &output_dir,
                    const std::filesystem::path &template_dir = {});

} // namespace ezdox
