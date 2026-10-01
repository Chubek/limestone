#include "Grafitt.hpp"

#include <iostream>
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;

    G g(grafitt::direction::directed);
    g.add_edge("alice", "repoA", "writes");
    g.add_edge("repoA", "cpp", "uses");
    g.add_edge("alice", "repoA", "owns");

    G pattern(grafitt::direction::directed);
    pattern.add_edge("User", "Repo", "writes");
    pattern.add_edge("Repo", "Lang", "uses");

    grafitt::algo::subgraph_match_options<std::string, std::string, std::string> opts;
    opts.max_witnesses = 1;

    auto [rewritten, result] = grafitt::rewrite::apply_first_subgraph_match_copy(
        g,
        pattern,
        [](G& graph, const auto& witness) {
            const auto it_user = witness.vertex_bindings.find("User");
            const auto it_repo = witness.vertex_bindings.find("Repo");
            if (it_user == witness.vertex_bindings.end() || it_repo == witness.vertex_bindings.end()) return;

            const std::string& user = it_user->second;
            const std::string& repo = it_repo->second;

            if (graph.mem_edge(user, repo)) {
                for (const auto& e : graph.find_all_edges(user, repo)) {
                    if (e.label == "writes") graph.remove_edge_e(e);
                }
            }
            graph.add_edge(user, repo, "authors");
        },
        opts
    );

    if (!result.applied) return 1;
    if (!rewritten.mem_edge("alice", "repoA")) return 1;

    bool has_authors = false;
    bool has_writes = false;
    for (const auto& e : rewritten.find_all_edges("alice", "repoA")) {
        if (e.label == "authors") has_authors = true;
        if (e.label == "writes") has_writes = true;
    }
    if (!has_authors || has_writes) return 1;

    const auto qmatches = grafitt::rewrite::to_query_match_results(result.witnesses);
    if (qmatches.empty()) return 1;
    if (!qmatches.front().metadata.contains("User")) return 1;

    std::cout << "Applied: " << std::boolalpha << result.applied << "\n";
    std::cout << "Bindings captured: " << result.bindings.size() << "\n";
    return 0;
}
