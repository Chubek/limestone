#pragma once

#include "SatiePlugin.hpp"

#include <regex>

namespace satie::stdplugin
{
class SemVerValidator final : public Plugin
{
public:
  PluginInfo info () const override
  {
    return {"semver-validator", "1.0.0", "Validate Semantic Version strings."};
  }

  void install (PluginHost &host) override
  {
    host.register_command ("semver.valid", [] (const PluginArguments &args) {
      if (args.empty ())
        return std::string ("false");
      static const std::regex pattern (
          R"(^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-((0|[1-9][0-9]*|[0-9A-Za-z-]*[A-Za-z-][0-9A-Za-z-]*)\.)*(0|[1-9][0-9]*|[0-9A-Za-z-]*[A-Za-z-][0-9A-Za-z-]*))?(\+[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?$)");
      return std::regex_match (args.front (), pattern) ? std::string ("true") : std::string ("false");
    });
  }
};
} // namespace satie::stdplugin
