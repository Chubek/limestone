#include "Grafitt.hpp"

#include <iostream>
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;

    G g(grafitt::direction::directed);
    g.add_edge("u1", "r1", "writes");
    g.add_edge("r1", "cpp", "uses");
    g.add_edge("u2", "r2", "writes");
    g.add_edge("r2", "cpp", "uses");
    g.add_edge("u3", "r3", "writes");
    g.add_edge("r3", "rust", "uses");

    G pattern(grafitt::direction::directed);
    pattern.add_edge("User", "Repo", "writes");
    pattern.add_edge("Repo", "Lang", "uses");

    grafitt::algo::subgraph_match_options<std::string, std::string, std::string> match_opts;
    match_opts.max_witnesses = 16;
    match_opts.vertex_compatible = [](const std::string& p, const std::string& t) {
        if (p == "Lang") return t == "cpp";
        return true;
    };

    grafitt::rewrite::matched_rewrite_run_options run_opts;
    run_opts.strategy = grafitt::rewrite::matched_rewrite_strategy::bounded;
    run_opts.max_applications = 2;

    auto [rewritten, run] = grafitt::rewrite::apply_subgraph_matches_copy(
        g,
        pattern,
        [](G& graph, const auto& witness) {
            const auto user = witness.vertex_bindings.at("User");
            const auto repo = witness.vertex_bindings.at("Repo");
            for (const auto& e : graph.find_all_edges(user, repo)) {
                if (e.label == "writes") graph.remove_edge_e(e);
            }
            graph.add_edge(user, repo, "authors");
        },
        match_opts,
        run_opts
    );

    if (!run.changed) return 1;
    if (run.stats.applications != 2) return 1;
    if (!run.stats.hit_application_limit) return 1;
    if (run.applications.size() != 2) return 1;
    if (!rewritten.mem_edge("u1", "r1")) return 1;

    std::size_t authors = 0;
    rewritten.iter_edges_e([&](const auto& e) {
        if (e.label == "authors") ++authors;
    });
    if (authors != 2) return 1;

    std::cout << "Rounds: " << run.stats.rounds << "\n";
    std::cout << "Applications: " << run.stats.applications << "\n";
    std::cout << "Observed matches: " << run.stats.matches_observed << "\n";
    return 0;
}
