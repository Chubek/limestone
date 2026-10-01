#pragma once

#include "SatiePlugin.hpp"

#include <array>
#include <cctype>
#include <map>
#include <tuple>

namespace satie::stdplugin
{
class PackageResolver final : public Plugin
{
  struct Version
  {
    struct Identifier
    {
      std::string text;
      bool numeric = false;

      friend bool operator< (const Identifier &left, const Identifier &right)
      {
        if (left.numeric && right.numeric)
          return left.text.size () == right.text.size () ? left.text < right.text
                                                         : left.text.size () < right.text.size ();
        if (left.numeric != right.numeric)
          return left.numeric;
        return left.text < right.text;
      }
    };

    std::uint64_t major = 0;
    std::uint64_t minor = 0;
    std::uint64_t patch = 0;
    std::optional<std::vector<Identifier>> prerelease;

    friend bool operator< (const Version &left, const Version &right)
    {
      if (const auto core = std::tie (left.major, left.minor, left.patch) <=>
                             std::tie (right.major, right.minor, right.patch);
          core != 0)
        return core < 0;
      if (!left.prerelease)
        return false;
      if (!right.prerelease)
        return true;
      const auto &left_identifiers = *left.prerelease;
      const auto &right_identifiers = *right.prerelease;
      const std::size_t count = std::min (left_identifiers.size (), right_identifiers.size ());
      for (std::size_t index = 0; index < count; ++index)
        {
          if (left_identifiers[index] < right_identifiers[index])
            return true;
          if (right_identifiers[index] < left_identifiers[index])
            return false;
        }
      return left_identifiers.size () < right_identifiers.size ();
    }
  };

  static Version parse_version (std::string_view text)
  {
    const auto metadata_start = text.find ('+');
    const std::string_view without_metadata = text.substr (0, metadata_start);
    const std::string_view metadata =
        metadata_start == std::string_view::npos ? std::string_view{}
                                                 : text.substr (metadata_start + 1);
    const auto validate_identifiers = [] (std::string_view identifiers,
                                          bool reject_numeric_leading_zero) {
      if (identifiers.empty ())
        throw std::invalid_argument ("package.resolve version is not valid SemVer");
      std::size_t identifier_start = 0;
      while (identifier_start <= identifiers.size ())
        {
          const std::size_t identifier_end = identifiers.find ('.', identifier_start);
          const std::string_view identifier =
              identifiers.substr (identifier_start, identifier_end - identifier_start);
          if (identifier.empty ())
            throw std::invalid_argument ("package.resolve version is not valid SemVer");
          bool numeric = true;
          for (const char character : identifier)
            {
              if (!std::isalnum (static_cast<unsigned char> (character)) && character != '-')
                throw std::invalid_argument ("package.resolve version is not valid SemVer");
              numeric = numeric && std::isdigit (static_cast<unsigned char> (character));
            }
          if (reject_numeric_leading_zero && numeric && identifier.size () > 1 &&
              identifier.front () == '0')
            throw std::invalid_argument ("package.resolve version is not valid SemVer");
          if (identifier_end == std::string_view::npos)
            break;
          identifier_start = identifier_end + 1;
        }
    };
    if (metadata_start != std::string_view::npos)
      validate_identifiers (metadata, false);
    const auto prerelease_start = without_metadata.find ('-');
    const std::string_view core = without_metadata.substr (0, prerelease_start);
    Version result;
    std::array<std::uint64_t *, 3> parts{&result.major, &result.minor, &result.patch};
    std::size_t start = 0;
    for (std::size_t index = 0; index < parts.size (); ++index)
      {
        const std::size_t end = core.find ('.', start);
        const std::string_view part = core.substr (start, end - start);
        if (part.empty () || (part.size () > 1 && part.front () == '0'))
          throw std::invalid_argument ("package.resolve version is not valid SemVer");
        const auto [parsed, error] =
            std::from_chars (part.data (), part.data () + part.size (), *parts[index]);
        if (error != std::errc{} || parsed != part.data () + part.size ())
          throw std::invalid_argument ("package.resolve version is not valid SemVer");
        if (index + 1 == parts.size ())
          {
            if (end != std::string_view::npos)
              throw std::invalid_argument ("package.resolve version is not valid SemVer");
          }
        else if (end == std::string_view::npos)
          throw std::invalid_argument ("package.resolve version is not valid SemVer");
        start = end + 1;
      }
    if (prerelease_start != std::string_view::npos)
      {
        const std::string_view prerelease = without_metadata.substr (prerelease_start + 1);
        validate_identifiers (prerelease, true);
        std::vector<Version::Identifier> identifiers;
        std::size_t identifier_start = 0;
        while (identifier_start <= prerelease.size ())
          {
            const std::size_t identifier_end = prerelease.find ('.', identifier_start);
            const std::string_view identifier =
                prerelease.substr (identifier_start, identifier_end - identifier_start);
            bool numeric = true;
            for (const char character : identifier)
              {
                numeric = numeric && std::isdigit (static_cast<unsigned char> (character));
              }
            identifiers.push_back ({std::string (identifier), numeric});
            if (identifier_end == std::string_view::npos)
              break;
            identifier_start = identifier_end + 1;
          }
        result.prerelease = std::move (identifiers);
      }
    return result;
  }

public:
  PluginInfo info () const override
  {
    return {"package-resolver", "1.0.0", "Resolve package names to the highest requested version."};
  }

  void install (PluginHost &host) override
  {
    host.register_command ("package.resolve", [] (const PluginArguments &args) {
      if (args.empty ())
        throw std::invalid_argument ("package.resolve expects package=version entries");
      struct SelectedVersion
      {
        Version parsed;
        std::string text;
      };
      std::map<std::string, SelectedVersion> versions;
      for (const auto &entry : args)
        {
          const auto separator = entry.find ('=');
          if (separator == std::string::npos || separator == 0 ||
              separator + 1 == entry.size ())
            throw std::invalid_argument (
                "package.resolve entries must use package=semantic-version");
          const auto name = entry.substr (0, separator);
          const auto version = entry.substr (separator + 1);
          const Version parsed = parse_version (version);
          auto it = versions.find (name);
          if (it == versions.end () || it->second.parsed < parsed)
            versions[name] = {parsed, version};
        }
      std::ostringstream result;
      bool first = true;
      for (const auto &[name, version] : versions)
        {
          if (!first)
            result << ',';
          first = false;
          result << name << '=' << version.text;
        }
      return result.str ();
    });
  }
};
} // namespace satie::stdplugin
