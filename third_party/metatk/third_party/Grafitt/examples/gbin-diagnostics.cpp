#include "Grafitt.hpp"

#include <iostream>
#include <string>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;

    G g(grafitt::direction::directed);
    g.add_edge("a", "b", "x");
    g.add_edge("b", "c", "y");

    const auto bytes = grafitt::gbin::serialize(g);
    const auto header = grafitt::gbin::inspect_header(bytes);
    if (!header) return 1;
    if (header->vertex_count != g.nb_vertex()) return 1;
    if (header->edge_count != g.nb_edges()) return 1;

    const auto decoded = grafitt::gbin::deserialize_detailed<G>(bytes, true);
    if (!decoded.ok()) return 1;
    if (!decoded.graph->mem_edge("a", "b")) return 1;

    auto corrupted = bytes;
    if (!corrupted.empty()) corrupted[0] = static_cast<std::uint8_t>('X');
    const auto bad = grafitt::gbin::deserialize_detailed<G>(corrupted, true);
    if (bad.status != grafitt::gbin::decode_status::bad_magic) return 1;

    std::cout << "GBIN header vertices: " << header->vertex_count << "\n";
    std::cout << "Decode offset: " << decoded.offset << "\n";
    return 0;
}
