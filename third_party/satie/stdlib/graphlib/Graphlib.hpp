#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

namespace satie::stdlib::graphlib
{

/// Directed graph over integer nodes with deterministic traversal order.
class DiGraph
{
public:
  void add_node (int node) { adjacency_[node]; }

  void add_edge (int from, int to)
  {
    adjacency_[from].push_back (to);
    adjacency_[to];
  }

  bool has_edge (int from, int to) const
  {
    auto it = adjacency_.find (from);
    if (it == adjacency_.end ())
      return false;
    return std::find (it->second.begin (), it->second.end (), to) != it->second.end ();
  }

  std::vector<int> nodes () const
  {
    std::vector<int> out;
    for (const auto &[node, successors] : adjacency_)
      {
        (void)successors;
        out.push_back (node);
      }
    return out;
  }

  std::size_t out_degree (int node) const
  {
    auto it = adjacency_.find (node);
    return it == adjacency_.end () ? 0 : it->second.size ();
  }

  std::vector<int> bfs (int start) const
  {
    std::vector<int> order;
    std::map<int, bool> seen;
    std::queue<int> queue;
    if (adjacency_.count (start) == 0)
      return order;
    queue.push (start);
    seen[start] = true;
    while (!queue.empty ())
      {
        const int node = queue.front ();
        queue.pop ();
        order.push_back (node);
        auto it = adjacency_.find (node);
        if (it == adjacency_.end ())
          continue;
        std::vector<int> successors = it->second;
        std::sort (successors.begin (), successors.end ());
        for (int next : successors)
          if (!seen[next])
            {
              seen[next] = true;
              queue.push (next);
            }
      }
    return order;
  }

  bool has_path (int from, int to) const
  {
    for (int node : bfs (from))
      if (node == to)
        return true;
    return false;
  }

  /// Kahn's algorithm in lexicographic order; throws on cycles.
  std::vector<int> topo_sort () const
  {
    std::map<int, std::size_t> indegree;
    for (const auto &[node, successors] : adjacency_)
      {
        indegree[node];
        for (int next : successors)
          ++indegree[next];
      }
    std::vector<int> ready;
    for (const auto &[node, degree] : indegree)
      if (degree == 0)
        ready.push_back (node);
    std::vector<int> order;
    std::map<int, std::vector<int>> succ = adjacency_;
    while (!ready.empty ())
      {
        std::sort (ready.begin (), ready.end ());
        const int node = ready.front ();
        ready.erase (ready.begin ());
        order.push_back (node);
        for (int next : succ[node])
          if (--indegree[next] == 0)
            ready.push_back (next);
      }
    if (order.size () != indegree.size ())
      throw std::logic_error ("graphlib::topo_sort found a cycle");
    return order;
  }

private:
  std::map<int, std::vector<int>> adjacency_;
};

} // namespace satie::stdlib::graphlib
