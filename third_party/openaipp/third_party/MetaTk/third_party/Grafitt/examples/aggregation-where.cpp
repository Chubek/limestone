#include "Grafitt.hpp"
#include <cstdint>
#include <string>
#include <variant>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;
    G g;
    g.add_vertex("Friend:Alice");
    g.add_vertex("Friend:Bob");
    g.add_vertex("Person:Carol");

    auto q = grafitt::queryfitt::aggregate("count", "vertices");
    auto& clause = std::get<grafitt::queryfitt::aggregation_clause>(q.body);
    clause.where = grafitt::queryfitt::predicate_expr{"contains Friend:"};

    auto result = grafitt::queryfitt::execute(
        g,
        q,
        [](const std::string& v) { return v; },
        [](const std::string&) { return true; },
        [](const auto&, const auto&) { return true; }
    );

    if (!std::holds_alternative<grafitt::queryfitt::scalar_result>(result)) return 1;
    const auto& scalar = std::get<grafitt::queryfitt::scalar_result>(result).value;
    auto n = std::get_if<std::int64_t>(&scalar);
    return (n && *n == 2) ? 0 : 1;
}
