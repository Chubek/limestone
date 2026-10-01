#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "Satie.h"
#include "SatieModule.h"
#include "SatiePlugin.h"

namespace satie::stdlib::ffi
{

/// RAII guards for the Satie C handles. Lua has no FFI surface (the dialect
/// cannot hold opaque pointers); use these from C++ embedding code.
struct SolverDeleter
{
  void operator() (SatieSolver *solver) const noexcept { satie_solver_destroy (solver); }
};

struct ModuleDeleter
{
  void operator() (SatieCModule *module) const noexcept { satie_module_destroy (module); }
};

struct PluginDeleter
{
  void operator() (SatieCPluginContext *context) const noexcept
  {
    satie_plugin_context_destroy (context);
  }
};

using SolverHandle = std::unique_ptr<SatieSolver, SolverDeleter>;
using ModuleHandle = std::unique_ptr<SatieCModule, ModuleDeleter>;
using PluginHandle = std::unique_ptr<SatieCPluginContext, PluginDeleter>;

inline SolverHandle make_solver ()
{
  SolverHandle handle (satie_solver_create ());
  if (handle == nullptr)
    throw std::runtime_error ("ffi::make_solver failed");
  return handle;
}

inline ModuleHandle make_module (const std::string &theory)
{
  ModuleHandle handle (satie_module_create (theory.c_str ()));
  if (handle == nullptr)
    throw std::runtime_error ("ffi::make_module failed for '" + theory + "'");
  return handle;
}

inline PluginHandle make_plugin_context ()
{
  PluginHandle handle (satie_plugin_context_create ());
  if (handle == nullptr)
    throw std::runtime_error ("ffi::make_plugin_context failed");
  return handle;
}

} // namespace satie::stdlib::ffi
