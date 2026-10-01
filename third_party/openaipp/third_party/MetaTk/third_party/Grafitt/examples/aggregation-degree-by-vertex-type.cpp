#include "Grafitt.hpp"
#include <cmath>
#include <string>
#include <variant>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;
    G g;
    g.add_edge("Friend:Alice", "Friend:Bob", "knows");
    g.add_edge("Friend:Bob", "Person:Carol", "knows");
    g.add_edge("Person:Carol", "Person:Dave", "knows");

    auto q = grafitt::queryfitt::aggregate("avg", "degree");
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

    auto friend_avg = std::get_if<double>(&it_friend->second);
    auto person_avg = std::get_if<double>(&it_person->second);
    if (!friend_avg || !person_avg) return 1;

    const bool ok_friend = std::fabs(*friend_avg - 1.5) < 1e-9;
    const bool ok_person = std::fabs(*person_avg - 1.5) < 1e-9;
    return (ok_friend && ok_person) ? 0 : 1;
}
