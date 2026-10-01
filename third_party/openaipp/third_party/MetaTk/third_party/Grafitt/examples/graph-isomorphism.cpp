#include "Grafitt.hpp"

#include <iostream>
#include <string>

int main() {
    using G = grafitt::imperative_graph<int, std::string>;

    G a(grafitt::direction::directed);
    a.add_edge(1, 2, "x");
    a.add_edge(2, 3, "y");
    a.add_edge(3, 1, "z");

    G b(grafitt::direction::directed);
    b.add_edge(10, 20, "x");
    b.add_edge(20, 30, "y");
    b.add_edge(30, 10, "z");

    G c(grafitt::direction::directed);
    c.add_edge(10, 20, "x");
    c.add_edge(20, 30, "y");
    c.add_edge(30, 10, "wrong");

    if (grafitt::algo::structurally_equal(a, b)) return 1;

    const auto iso_ab = grafitt::algo::graph_isomorphism(a, b);
    if (!iso_ab.isomorphic()) return 1;
    if (iso_ab.forward_map.size() != a.nb_vertex()) return 1;

    const auto iso_ac = grafitt::algo::graph_isomorphism(a, c);
    if (iso_ac.isomorphic()) return 1;

    grafitt::algo::isomorphism_options<int, std::string> unlabeled;
    unlabeled.match_edge_labels = false;
    const auto iso_ac_unlabeled = grafitt::algo::graph_isomorphism(a, c, unlabeled);
    if (!iso_ac_unlabeled.isomorphic()) return 1;

    std::cout << "Graph isomorphism checks work.\n";
    return 0;
}
