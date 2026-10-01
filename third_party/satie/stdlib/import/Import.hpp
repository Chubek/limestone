#pragma once

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "SatiePlugin.hpp"

namespace satie::stdlib::import
{

/// Runs a Lua file in `context` and returns its chunk value (module tables
/// conventionally). Throws on I/O or script errors. This is the file-loading
/// half that the QaMRpp dialect itself cannot express (`dofile`/`require`
/// are stubs there); Lua callers receive already-loaded tables instead.
inline qamrpp::ValuePtr load_lua_file (qamrpp::Context &context, const std::string &path)
{
  std::ifstream in (path);
  if (!in)
    throw std::runtime_error ("import::load_lua_file cannot open '" + path + "'");
  std::ostringstream buffer;
  buffer << in.rdbuf ();
  return context.run (buffer.str ());
}

/// Runs a Lua snippet and returns its chunk value.
inline qamrpp::ValuePtr run_lua_snippet (qamrpp::Context &context, const std::string &code)
{
  return context.run (code);
}

} // namespace satie::stdlib::import
