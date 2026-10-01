#include "Grafitt.hpp"

#include <iostream>

int main() {
    using G = grafitt::imperative_graph<int>;

    G directed(grafitt::direction::directed);
    directed.add_edge(1, 2);
    directed.add_edge(2, 1);
    directed.add_edge(2, 3);
    directed.add_edge(3, 4);
    directed.add_edge(4, 3);
    directed.add_edge(4, 5);

    const auto wcc = grafitt::algo::weakly_connected_components(directed);
    const auto scc = grafitt::algo::strongly_connected_components(directed);

    if (wcc.component_count() != 1) return 1;
    if (scc.component_count() != 3) return 1;

    const auto c1 = scc.component_id_of(1);
    const auto c2 = scc.component_id_of(2);
    const auto c3 = scc.component_id_of(3);
    const auto c4 = scc.component_id_of(4);
    const auto c5 = scc.component_id_of(5);

    if (!c1 || !c2 || !c3 || !c4 || !c5) return 1;
    if (*c1 != *c2) return 1;
    if (*c3 != *c4) return 1;
    if (*c2 == *c3) return 1;
    if (*c5 == *c4 || *c5 == *c2) return 1;

    std::cout << "WCC count: " << wcc.component_count() << "\n";
    std::cout << "SCC count: " << scc.component_count() << "\n";
    return 0;
}
