#include "Grafitt.hpp"
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;
    auto g = grafitt::builder::imperative_builder<G>(grafitt::direction::directed)
        .vertex("Alice")
        .vertex("Bob")
        .edge("Alice", "Bob", "friend")
        .build();
    return g.mem_edge("Alice", "Bob") ? 0 : 1;
}
