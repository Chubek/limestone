#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace satie::stdlib::log
{

enum class Level : std::uint8_t
{
  Debug = 0,
  Info = 1,
  Warning = 2,
  Error = 3,
  Silent = 4
};

inline std::string level_name (Level level)
{
  switch (level)
    {
    case Level::Debug:
      return "DEBUG";
    case Level::Info:
      return "INFO";
    case Level::Warning:
      return "WARNING";
    case Level::Error:
      return "ERROR";
    case Level::Silent:
      return "SILENT";
    }
  return "UNKNOWN";
}

struct Record
{
  Level level = Level::Info;
  std::string scope;
  std::string message;
};

/// Thread-safe leveled logger with pluggable sinks and per-level counters.
class Logger
{
public:
  using Sink = std::function<void (const Record &)>;

  explicit Logger (std::string scope = {}, Level level = Level::Info)
      : scope_ (std::move (scope)), level_ (level)
  {
  }

  void set_level (Level level)
  {
    std::lock_guard lock (mutex_);
    level_ = level;
  }

  void add_sink (Sink sink)
  {
    std::lock_guard lock (mutex_);
    sinks_.push_back (std::move (sink));
  }

  std::uint64_t count (Level level) const
  {
    std::lock_guard lock (mutex_);
    return counts_[static_cast<std::size_t> (level)];
  }

  void log (Level level, const std::string &message)
  {
    std::vector<Sink> sinks;
    {
      std::lock_guard lock (mutex_);
      if (level < level_)
        return;
      counts_[static_cast<std::size_t> (level)] += 1;
      sinks = sinks_;
    }
    const Record record{ level, scope_, message };
    for (const Sink &sink : sinks)
      sink (record);
  }

  void debug (const std::string &message) { log (Level::Debug, message); }
  void info (const std::string &message) { log (Level::Info, message); }
  void warning (const std::string &message) { log (Level::Warning, message); }
  void error (const std::string &message) { log (Level::Error, message); }

private:
  std::string scope_;
  Level level_ = Level::Info;
  std::vector<Sink> sinks_;
  std::uint64_t counts_[5] = { 0, 0, 0, 0, 0 };
  mutable std::mutex mutex_;
};

} // namespace satie::stdlib::log
