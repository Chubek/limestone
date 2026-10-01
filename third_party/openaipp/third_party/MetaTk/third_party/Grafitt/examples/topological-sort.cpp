#include "Grafitt.hpp"

#include <iostream>
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string>;
    G dag(grafitt::direction::directed);
    dag.add_edge("parse", "normalize");
    dag.add_edge("normalize", "rewrite");
    dag.add_edge("rewrite", "emit");
    dag.add_edge("parse", "validate");
    dag.add_edge("validate", "emit");

    const auto topo = grafitt::algo::topological_sort_stable(dag);
    if (!topo.has_order()) return 1;

    std::cout << "Topological order:\n";
    for (const auto& v : topo.order) std::cout << "  " << v << "\n";

    G cyclic(grafitt::direction::directed);
    cyclic.add_edge("a", "b");
    cyclic.add_edge("b", "c");
    cyclic.add_edge("c", "a");

    const auto cyclic_topo = grafitt::algo::topological_sort(cyclic);
    if (!cyclic_topo.has_cycle()) return 1;

    std::cout << "\nCycle witness:\n";
    for (std::size_t i = 0; i < cyclic_topo.cycle_witness.size(); ++i) {
        std::cout << cyclic_topo.cycle_witness[i];
        if (i + 1 < cyclic_topo.cycle_witness.size()) std::cout << " -> ";
    }
    std::cout << "\n";
    return 0;
}
