#include "Grafitt.hpp"

#include <iostream>
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string, int>;

    G g(grafitt::direction::directed);
    g.add_edge("S", "A", 4);
    g.add_edge("S", "B", 5);
    g.add_edge("A", "B", -3);
    g.add_edge("B", "C", 2);

    const auto ok = grafitt::algo::bellman_ford_shortest_paths(
        g,
        std::string("S"),
        [](const G::edge_type& e) { return e.label; },
        0
    );
    if (!ok.ok()) return 1;

    const auto dist_c = ok.distance_to("C");
    const auto path_c = ok.path_to("C");
    if (!dist_c || !path_c) return 1;
    std::cout << "Distance S->C: " << *dist_c << "\nPath: ";
    for (std::size_t i = 0; i < path_c->size(); ++i) {
        std::cout << (*path_c)[i];
        if (i + 1 < path_c->size()) std::cout << " -> ";
    }
    std::cout << "\n";

    G ng(grafitt::direction::directed);
    ng.add_edge("X", "Y", 1);
    ng.add_edge("Y", "Z", -4);
    ng.add_edge("Z", "X", 1);

    const auto bad = grafitt::algo::bellman_ford_shortest_paths(
        ng,
        std::string("X"),
        [](const G::edge_type& e) { return e.label; },
        0
    );
    if (!bad.has_negative_cycle()) return 1;

    std::cout << "Negative cycle witness: ";
    for (std::size_t i = 0; i < bad.negative_cycle_witness.size(); ++i) {
        std::cout << bad.negative_cycle_witness[i];
        if (i + 1 < bad.negative_cycle_witness.size()) std::cout << " -> ";
    }
    std::cout << "\n";
    return 0;
}
