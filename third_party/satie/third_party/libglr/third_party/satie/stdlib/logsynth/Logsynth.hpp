#pragma once

#include <cstdint>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace satie::stdlib::logsynth
{

/// JSON-lines event writer: each event is one flat object with string,
/// integer, and boolean fields. Callers supply timestamps (no clock
/// dependency), keeping the writer deterministic and testable.
class EventLog
{
public:
  class Event
  {
  public:
    Event (EventLog &log, std::string name) : log_ (log), name_ (std::move (name)) {}

    Event &field (const std::string &key, const std::string &value)
    {
      strings_.emplace_back (key, value);
      return *this;
    }

    Event &field (const std::string &key, std::int64_t value)
    {
      integers_.emplace_back (key, value);
      return *this;
    }

    Event &field (const std::string &key, bool value)
    {
      booleans_.emplace_back (key, value);
      return *this;
    }

    void emit ()
    {
      std::string line = "{\"event\":" + quote (name_);
      for (const auto &[key, value] : strings_)
        line += ",\"" + key + "\":" + quote (value);
      for (const auto &[key, value] : integers_)
        line += ",\"" + key + "\":" + std::to_string (value);
      for (const auto &[key, value] : booleans_)
        line += ",\"" + key + "\":" + std::string (value ? "true" : "false");
      log_.write (line + "}\n");
    }

  private:
    static std::string quote (const std::string &text)
    {
      std::string out = "\"";
      for (char c : text)
        if (c == '"' || c == '\\')
          {
            out.push_back ('\\');
            out.push_back (c);
          }
        else if (c == '\n')
          out += "\\n";
        else
          out.push_back (c);
      return out + "\"";
    }

    EventLog &log_;
    std::string name_;
    std::vector<std::pair<std::string, std::string>> strings_;
    std::vector<std::pair<std::string, std::int64_t>> integers_;
    std::vector<std::pair<std::string, bool>> booleans_;
  };

  explicit EventLog (std::ostream &out) : out_ (out) {}

  Event event (const std::string &name) { return Event (*this, name); }
  std::size_t count () const { return count_; }

private:
  void write (const std::string &line)
  {
    out_ << line;
    ++count_;
  }

  std::ostream &out_;
  std::size_t count_ = 0;
};

} // namespace satie::stdlib::logsynth
