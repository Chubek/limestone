#pragma once

#include "Satie.hpp"

#include "../third_party/QaMRpp/include/QaMRpp.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace satie
{

using PluginArguments = std::vector<std::string>;
using PluginCommand = std::function<std::string(const PluginArguments &)>;

struct PluginInfo
{
  std::string name;
  std::string version = "1.0.0";
  std::string description;
};

class PluginHost
{
public:
  void register_command (std::string name, PluginCommand command)
  {
    if (name.empty () || !command)
      throw std::invalid_argument ("plugin commands require a name and callback");
    std::lock_guard lock (mutex_);
    const auto [_, inserted] = commands_.emplace (std::move (name), std::move (command));
    if (!inserted)
      throw std::invalid_argument ("Satie plugin command is already registered");
  }

  bool has_command (std::string_view name) const
  {
    std::lock_guard lock (mutex_);
    return commands_.find (std::string (name)) != commands_.end ();
  }

  std::string invoke (std::string_view name, const PluginArguments &arguments = {}) const
  {
    PluginCommand command;
    {
      std::lock_guard lock (mutex_);
      auto it = commands_.find (std::string (name));
      if (it == commands_.end ())
        throw std::out_of_range ("unknown Satie plugin command: " + std::string (name));
      command = it->second;
    }
    return command (arguments);
  }

  std::vector<std::string> commands () const
  {
    std::lock_guard lock (mutex_);
    std::vector<std::string> result;
    result.reserve (commands_.size ());
    for (const auto &[name, _] : commands_)
      result.push_back (name);
    return result;
  }

private:
  mutable std::mutex mutex_;
  std::map<std::string, PluginCommand> commands_;
};

class Plugin
{
public:
  virtual ~Plugin () = default;
  virtual PluginInfo info () const = 0;
  virtual void install (PluginHost &host) = 0;
};

class PluginManager
{
public:
  explicit PluginManager (PluginHost &host) : host_ (host) {}

  void install (std::unique_ptr<Plugin> plugin)
  {
    if (!plugin)
      throw std::invalid_argument ("cannot install a null Satie plugin");
    plugin->install (host_);
    plugins_.push_back (std::move (plugin));
  }

  template <typename T, typename... Args>
  T &emplace (Args &&...args)
  {
    auto plugin = std::make_unique<T> (std::forward<Args> (args)...);
    T &result = *plugin;
    install (std::move (plugin));
    return result;
  }

  const std::vector<std::unique_ptr<Plugin>> &plugins () const { return plugins_; }

private:
  PluginHost &host_;
  std::vector<std::unique_ptr<Plugin>> plugins_;
};

namespace detail
{
inline std::string value_to_string (const qamrpp::ValuePtr &value)
{
  return value ? value->to_string () : "nil";
}

inline qamrpp::ValuePtr string_value (std::string value)
{
  return std::make_shared<qamrpp::Value> (std::move (value));
}

inline qamrpp::ValuePtr bool_value (bool value)
{
  return std::make_shared<qamrpp::Value> (value);
}

inline qamrpp::ValuePtr integer_value (std::int64_t value)
{
  return std::make_shared<qamrpp::Value> (value);
}

inline qamrpp::ValuePtr table_value ()
{
  return qamrpp::Value::make_table ();
}

inline qamrpp::ValuePtr native_value (qamrpp::NativeFn function)
{
  return std::make_shared<qamrpp::Value> (std::move (function));
}

inline void table_set (const qamrpp::ValuePtr &table, std::string key,
                       qamrpp::ValuePtr value)
{
  table->table_entries.emplace_back (string_value (std::move (key)), std::move (value));
}

inline qamrpp::ValuePtr arguments_table (const PluginArguments &arguments)
{
  auto table = qamrpp::Value::make_table ();
  for (std::size_t i = 0; i < arguments.size (); ++i)
    table->table_entries.emplace_back (
        std::make_shared<qamrpp::Value> (static_cast<std::int64_t> (i + 1)),
        string_value (arguments[i]));
  return table;
}

inline PluginArguments table_arguments (const qamrpp::ValuePtr &value)
{
  if (!value || value->type != qamrpp::Value::TABLE)
    throw std::invalid_argument ("plugin callback expects an argument table");
  PluginArguments result;
  for (std::size_t i = 1;; ++i)
    {
      auto key = std::make_shared<qamrpp::Value> (static_cast<std::int64_t> (i));
      auto item = std::find_if (
          value->table_entries.begin (), value->table_entries.end (),
          [&key] (const auto &entry) {
            return entry.first->type == qamrpp::Value::INT &&
                   entry.first->int_value == key->int_value;
          });
      if (item == value->table_entries.end ())
        break;
      result.push_back (item->second ? item->second->to_string () : "nil");
    }
  return result;
}

inline std::string require_string (const qamrpp::ValuePtr &value,
                                   std::string_view argument_name)
{
  if (!value || value->type != qamrpp::Value::STRING)
    throw std::invalid_argument (std::string (argument_name) + " must be a string");
  return value->string_value;
}

inline Engine parse_engine (const qamrpp::ValuePtr &value)
{
  if (!value || value->type == qamrpp::Value::NIL)
    return Engine::CDCL;
  const std::string engine = require_string (value, "engine");
  if (engine == "native")
    return Engine::Native;
  if (engine == "dpll")
    return Engine::DPLL;
  if (engine == "cdcl")
    return Engine::CDCL;
  throw std::invalid_argument ("engine must be native, dpll, or cdcl");
}

inline qamrpp::ValuePtr solve_result_value (const SolverReport &report)
{
  auto result = table_value ();
  table_set (result, "status",
             string_value (report.result.satisfiable () ? "SAT" : "UNSAT"));
  table_set (result, "satisfiable", bool_value (report.result.satisfiable ()));
  table_set (result, "engine", string_value (std::string (engine_name (report.engine))));
  auto model = table_value ();
  for (std::size_t variable = 1; variable <= report.result.assignment.size (); ++variable)
    {
      const Value value = report.result.assignment.get_var (static_cast<Var> (variable));
      if (value == Value::UNKNOWN)
        continue;
      table_set (model, std::to_string (variable), bool_value (value == Value::TRUE));
    }
  table_set (result, "model", model);
  return result;
}
} // namespace detail

class LuaPlugin
{
public:
  static void install_library (qamrpp::Context &context, PluginHost &host)
  {
    install_library (context, host, {});
  }

  static void install_library (qamrpp::Context &context, PluginHost &host,
                               std::shared_ptr<qamrpp::Context> owner)
  {
    auto library = qamrpp::Value::make_table ();
    library->table_entries.emplace_back (
        detail::string_value ("solve"),
        detail::native_value (qamrpp::NativeFn (
            [] (qamrpp::Context &, std::vector<qamrpp::ValuePtr> &args) {
              if (args.empty () || args.size () > 2)
                throw std::invalid_argument ("lsatie.solve expects formula and optional engine");
              const CNF cnf = parse_auto (detail::require_string (args[0], "formula"));
              const Engine engine = args.size () == 2 ? detail::parse_engine (args[1]) : Engine::CDCL;
              return detail::solve_result_value (solve_with_report (cnf, {engine}));
            })));
    library->table_entries.emplace_back (
        detail::string_value ("satisfiable"),
        detail::native_value (qamrpp::NativeFn (
            [] (qamrpp::Context &, std::vector<qamrpp::ValuePtr> &args) {
              if (args.empty () || args.size () > 2)
                throw std::invalid_argument (
                    "lsatie.satisfiable expects formula and optional engine");
              const CNF cnf = parse_auto (detail::require_string (args[0], "formula"));
              const Engine engine = args.size () == 2 ? detail::parse_engine (args[1]) : Engine::CDCL;
              return detail::bool_value (solve (cnf, engine).satisfiable ());
            })));
    library->table_entries.emplace_back (
        detail::string_value ("parse"),
        detail::native_value (qamrpp::NativeFn (
            [] (qamrpp::Context &, std::vector<qamrpp::ValuePtr> &args) {
              if (args.size () != 1)
                throw std::invalid_argument ("lsatie.parse expects one formula");
              const CNF cnf = parse_auto (detail::require_string (args[0], "formula"));
              auto result = detail::table_value ();
              detail::table_set (result, "clauses",
                                 detail::integer_value (static_cast<std::int64_t> (cnf.clause_count ())));
              detail::table_set (
                  result, "variables",
                  detail::integer_value (static_cast<std::int64_t> (cnf.variable_count ())));
              detail::table_set (result, "dimacs", detail::string_value (to_dimacs_string (cnf)));
              return result;
            })));
    library->table_entries.emplace_back (
        detail::string_value ("invoke"),
        detail::native_value (qamrpp::NativeFn (
            [&host] (qamrpp::Context &, std::vector<qamrpp::ValuePtr> &args) {
              if (args.empty () || args.size () > 2)
                throw std::invalid_argument (
                    "lsatie.invoke expects a command and optional argument table");
              const std::string name = detail::require_string (args[0], "command");
              const PluginArguments command_args =
                  args.size () == 2 ? detail::table_arguments (args[1]) : PluginArguments{};
              return detail::string_value (host.invoke (name, command_args));
            })));
    library->table_entries.emplace_back (
        detail::string_value ("commands"),
        detail::native_value (qamrpp::NativeFn (
            [&host] (qamrpp::Context &, std::vector<qamrpp::ValuePtr> &args) {
              if (!args.empty ())
                throw std::invalid_argument ("lsatie.commands expects no arguments");
              return detail::arguments_table (host.commands ());
            })));
    library->table_entries.emplace_back (
        detail::string_value ("register"),
        detail::native_value (qamrpp::NativeFn (
            [&host, owner, &context] (qamrpp::Context &,
                                      std::vector<qamrpp::ValuePtr> &args) {
              if (args.size () < 2 || !args[0] ||
                  args[0]->type != qamrpp::Value::STRING || !args[1] ||
                  args[1]->type != qamrpp::Value::FUNCTION)
                throw std::invalid_argument ("lsatie.register expects a name and function");
              const std::string name = args[0]->string_value;
              if (name.empty ())
                throw std::invalid_argument ("lsatie.register command name must not be empty");
              const qamrpp::ValuePtr callback = args[1];
              host.register_command (
                  name,
                  [owner, &context, callback] (const PluginArguments &arguments) {
                    std::vector<qamrpp::ValuePtr> call_args{
                        detail::arguments_table (arguments)};
                    qamrpp::Context &callback_context = owner ? *owner : context;
                    return detail::value_to_string (
                        callback->function_value (callback_context, call_args));
                  });
              return std::make_shared<qamrpp::Value> ();
            })));
    context.globals["lsatie"] = library;
  }

  static qamrpp::ValuePtr run (std::string_view source, PluginHost &host,
                               qamrpp::Context *context = nullptr)
  {
    if (context)
      {
        install_library (*context, host);
        return context->run (std::string (source));
      }
    auto owned = std::make_shared<qamrpp::Context> ();
    install_library (*owned, host, owned);
    return owned->run (std::string (source));
  }
};

inline void install_lua_library (qamrpp::Context &context, PluginHost &host)
{
  LuaPlugin::install_library (context, host);
}

} // namespace satie
