#pragma once

#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace satie::stdlib::graphviz
{

inline std::string escape_label (std::string_view text)
{
  std::string out;
  for (char c : text)
    {
      if (c == '"' || c == '\\')
        out.push_back ('\\');
      out.push_back (c);
    }
  return out;
}

/// Minimal DOT graph builder (directed by default).
class Dot
{
public:
  explicit Dot (std::string name, bool directed = true)
      : name_ (std::move (name)), directed_ (directed)
  {
  }

  void node (const std::string &id, const std::string &label = {},
             const std::map<std::string, std::string> &attrs = {})
  {
    nodes_.push_back ({ id, label.empty () ? id : label, attrs });
  }

  void edge (const std::string &from, const std::string &to,
             const std::string &label = {},
             const std::map<std::string, std::string> &attrs = {})
  {
    edges_.push_back ({ from, to, label, attrs });
  }

  std::string dump () const
  {
    std::ostringstream out;
    out << (directed_ ? "digraph " : "graph ") << name_ << " {\n";
    const std::string link = directed_ ? " -> " : " -- ";
    for (const DotNode &node : nodes_)
      {
        out << "  \"" << escape_label (node.id) << "\" [label=\"" << escape_label (node.label)
            << "\"";
        for (const auto &[key, value] : node.attrs)
          out << ", " << key << "=\"" << escape_label (value) << "\"";
        out << "];\n";
      }
    for (const DotEdge &edge : edges_)
      {
        out << "  \"" << escape_label (edge.from) << "\"" << link << "\""
            << escape_label (edge.to) << "\"";
        bool first = true;
        auto emit = [&] (const std::string &key, const std::string &value) {
          out << (first ? " [" : ", ") << key << "=\"" << escape_label (value) << "\"";
          first = false;
        };
        if (!edge.label.empty ())
          emit ("label", edge.label);
        for (const auto &[key, value] : edge.attrs)
          emit (key, value);
        if (!first)
          out << "]";
        out << ";\n";
      }
    out << "}\n";
    return out.str ();
  }

private:
  struct DotNode
  {
    std::string id;
    std::string label;
    std::map<std::string, std::string> attrs;
  };
  struct DotEdge
  {
    std::string from;
    std::string to;
    std::string label;
    std::map<std::string, std::string> attrs;
  };

  std::string name_;
  bool directed_ = true;
  std::vector<DotNode> nodes_;
  std::vector<DotEdge> edges_;
};

} // namespace satie::stdlib::graphviz
