#pragma once
#include "EzDox.hpp"
#include <string>
#include <string_view>

namespace ezdox {

/// Markup module: converts a DocumentModel into a formatted string.
/// Each module function takes a DocumentModel and returns a string
/// in the target markup format.

/// Generate Markdown-formatted documentation.
std::string markup_markdown(const DocumentModel &model);

/// Generate ReStructuredText-formatted documentation.
std::string markup_restructuredtext(const DocumentModel &model);

/// Generate Docbook XML-formatted documentation.
std::string markup_docbook(const DocumentModel &model);

/// Generate XWiki-formatted documentation.
std::string markup_xwiki(const DocumentModel &model);

/// Resolve a markup name to its output string. Falls back to Markdown
/// if the name is unrecognized.
std::string resolve_markup(std::string_view name, const DocumentModel &model);

} // namespace ezdox
