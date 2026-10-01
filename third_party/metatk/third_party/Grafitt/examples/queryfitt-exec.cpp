#include "Grafitt.hpp"
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;
    G g;
    g.add_edge("Alice", "Bob", "friend");
    g.add_edge("Bob", "Carol", "friend");

    auto q = grafitt::queryfitt::shortest_path_between("Alice", "Carol");
    auto result = grafitt::queryfitt::execute(
        g,
        q,
        [](const std::string& v) { return v; },
        [](const std::string&) { return true; },
        [](const auto&, const auto&) { return true; }
    );
    (void)result;
    return 0;
}
