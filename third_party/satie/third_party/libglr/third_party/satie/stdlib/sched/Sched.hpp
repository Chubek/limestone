#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace satie::stdlib::sched
{

/// Deterministic task scheduler: tasks run once their dependencies have
/// run, in lexicographic order among the ready set. Cycles throw.
/// Results are stored by task name for downstream tasks to read.
class TaskGraph
{
public:
  using Task = std::function<std::string ()>;

  void add_task (std::string name, std::vector<std::string> deps, Task task)
  {
    if (tasks_.count (name) != 0)
      throw std::invalid_argument ("sched::TaskGraph duplicate task '" + name + "'");
    if (!task)
      throw std::invalid_argument ("sched::TaskGraph task needs a body");
    tasks_.emplace (std::move (name), Entry{ std::move (deps), std::move (task) });
  }

  std::map<std::string, std::string> run ()
  {
    std::map<std::string, std::string> results;
    std::map<std::string, bool> done;
    while (results.size () < tasks_.size ())
      {
        std::string next;
        for (const auto &[name, entry] : tasks_)
          {
            if (done[name])
              continue;
            bool ready = true;
            for (const std::string &dep : entry.deps)
              {
                if (tasks_.count (dep) == 0)
                  throw std::logic_error ("sched::TaskGraph unknown dependency '" + dep + "'");
                if (!done[dep])
                  ready = false;
              }
            if (ready && (next.empty () || name < next))
              next = name;
          }
        if (next.empty ())
          throw std::logic_error ("sched::TaskGraph found a dependency cycle");
        results.emplace (next, tasks_.at (next).task ());
        done[next] = true;
      }
    return results;
  }

private:
  struct Entry
  {
    std::vector<std::string> deps;
    Task task;
  };

  std::map<std::string, Entry> tasks_;
};

} // namespace satie::stdlib::sched
