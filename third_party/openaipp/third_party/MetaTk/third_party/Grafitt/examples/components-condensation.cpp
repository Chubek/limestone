#include "Grafitt.hpp"

#include <iostream>

int main() {
    grafitt::imperative_graph<int> g(grafitt::direction::directed);
    g.add_edge(1, 2);
    g.add_edge(2, 1); // SCC A: {1,2}
    g.add_edge(2, 3);
    g.add_edge(3, 4);
    g.add_edge(4, 3); // SCC B: {3,4}
    g.add_edge(4, 5); // SCC C: {5}

    const auto weak = grafitt::algo::weakly_connected_components(g);
    const auto strong = grafitt::algo::strongly_connected_components(g);
    const auto condensation = grafitt::algo::condensation_graph(g);

    if (weak.component_count() != 1) return 1;
    if (strong.component_count() != 3) return 1;
    if (!grafitt::algo::validate_condensation_graph(g, condensation)) return 1;
    if (condensation.dag.nb_vertex() != 3) return 1;
    if (condensation.dag.nb_edges() != 2) return 1;

    std::cout << "WCC count: " << weak.component_count() << "\n";
    std::cout << "SCC count: " << strong.component_count() << "\n";
    std::cout << "Condensation DAG vertices: " << condensation.dag.nb_vertex() << "\n";
    std::cout << "Condensation DAG edges: " << condensation.dag.nb_edges() << "\n";
    return 0;
}
