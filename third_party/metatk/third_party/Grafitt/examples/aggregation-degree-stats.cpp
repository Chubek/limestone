#include "Grafitt.hpp"
#include <cstdint>
#include <string>
#include <variant>

int main() {
    using G = grafitt::imperative_graph<std::string, std::string>;
    G g;
    g.add_edge("Friend:Alice", "Friend:Bob", "knows");
    g.add_edge("Friend:Bob", "Person:Carol", "knows");
    g.add_edge("Person:Carol", "Person:Dave", "knows");

    auto q = grafitt::queryfitt::aggregate("sum", "degree");
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
    return (n && *n == 6) ? 0 : 1;
}
