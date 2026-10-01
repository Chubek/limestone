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
    g.add_edge("C", "D", "cd");

    std::unordered_set<std::string> highlighted_vertices {"A", "C"};
    auto vertex_style = grafitt::vizz::dot_vertex_highlight_style<std::string>(
        std::move(highlighted_vertices),
        "critical-node",
        "normal-node"
    );

    auto path_style = grafitt::vizz::dot_edge_path_highlight_style<G>(
        std::vector<std::string>{"A", "B", "C"},
        "critical-edge",
        "normal-edge"
    );

    grafitt::vizz::graphjs_export_options<G> options;
    options.vertex_class = vertex_style;
    options.edge_class = path_style;
    const auto json = grafitt::vizz::to_graphjs(g, options);
    if (json.find("\"nodes\"") == std::string::npos) return 1;
    if (json.find("\"edges\"") == std::string::npos) return 1;
    if (json.find("critical-node") == std::string::npos) return 1;
    if (json.find("critical-edge") == std::string::npos) return 1;

    const auto cond = grafitt::algo::condensation_graph(g);
    grafitt::vizz::condensation_graphjs_export_options<std::string> cond_opts;
    cond_opts.component_class =
        [](std::size_t, const std::vector<std::string>& members) {
            return members.size() > 1 ? std::string("multi-scc") : std::string("single-scc");
        };
    const auto cond_json = grafitt::vizz::to_graphjs(cond, cond_opts);
    if (cond_json.find("multi-scc") == std::string::npos) return 1;

    std::cout << "Graph.js JSON size: " << json.size() << "\n";
    std::cout << "Condensation JSON size: " << cond_json.size() << "\n";
    return 0;
}
