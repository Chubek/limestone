#include "Grafitt.hpp"

#include <iostream>

int main() {
    using G = grafitt::imperative_graph<int>;

    G g(grafitt::direction::directed);
    g.add_edge(1, 2);
    g.add_edge(2, 1);
    g.add_edge(2, 3);
    g.add_edge(3, 4);
    g.add_edge(4, 3);
    g.add_edge(4, 5);

    const auto cond = grafitt::algo::condensation_graph(g);
    if (cond.component_count() != 3) return 1;
    if (!grafitt::algo::validate_condensation_graph(g, cond)) return 1;

    const auto topo = grafitt::algo::topological_sort(cond.dag);
    if (!topo.has_order()) return 1;

    std::cout << "Condensation components: " << cond.component_count() << "\n";
    std::cout << "Condensation edges: " << cond.dag.nb_edges() << "\n";
    return 0;
}
