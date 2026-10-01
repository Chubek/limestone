#include "Grafitt.hpp"
#include <cstdint>
#include <cmath>
#include <string>
#include <variant>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;
    G g;
    g.add_edge("Friend:Alice", "Friend:Bob", "knows");
    g.add_edge("Friend:Bob", "Person:Carol", "knows");
    g.add_edge("Person:Carol", "Person:Dave", "knows");

    auto label_of = [](const std::string& v) { return v; };
    auto vpred = [](const std::string&) { return true; };
    auto epred = [](const auto&, const auto&) { return true; };

    auto q1 = grafitt::queryfitt::aggregate("count", "vertices");
    auto r1 = grafitt::queryfitt::execute(g, q1, label_of, vpred, epred);
    if (!std::holds_alternative<grafitt::queryfitt::scalar_result>(r1)) return 1;
    auto n1 = std::get_if<std::int64_t>(&std::get<grafitt::queryfitt::scalar_result>(r1).value);
    if (!n1 || *n1 != 4) return 1;

    auto q2 = grafitt::queryfitt::aggregate("count", "vertices");
    auto& c2 = std::get<grafitt::queryfitt::aggregation_clause>(q2.body);
    c2.where = grafitt::queryfitt::predicate_expr{"contains Friend:"};
    c2.by = "vertex_type";
    auto r2 = grafitt::queryfitt::execute(g, q2, label_of, vpred, epred);
    if (!std::holds_alternative<grafitt::queryfitt::grouped_scalar_result>(r2)) return 1;
    const auto& g2 = std::get<grafitt::queryfitt::grouped_scalar_result>(r2).groups;
    auto itf = g2.find("Friend");
    if (itf == g2.end()) return 1;
    auto nf = std::get_if<std::int64_t>(&itf->second);
    if (!nf || *nf != 2 || g2.size() != 1) return 1;

    auto q3 = grafitt::queryfitt::aggregate("avg", "degree");
    auto& c3 = std::get<grafitt::queryfitt::aggregation_clause>(q3.body);
    c3.by = "vertex_type";
    auto r3 = grafitt::queryfitt::execute(g, q3, label_of, vpred, epred);
    if (!std::holds_alternative<grafitt::queryfitt::grouped_scalar_result>(r3)) return 1;
    const auto& g3 = std::get<grafitt::queryfitt::grouped_scalar_result>(r3).groups;
    auto itf3 = g3.find("Friend");
    auto itp3 = g3.find("Person");
    if (itf3 == g3.end() || itp3 == g3.end()) return 1;
    auto favg = std::get_if<double>(&itf3->second);
    auto pavg = std::get_if<double>(&itp3->second);
    if (!favg || !pavg) return 1;
    if (std::fabs(*favg - 1.5) > 1e-9 || std::fabs(*pavg - 1.5) > 1e-9) return 1;

    return 0;
}
