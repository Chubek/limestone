#include "Grafitt.hpp"
#include <string>

int main() {
    grafitt::imperative_graph<std::string, std::string> g;
    g.add_edge("Alice", "Bob", "friend");
    g.add_edge("Bob", "Carol", "friend");

    bool ok = grafitt::algo::reachable(g, std::string("Alice"), std::string("Bob"));
    auto p = grafitt::algo::shortest_path(g, std::string("Alice"), std::string("Bob"));
    return (ok && p && p->size() == 2) ? 0 : 1;
}
