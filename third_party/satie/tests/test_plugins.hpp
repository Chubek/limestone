#pragma once

#include "../include/SatiePlugin.hpp"
#include "../stdplugin/DependencySAT/DependencySAT.hpp"
#include "../stdplugin/PackageResolver/PackageResolver.hpp"
#include "../stdplugin/SemVerValidator/SemVerValidator.hpp"

using namespace satie;

TEST_CASE ("plugin host rejects duplicate commands")
{
  PluginHost host;
  host.register_command ("answer", [] (const PluginArguments &) { return std::string ("42"); });
  REQUIRE(host.invoke ("answer") == "42");
  REQUIRE_THROWS_AS (
      host.register_command ("answer", [] (const PluginArguments &) { return std::string ("0"); }),
      std::invalid_argument);
}

TEST_CASE ("standard plugins validate and resolve semantic inputs")
{
  PluginHost host;
  PluginManager manager (host);
  manager.emplace<stdplugin::SemVerValidator> ();
  manager.emplace<stdplugin::DependencySAT> ();
  manager.emplace<stdplugin::PackageResolver> ();

  REQUIRE(host.invoke ("semver.valid", {"1.2.3-alpha.1+build.4"}) == "true");
  REQUIRE(host.invoke ("semver.valid", {"1.02.3"}) == "false");
  REQUIRE(host.invoke ("semver.valid", {"1.2.3-01"}) == "false");
  REQUIRE(host.invoke ("dependency.sat", {"1 -2", "2"}) == "SAT");
  REQUIRE(host.invoke ("dependency.sat", {"1", "-1"}) == "UNSAT");
  REQUIRE_THROWS_AS (host.invoke ("dependency.sat", {"1 0"}), std::invalid_argument);
  REQUIRE(host.invoke ("package.resolve",
                       {"core=1.9.0", "core=1.10.0", "ui=2.0.0-alpha.2",
                        "ui=2.0.0-alpha.10"}) == "core=1.10.0,ui=2.0.0-alpha.10");
  REQUIRE_THROWS_AS (host.invoke ("package.resolve", {"core=1.2"}), std::invalid_argument);
  REQUIRE_THROWS_AS (host.invoke ("package.resolve", {"core=1.2.3+?"}), std::invalid_argument);
}

TEST_CASE ("Lua lsatie library exposes solver and host commands")
{
  PluginHost host;
  qamrpp::Context context;
  LuaPlugin::install_library (context, host);
  const auto result = context.run (
      "local solved = lsatie.solve('(x1) & (~x2)', 'cdcl'); "
      "lsatie.register('lua.echo', function(args) return args[1] end); "
      "return solved.status");

  REQUIRE(result->type == qamrpp::Value::STRING);
  REQUIRE(result->string_value == "SAT");
  REQUIRE(host.invoke ("lua.echo", {"registered"}) == "registered");
  REQUIRE_NOTHROW (context.run ("return lsatie.satisfiable('(x1) & (~x1)')"));
  const auto unsat = context.run ("return lsatie.satisfiable('(x1) & (~x1)')");
  REQUIRE(unsat->type == qamrpp::Value::BOOL);
  REQUIRE_FALSE(unsat->bool_value);
}
