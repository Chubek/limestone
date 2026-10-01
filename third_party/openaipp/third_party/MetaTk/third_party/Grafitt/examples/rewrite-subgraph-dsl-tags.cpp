#include "Grafitt.hpp"

#include <iostream>
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;
    G g(grafitt::direction::directed);
    g.add_edge("user:alice", "repo:core", "writes");
    g.add_edge("repo:core", "lang:cpp", "uses");
    g.add_edge("user:bob", "repo:web", "writes");
    g.add_edge("repo:web", "lang:ts", "uses");

    G pattern(grafitt::direction::directed);
    pattern.add_edge("PUser", "PRepo", "writes");
    pattern.add_edge("PRepo", "PLang", "uses");

    grafitt::algo::subgraph_match_options<std::string, std::string, std::string> opts;
    opts.max_witnesses = 4;

#if GRAFITT_HAS_DSLUTILS
    auto result = grafitt::rewrite::apply_first_subgraph_match_with_dsl_tags<"[A-Z].*", ".*:.*">(
        g,
        pattern,
        [](const std::string& pv) { return pv; },
        [](const std::string& tv) { return tv; },
        [](G& graph, const auto& witness) {
            const auto user = witness.vertex_bindings.at("PUser");
            const auto repo = witness.vertex_bindings.at("PRepo");
            graph.add_edge(user, repo, "tagged");
        },
        opts
    );

    if (!result.applied) return 1;
    if (!g.mem_edge("user:alice", "repo:core")) return 1;
    bool tagged = false;
    for (const auto& e : g.find_all_edges("user:alice", "repo:core")) {
        if (e.label == "tagged") tagged = true;
    }
    if (!tagged) return 1;
#else
    (void)opts;
#endif

    std::cout << "DSL-tag constrained rewrite executed.\n";
    return 0;
}
