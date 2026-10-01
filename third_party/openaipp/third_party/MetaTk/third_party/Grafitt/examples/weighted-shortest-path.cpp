#include "Grafitt.hpp"

#include <iostream>
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string, int>;
    G g(grafitt::direction::directed);
    g.add_edge("A", "B", 4);
    g.add_edge("A", "C", 1);
    g.add_edge("C", "B", 2);
    g.add_edge("B", "D", 1);
    g.add_edge("C", "D", 7);

    const auto result = grafitt::algo::dijkstra_shortest_paths(
        g,
        std::string("A"),
        [](const G::edge_type& e) { return e.label; },
        0
    );
    if (!result.ok()) return 1;

    const auto dist_d = result.distance_to("D");
    const auto path_d = result.path_to("D");
    if (!dist_d || !path_d) return 1;

    std::cout << "Distance A->D: " << *dist_d << "\nPath: ";
    for (std::size_t i = 0; i < path_d->size(); ++i) {
        std::cout << (*path_d)[i];
        if (i + 1 < path_d->size()) std::cout << " -> ";
    }
    std::cout << "\n";
    return 0;
}
