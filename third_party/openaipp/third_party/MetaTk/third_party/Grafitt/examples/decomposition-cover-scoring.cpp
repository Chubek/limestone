#include "Grafitt.hpp"

#include <iostream>
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;
    G g(grafitt::direction::directed);
    g.add_edge("a", "b", "x");
    g.add_edge("b", "c", "x");
    g.add_edge("c", "a", "x");
    g.add_edge("c", "d", "x");
    g.add_edge("d", "e", "x");
    g.add_edge("e", "f", "x");

    grafitt::algo::condensation_dag_cover_options coarse;
    coarse.strategy = grafitt::algo::condensation_cover_strategy::weak_component_partition;
    const auto cover_coarse = grafitt::algo::condensation_dag_cover(g, coarse);

    grafitt::algo::condensation_dag_cover_options chunked;
    chunked.strategy = grafitt::algo::condensation_cover_strategy::topo_chunked;
    chunked.max_piece_components = 1;
    const auto cover_chunked = grafitt::algo::condensation_dag_cover(g, chunked);

    const auto scored_coarse = grafitt::algo::score_cover(cover_coarse, grafitt::algo::cover_objective::minimize_piece_count);
    const auto scored_chunked = grafitt::algo::score_cover(cover_chunked, grafitt::algo::cover_objective::minimize_piece_count);
    if (scored_coarse.score <= scored_chunked.score) return 1;

    grafitt::algo::bfs_forest_cover_options forest_opts;
    forest_opts.strategy = grafitt::algo::bfs_forest_cover_strategy::degree_desc_root_priority;
    forest_opts.max_piece_vertices = 3;
    const auto forest = grafitt::algo::bfs_forest_cover(g, forest_opts);
    const auto forest_check = grafitt::algo::validate_bfs_forest_cover(g, forest);
    if (!forest_check.ok()) return 1;

    const auto forest_score = grafitt::algo::score_cover(forest, grafitt::algo::cover_objective::maximize_locality);
    if (forest_score.score <= 0.0) return 1;

    std::cout << "Coarse score: " << scored_coarse.score << "\n";
    std::cout << "Chunked score: " << scored_chunked.score << "\n";
    std::cout << "Forest score: " << forest_score.score << "\n";
    return 0;
}
