#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "Common.hpp"

namespace satie
{

class TheoryModule
{
public:
  virtual ~TheoryModule () = default;
  virtual std::string_view name () const noexcept = 0;
  virtual SolveResult check (const CNF &cnf) const = 0;
};

/// Calls every `satie_register_<theory>_theory()` hook once. Defined in
/// src/SatieModule.cpp; guarantees the static library's theory objects are
/// linked and registered even though nothing else references them.
void ensure_theories_registered ();

class TheoryRegistry
{
public:
  using Factory = std::function<std::unique_ptr<TheoryModule> ()>;

  static TheoryRegistry &instance ()
  {
    static TheoryRegistry registry;
    return registry;
  }

  void register_factory (std::string name, Factory factory)
  {
    std::lock_guard lock (mutex_);
    for (const auto &entry : entries_)
      if (entry.name == name)
        return;
    entries_.push_back ({ std::move (name), std::move (factory) });
  }

  std::unique_ptr<TheoryModule> create (std::string_view name) const
  {
    ensure_theories_registered ();
    std::lock_guard lock (mutex_);
    for (const auto &entry : entries_)
      if (entry.name == name)
        return entry.factory ();
    return nullptr;
  }

  std::vector<std::string> names () const
  {
    ensure_theories_registered ();
    std::lock_guard lock (mutex_);
    std::vector<std::string> out;
    out.reserve (entries_.size ());
    for (const auto &entry : entries_)
      out.push_back (entry.name);
    return out;
  }

  std::size_t size () const
  {
    ensure_theories_registered ();
    std::lock_guard lock (mutex_);
    return entries_.size ();
  }

private:
  struct Entry
  {
    std::string name;
    Factory factory;
  };

  mutable std::mutex mutex_;
  std::vector<Entry> entries_;
};

struct TheoryRegistrar
{
  TheoryRegistrar (std::string name, TheoryRegistry::Factory factory)
  {
    TheoryRegistry::instance ().register_factory (std::move (name), std::move (factory));
  }
};

/// Number of theories registered so far. Defined in SatieModule.cpp so the
/// registry singleton is referenced by the static library itself.
std::size_t registered_theory_count ();

} // namespace satie
