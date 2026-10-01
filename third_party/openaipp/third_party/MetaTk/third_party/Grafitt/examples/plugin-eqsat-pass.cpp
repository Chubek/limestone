#include "Grafitt-Plugin.hpp"

#include <iostream>

struct ExprBox {
    grafitt::eqsat::term expr;
};

int main() {
    using namespace grafitt;

    ExprBox subject{
        .expr = eqsat::term{
            "add",
            {eqsat::term{"x", {}}, eqsat::term{"0", {}}}
        }
    };

    plugin::eqsat_pass_options opts;
    opts.rules.push_back(eqsat::rewrite_rule{
        .name = "add-zero-right",
        .lhs = eqsat::term{"add", {eqsat::term{"$a", {}}, eqsat::term{"0", {}}}},
        .rhs = eqsat::term{"$a", {}}
    });
    opts.saturation.max_iterations = 6;
    opts.schedule = eqsat::rule_schedule_policy::stable;
    opts.extraction = eqsat::extraction_options{};

    plugin::PassRegistry<ExprBox> registry;
    const plugin::analysis_key<std::size_t> op_count_key{"analysis.op_count"};
    const auto reg_analysis = registry.register_factory(
        plugin::pass_metadata{
            .name = "op-count-analysis",
            .kind = plugin::pass_kind::analysis,
            .version = "0.1.0",
            .description = "Caches operation count for the expression",
            .declares_preserved_analyses = {"analysis.op_count"}
        },
        [op_count_key]() {
            return plugin::make_analysis_pass<ExprBox, std::size_t>(
                plugin::pass_metadata{
                    .name = "op-count-analysis",
                    .kind = plugin::pass_kind::analysis,
                    .version = "0.1.0",
                    .description = "Caches operation count for the expression",
                    .declares_preserved_analyses = {"analysis.op_count"}
                },
                op_count_key,
                [](const ExprBox& subject, plugin::pass_context<ExprBox>&) {
                    std::size_t count = 0;
                    std::function<void(const grafitt::eqsat::term&)> visit = [&](const grafitt::eqsat::term& t) {
                        ++count;
                        for (const auto& c : t.children) visit(c);
                    };
                    visit(subject.expr);
                    return count;
                }
            );
        }
    );
    if (reg_analysis != plugin::pass_registry_status::ok) return 1;

    const auto reg_eqsat = plugin::register_eqsat_transform_pass<ExprBox>(
        registry,
        plugin::pass_metadata{
            .name = "eqsat-canonicalize",
            .kind = plugin::pass_kind::eqsat,
            .version = "0.1.0",
            .description = "Canonicalize add-zero forms"
        },
        opts,
        [](const ExprBox& box) { return box.expr; },
        [](ExprBox& box, const eqsat::term& best) -> bool {
            const auto before = box.expr.op;
            box.expr = best;
            return before != box.expr.op;
        }
    );
    if (reg_eqsat != plugin::pass_registry_status::ok) return 1;

    plugin::PassPipeline<ExprBox> pipeline;
    const auto preset_build = registry.append_preset_to_pipeline(
        pipeline,
        plugin::pipeline_preset::analysis_first
    );
    if (!preset_build.ok()) return 1;
    pipeline.set_options(plugin::pipeline_options{.emit_invalidation_diagnostics = true});
    const auto run = pipeline.run(subject);

    if (!run.success) return 1;
    if (run.records.size() != 2) return 1;
    if (run.diagnostics.empty()) return 1;
    if (subject.expr.op != "x") return 1;
    if (run.final_analysis_generation == 0) return 1;
    if (run.final_cached_analyses != 0) return 1;
    if (run.diagnostics.size() < 2) return 1;
    const auto text_report = plugin::to_text_report(run);
    const auto json_report = plugin::to_json_report(run);
    if (text_report.empty() || json_report.empty()) return 1;
    if (json_report.find("\"records\"") == std::string::npos) return 1;

    std::cout << "Pipeline changed: " << std::boolalpha << run.changed << "\n";
    std::cout << "Diagnostics: " << run.diagnostics.size() << "\n";
    return 0;
}
