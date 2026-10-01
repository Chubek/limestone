#include "Grafitt.hpp"
#include <fstream>
#include <string>

int main() {
    grafitt::imperative_graph<std::string, std::string> g;
    g.add_edge("Alice", "Bob", "friend");

    auto bytes = grafitt::gbin::serialize(g);
    std::ofstream os("friendship-graph.gbin", std::ios::binary);
    if (!os) return 1;
    os.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return os.good() ? 0 : 1;
}
