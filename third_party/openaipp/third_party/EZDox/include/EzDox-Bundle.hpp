#pragma once
#include "EzDox.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace ezdox {

/// Bundle management module.
/// Bundles are distributable packages of markup parsers, target renderers,
/// and templates that can be installed into $EZDOX_HOME.

/// Build a bundle archive from a source directory.
void build_bundle(const std::filesystem::path &source,
                  const std::filesystem::path &output,
                  const std::string &name,
                  const std::string &version,
                  const std::string &description = "");

/// Install a bundle archive into $EZDOX_HOME.
void install_bundle(const std::filesystem::path &bundle,
                    const std::filesystem::path &home = {},
                    bool force = false);

/// List installed bundles.
std::vector<std::filesystem::path> list_bundles(const std::filesystem::path &home = {});

/// Remove a bundle by name.
void remove_bundle(const std::string &name,
                   const std::filesystem::path &home = {});

/// Inspect a bundle archive and return its contents.
std::vector<std::string> inspect_bundle(const std::filesystem::path &bundle);

} // namespace ezdox
