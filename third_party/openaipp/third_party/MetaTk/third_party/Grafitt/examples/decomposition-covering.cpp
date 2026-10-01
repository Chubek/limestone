#include "Grafitt.hpp"

#include <iostream>
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;
    G g(grafitt::direction::directed);
    g.add_edge("a", "b", "x");
    g.add_edge("b", "a", "x");
    g.add_edge("b", "c", "x");
    g.add_edge("c", "d", "x");
    g.add_edge("d", "e", "x");
    g.add_edge("e", "d", "x");

    const auto dag_cover = grafitt::algo::condensation_dag_cover(g);
    const auto dag_cover_check = grafitt::algo::validate_condensation_dag_cover(g, dag_cover);
    if (!dag_cover_check.ok()) return 1;
    if (dag_cover.pieces.empty()) return 1;

    const auto forest_cover = grafitt::algo::bfs_forest_cover(g);
    const auto forest_check = grafitt::algo::validate_bfs_forest_cover(g, forest_cover);
    if (!forest_check.ok()) return 1;
    if (forest_cover.pieces.empty()) return 1;

    const auto dot = grafitt::vizz::to_dot(dag_cover);
    const auto graphjs = grafitt::vizz::to_graphjs(forest_cover);
    if (dot.empty() || graphjs.empty()) return 1;

    std::cout << "DAG cover pieces: " << dag_cover.metrics.piece_count << "\n";
    std::cout << "Forest cover pieces: " << forest_cover.metrics.piece_count << "\n";
    return 0;
}
