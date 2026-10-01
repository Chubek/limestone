#include "Grafitt.hpp"

#include <iostream>
#include <string>

int main() {
    using Pattern = grafitt::imperative_graph<std::string, std::string>;
    using Target = grafitt::imperative_graph<std::string, std::string>;

    Pattern pattern(grafitt::direction::directed);
    pattern.add_edge("PUser", "PRepo", "writes");
    pattern.add_edge("PRepo", "PLang", "uses");

    Target target(grafitt::direction::directed);
    target.add_edge("alice", "repoA", "writes");
    target.add_edge("repoA", "cpp", "uses");
    target.add_edge("bob", "repoB", "writes");
    target.add_edge("repoB", "rust", "uses");
    target.add_edge("alice", "repoB", "stars");

    grafitt::algo::subgraph_match_options<std::string, std::string, std::string> options;
    options.max_witnesses = 8;
    options.vertex_compatible = [](const std::string& p, const std::string& t) {
        if (p == "PLang") return t == "cpp" || t == "rust";
        return !t.empty();
    };

    const auto exact = grafitt::algo::subgraph_match(pattern, target, options);
    if (!exact.matched()) return 1;

    for (const auto& witness : exact.witnesses) {
        const auto bindings = witness.to_named_bindings(
            [](const std::string& p) { return p; },
            [](const std::string& t) { return t; }
        );
        if (bindings.empty()) return 1;
    }

    auto heuristic_options = options;
    heuristic_options.max_search_steps = 2;
    heuristic_options.max_candidates_per_vertex = 1;
    const auto heuristic = grafitt::algo::subgraph_match_heuristic(pattern, target, heuristic_options);
    if (heuristic.status != grafitt::algo::subgraph_match_status::search_budget_exhausted &&
        heuristic.status != grafitt::algo::subgraph_match_status::match_found) {
        return 1;
    }

    std::cout << "Exact matches: " << exact.witnesses.size() << "\n";
    std::cout << "Heuristic search steps: " << heuristic.search_steps << "\n";
    return 0;
}
