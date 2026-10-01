#include "Grafitt.hpp"

#include <iostream>

int main() {
    using G = grafitt::imperative_graph<int, int>;

    G pattern(grafitt::direction::undirected);
    pattern.add_edge(1, 2, 1);
    pattern.add_edge(2, 3, 1);

    G target(grafitt::direction::undirected);
    target.add_edge(10, 20, 1);
    target.add_edge(20, 30, 1);
    target.add_edge(10, 30, 1); // triangle breaks induced path matching

    grafitt::algo::subgraph_match_options<int, int, int> non_induced;
    non_induced.match_edge_labels = true;
    non_induced.induced = false;
    non_induced.max_witnesses = 4;
    const auto plain = grafitt::algo::subgraph_match(pattern, target, non_induced);
    if (!plain.matched()) return 1;

    auto induced = non_induced;
    induced.induced = true;
    const auto strict = grafitt::algo::subgraph_match(pattern, target, induced);
    if (strict.matched()) return 1;

    std::cout << "Non-induced matched: " << plain.witnesses.size() << "\n";
    std::cout << "Induced matched: " << strict.witnesses.size() << "\n";
    return 0;
}
