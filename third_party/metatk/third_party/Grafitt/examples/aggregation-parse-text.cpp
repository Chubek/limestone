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

    auto parsed = grafitt::queryfitt::parse_text(
        "aggregate in \"my-graphs/friendship-graph.gbin\" {\n"
        "  count vertices\n"
        "  where contains Friend:\n"
        "  by vertex_type\n"
        "}\n"
    );
    if (!parsed) return 1;

    auto result = grafitt::queryfitt::execute(
        g,
        parsed->value,
        [](const std::string& v) { return v; },
        [](const std::string&) { return true; },
        [](const auto&, const auto&) { return true; }
    );

    if (!std::holds_alternative<grafitt::queryfitt::grouped_scalar_result>(result)) return 1;
    const auto& groups = std::get<grafitt::queryfitt::grouped_scalar_result>(result).groups;
    auto it_friend = groups.find("Friend");
    if (it_friend == groups.end()) return 1;

    auto n = std::get_if<std::int64_t>(&it_friend->second);
    return (n && *n == 2 && groups.size() == 1) ? 0 : 1;
}
