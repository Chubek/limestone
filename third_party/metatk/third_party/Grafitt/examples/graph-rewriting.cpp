#include "Grafitt.hpp"
#include <string>

int main() {
    using G = grafitt::persistent_graph<std::string, std::string>;
    G g;
    g = g.add_edge("Alice", "Bob", "friend");
    g = g.add_edge("Alice", "Carol", "friend");

    grafitt::rewrite::rule r{
        .name = "RedirectFromAlice",
        .match = grafitt::queryfitt::aggregate("count", "edges"),
        .replacement = "Alice->Charlie"
    };

    auto g2 = grafitt::rewrite::apply_once(g, r);
    return g2.mem_edge("Alice", "Charlie") ? 0 : 1;
}
