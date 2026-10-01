#include "Grafitt.hpp"

#include <iostream>

int main() {
    using namespace grafitt;

    eqsat::term seed{
        "add",
        {
            eqsat::term{"$x", {}},
            eqsat::term{"0", {}}
        }
    };

    std::vector<eqsat::rewrite_rule> rules{
        eqsat::rewrite_rule{
            .name = "add-zero-right",
            .lhs = eqsat::term{"add", {eqsat::term{"$a", {}}, eqsat::term{"0", {}}}},
            .rhs = eqsat::term{"$a", {}}
        },
        eqsat::rewrite_rule{
            .name = "add-zero-left",
            .lhs = eqsat::term{"add", {eqsat::term{"0", {}}, eqsat::term{"$a", {}}}},
            .rhs = eqsat::term{"$a", {}}
        }
    };

    eqsat::egraph eg;
    const auto root = eg.add_term(seed);
    eqsat::saturation_options opts;
    opts.max_iterations = 8;
    eqsat::saturation_trace trace;
    auto summary = eqsat::saturate(eg, rules, trace, opts, eqsat::rule_schedule_policy::round_robin);

    if (summary.iterations == 0) return 1;
    eqsat::extraction_options extract_opts;
    extract_opts.cost_model_name = "symbol-aware";
    extract_opts.tie_break = eqsat::extraction_tie_break::lexicographic_smallest;
    extract_opts.node_cost = [](std::string_view op, std::size_t arity) {
        if (op == "add") return static_cast<std::size_t>(3 + arity);
        if (!op.empty() && op.front() == '$') return static_cast<std::size_t>(1);
        return static_cast<std::size_t>(2 + arity);
    };

    const auto [extracted, explanation] = eqsat::extract_best_with_explanation(eg, root, extract_opts);
    if (!extracted.found) return 1;
    const auto by_rule = eqsat::explain_rule(trace, "add-zero-right");
    if (summary.rule_applications > 0 && by_rule.empty()) return 1;
    const auto by_class = eqsat::explain_class(trace, eg.find(root));
    (void)by_class;

    std::cout << "E-classes: " << summary.eclasses << "\n";
    std::cout << "E-nodes: " << summary.enodes << "\n";
    std::cout << "Trace events: " << summary.trace_events << "\n";
    std::cout << "Cost model: " << extracted.cost_model << "\n";
    std::cout << "Extraction decisions: " << explanation.decisions.size() << "\n";
    std::cout << "Best cost: " << extracted.cost << "\n";
    return 0;
}
