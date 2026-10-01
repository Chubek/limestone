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
    g.add_vertex("Person:Dave");

    auto q = grafitt::queryfitt::aggregate("count", "vertices");
    auto& clause = std::get<grafitt::queryfitt::aggregation_clause>(q.body);
    clause.by = "vertex_type";

    auto result = grafitt::queryfitt::execute(
        g,
        q,
        [](const std::string& v) { return v; },
        [](const std::string&) { return true; },
        [](const auto&, const auto&) { return true; }
    );

    if (!std::holds_alternative<grafitt::queryfitt::grouped_scalar_result>(result)) return 1;
    const auto& groups = std::get<grafitt::queryfitt::grouped_scalar_result>(result).groups;

    auto it_friend = groups.find("Friend");
    auto it_person = groups.find("Person");
    if (it_friend == groups.end() || it_person == groups.end()) return 1;

    auto f = std::get_if<std::int64_t>(&it_friend->second);
    auto p = std::get_if<std::int64_t>(&it_person->second);
    return (f && p && *f == 2 && *p == 2) ? 0 : 1;
}
