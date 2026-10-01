#include "Grafitt.hpp"

#include <iostream>
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string, int>;

    G nonneg(grafitt::direction::directed);
    nonneg.add_edge("A", "B", 3);
    nonneg.add_edge("A", "C", 1);
    nonneg.add_edge("C", "B", 1);
    nonneg.add_edge("B", "D", 2);

    const auto auto_nonneg = grafitt::algo::weighted_shortest_paths(
        nonneg,
        std::string("A"),
        [](const G::edge_type& e) { return e.label; }
    );
    if (!auto_nonneg.ok()) return 1;
    if (auto_nonneg.strategy_used != grafitt::algo::weighted_shortest_path_strategy::dijkstra) return 1;

    const auto dist_d = auto_nonneg.distance_to("D");
    if (!dist_d || *dist_d != 4) return 1;

    G neg(grafitt::direction::directed);
    neg.add_edge("S", "A", 5);
    neg.add_edge("S", "B", 2);
    neg.add_edge("A", "B", -4);
    neg.add_edge("B", "T", 3);

    const auto auto_neg = grafitt::algo::weighted_shortest_paths(
        neg,
        std::string("S"),
        [](const G::edge_type& e) { return e.label; }
    );
    if (!auto_neg.ok()) return 1;
    if (auto_neg.strategy_used != grafitt::algo::weighted_shortest_path_strategy::bellman_ford) return 1;

    const auto forced_dijkstra = grafitt::algo::weighted_shortest_paths(
        neg,
        std::string("S"),
        [](const G::edge_type& e) { return e.label; },
        grafitt::algo::weighted_shortest_path_strategy::dijkstra
    );
    if (forced_dijkstra.ok()) return 1;

    std::cout << "Unified weighted shortest path facade works.\n";
    return 0;
}
