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

    auto cond = grafitt::algo::condensation_graph(g);
    const auto ok = grafitt::algo::validate_condensation_graph_detailed(g, cond);
    if (!ok.ok()) return 1;

    // Corrupt one witness edge to exercise diagnostics.
    if (!cond.edge_witnesses.empty()) {
        cond.edge_witnesses[0].src_vertex = 999;
    }

    const auto bad = grafitt::algo::validate_condensation_graph_detailed(g, cond);
    if (bad.ok()) return 1;
    if (bad.status != grafitt::algo::condensation_validation_status::component_mapping_missing &&
        bad.status != grafitt::algo::condensation_validation_status::witness_original_edge_missing) {
        return 1;
    }

    std::cout << "Diagnostic status: " << static_cast<int>(bad.status) << "\n";
    return 0;
}
