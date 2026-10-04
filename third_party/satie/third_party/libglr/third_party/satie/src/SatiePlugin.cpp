// C plugin context (SatiePlugin.h).
//
// NOTE: this translation unit intentionally does NOT include the C++
// SatiePlugin.hpp header: that header pulls in the vendored QaMRpp header,
// which defines its C bridge functions at header scope. Including it here
// would emit duplicate definitions when the static library and a test
// binary are linked together. The C context below is therefore a small
// self-contained registry with the same semantics (duplicate registration
// rejected, unknown command is an error).

#include "SatiePlugin.h"

#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace
{
thread_local std::string g_plugin_error;

void set_plugin_error (const std::string &message) { g_plugin_error = message; }
} // namespace

struct SatieCPluginContext
{
  std::mutex mutex;
  std::map<std::string, std::pair<SatieCCommandFn, void *>> commands;
};

extern "C"
{

SatieCPluginContext *satie_plugin_context_create (void)
{
  try
    {
      return new SatieCPluginContext{};
    }
  catch (const std::exception &e)
    {
      set_plugin_error (e.what ());
      return nullptr;
    }
}

void satie_plugin_context_destroy (SatieCPluginContext *context) { delete context; }

int satie_plugin_register (SatieCPluginContext *context, const char *name,
                           SatieCCommandFn function, void *userdata)
{
  if (context == nullptr || name == nullptr || name[0] == '\0' || function == nullptr)
    {
      set_plugin_error ("plugin register requires a context, name, and function");
      return -1;
    }
  try
    {
      std::lock_guard lock (context->mutex);
      const auto [_, inserted] =
          context->commands.emplace (name, std::make_pair (function, userdata));
      if (!inserted)
        {
          set_plugin_error ("Satie plugin command is already registered");
          return -1;
        }
      return 0;
    }
  catch (const std::exception &e)
    {
      set_plugin_error (e.what ());
      return -1;
    }
}

int satie_plugin_has (const SatieCPluginContext *context, const char *name)
{
  if (context == nullptr || name == nullptr)
    return 0;
  SatieCPluginContext &mutable_context = const_cast<SatieCPluginContext &> (*context);
  std::lock_guard lock (mutable_context.mutex);
  return mutable_context.commands.find (name) != mutable_context.commands.end () ? 1 : 0;
}

int satie_plugin_invoke (const SatieCPluginContext *context, const char *name,
                         const char *const *args, size_t count, char *out,
                         size_t out_size)
{
  if (context == nullptr || name == nullptr || out == nullptr || out_size == 0)
    {
      set_plugin_error ("plugin invoke requires a context, name, and output buffer");
      return -1;
    }
  SatieCCommandFn function = nullptr;
  void *userdata = nullptr;
  {
    SatieCPluginContext &mutable_context = const_cast<SatieCPluginContext &> (*context);
    std::lock_guard lock (mutable_context.mutex);
    auto it = mutable_context.commands.find (name);
    if (it == mutable_context.commands.end ())
      {
        set_plugin_error (std::string ("unknown Satie plugin command: ") + name);
        return -1;
      }
    function = it->second.first;
    userdata = it->second.second;
  }
  std::vector<const char *> c_args;
  c_args.reserve (count);
  std::vector<std::string> owned;
  owned.reserve (count);
  if (args != nullptr)
    for (size_t i = 0; i < count; ++i)
      owned.emplace_back (args[i] != nullptr ? args[i] : "");
  c_args.clear ();
  for (const std::string &arg : owned)
    c_args.push_back (arg.c_str ());
  const char *result = function (c_args.data (), count, userdata);
  if (result == nullptr)
    {
      set_plugin_error ("plugin command failed");
      return -1;
    }
  const size_t length = std::strlen (result);
  if (length + 1 > out_size)
    {
      set_plugin_error ("plugin result truncated: output buffer too small");
      return -1;
    }
  std::memcpy (out, result, length + 1);
  return 0;
}

const char *satie_plugin_last_error (void) { return g_plugin_error.c_str (); }

} // extern "C"
