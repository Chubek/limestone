#include "Grafitt.hpp"

#include <iostream>

int main() {
    using G = grafitt::imperative_graph<int, double>;

    G g(grafitt::direction::directed);
    g.add_edge(1, 2, 5.0);
    g.add_edge(2, 3, 5.0);
    g.add_edge(1, 3, 20.0);

    auto weight = [](const G::edge_type& e) { return e.label; };

    auto cache = grafitt::algo::incremental_dijkstra_build(g, 1, weight, 0.0);
    if (!cache.ok()) return 1;
    auto d3_before = cache.distance_to(3);
    if (!d3_before || *d3_before != 10.0) return 1;

    g.add_edge(1, 3, 2.0);
    const auto u1 = grafitt::algo::incremental_dijkstra_relax_edge(g, 1, 3, weight, cache);
    if (!u1.ok()) return 1;
    auto d3_after = cache.distance_to(3);
    if (!d3_after || *d3_after != 2.0) return 1;

    // Unsupported increase/deletion path: mark stale then rebuild.
    g.remove_edge(1, 3, 2.0);
    grafitt::algo::incremental_dijkstra_mark_stale(cache);
    if (!cache.stale()) return 1;
    const auto stale_try = grafitt::algo::incremental_dijkstra_relax_from_vertex(g, 1, weight, cache);
    if (stale_try.status != grafitt::algo::incremental_dijkstra_update_status::cache_stale_requires_rebuild) return 1;

    const auto rebuild_status = grafitt::algo::incremental_dijkstra_rebuild(g, weight, cache);
    if (rebuild_status != grafitt::algo::incremental_dijkstra_cache_status::ok) return 1;
    auto d3_rebuilt = cache.distance_to(3);
    if (!d3_rebuilt || *d3_rebuilt != 10.0) return 1;

    auto p = cache.path_to(3);
    if (!p || p->size() != 3) return 1;

    std::cout << "Incremental Dijkstra cache flow works.\n";
    return 0;
}
