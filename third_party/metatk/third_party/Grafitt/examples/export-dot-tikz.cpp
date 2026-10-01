#include "Grafitt.hpp"

#include <iostream>
#include <string>
#include <unordered_set>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;
    G g(grafitt::direction::directed);
    g.add_edge("A", "B", "ab");
    g.add_edge("B", "C", "bc");
    g.add_edge("C", "A", "ca");

    grafitt::vizz::dot_export_options<G> dot_opts;
    dot_opts.graph_name = "Example";
    dot_opts.vertex_style = grafitt::vizz::dot_vertex_highlight_style<std::string>(
        std::unordered_set<std::string>{"A", "C"},
        "shape=ellipse, style=filled, fillcolor=lightyellow",
        "shape=ellipse"
    );
    dot_opts.edge_style = grafitt::vizz::dot_edge_path_highlight_style<G>(
        std::vector<std::string>{"A", "B", "C"},
        "color=red, penwidth=2",
        "color=gray40"
    );
    const std::string dot = grafitt::vizz::to_dot(g, dot_opts);
    if (dot.find("digraph") == std::string::npos) return 1;

    grafitt::vizz::tikz_export_options<G> tikz_opts;
    tikz_opts.vertex_style = [](const std::string&) { return std::string("draw, circle"); };
    tikz_opts.edge_style = [](const G::edge_type&) { return std::string("->"); };
    const std::string tikz = grafitt::vizz::to_tikz(g, tikz_opts);
    if (tikz.find("\\begin{tikzpicture}") == std::string::npos) return 1;

    const auto cond = grafitt::algo::condensation_graph(g);
    grafitt::vizz::condensation_dot_export_options<std::string> cond_opts;
    cond_opts.component_style = grafitt::vizz::condensation_component_size_style<std::string>();
    const std::string cond_dot = grafitt::vizz::to_dot(cond, cond_opts);
    if (cond_dot.find("digraph") == std::string::npos) return 1;

    std::cout << "DOT size: " << dot.size() << "\n";
    std::cout << "TikZ size: " << tikz.size() << "\n";
    std::cout << "Condensation DOT size: " << cond_dot.size() << "\n";
    return 0;
}
