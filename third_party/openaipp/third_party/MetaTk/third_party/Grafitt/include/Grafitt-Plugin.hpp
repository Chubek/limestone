#pragma once

#include "Grafitt.hpp"

#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace grafitt::plugin {

/**
 * @brief Pass category metadata for pipeline introspection.
 */
enum class pass_kind {
    analysis,
    transform,
    rewrite,
    eqsat,
    validation,
    exporter
};

/**
 * @brief Severity for pass diagnostics.
 */
enum class diagnostic_severity {
    info,
    warning,
    error
};

/**
 * @brief One pipeline diagnostic event.
 */
struct pass_diagnostic {
    diagnostic_severity severity { diagnostic_severity::info };
    std::string pass_name;
    std::string message;
};

/**
 * @brief Lightweight metadata for pass registration/introspection.
 */
struct pass_metadata {
    std::string name;
    pass_kind kind { pass_kind::transform };
    std::string version = "0.1.0";
    std::string description;
    std::vector<std::string> declares_preserved_analyses;
};

/**
 * @brief Analysis cache invalidation policy returned by a pass.
 */
enum class analysis_invalidation_policy {
    preserve_all,
    invalidate_all,
    invalidate_except_list
};

/**
 * @brief Context shared across passes in one pipeline run.
 */
template<class Subject>
struct pass_context {
    std::vector<pass_diagnostic> diagnostics;
    std::optional<Subject> original_snapshot;
    std::unordered_map<std::string, std::any> analysis_cache;
    std::size_t analysis_generation = 0;

    template<class T>
    [[nodiscard]] bool has_analysis(const std::string& key) const {
        auto it = analysis_cache.find(key);
        return it != analysis_cache.end() && it->second.type() == typeid(T);
    }

    template<class T>
    [[nodiscard]] const T* get_analysis(const std::string& key) const {
        auto it = analysis_cache.find(key);
        if (it == analysis_cache.end()) return nullptr;
        return std::any_cast<T>(&it->second);
    }

    template<class T>
    void put_analysis(std::string key, T value) {
        analysis_cache[std::move(key)] = std::any{std::move(value)};
    }

    void invalidate_all_analyses() {
        if (!analysis_cache.empty()) ++analysis_generation;
        analysis_cache.clear();
    }

    void invalidate_all_except(const std::vector<std::string>& keep) {
        std::unordered_set<std::string> keep_set(keep.begin(), keep.end());
        bool changed = false;
        for (auto it = analysis_cache.begin(); it != analysis_cache.end();) {
            if (!keep_set.contains(it->first)) {
                it = analysis_cache.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
        if (changed) ++analysis_generation;
    }
};

/**
 * @brief Result of running one pass.
 */
struct pass_result {
    bool success = true;
    bool changed = false;
    analysis_invalidation_policy invalidation_policy { analysis_invalidation_policy::preserve_all };
    std::vector<std::string> preserved_analyses;
};

/**
 * @brief Typed analysis key descriptor for cache-safe retrieval.
 */
template<class T>
struct analysis_key {
    using value_type = T;
    std::string id;
};

/**
 * @brief Abstract base interface for all C++ pipeline passes.
 */
template<class Subject>
class IGraphPass {
public:
    virtual ~IGraphPass() = default;
    [[nodiscard]] virtual pass_metadata metadata() const = 0;
    [[nodiscard]] virtual pass_result run(Subject& subject, pass_context<Subject>& context) = 0;
};

/**
 * @brief Abstract base for analysis passes with built-in cache contract.
 */
template<class Subject, class AnalysisT>
class IAnalysisPass : public IGraphPass<Subject> {
public:
    using analysis_type = AnalysisT;
    virtual ~IAnalysisPass() = default;

    [[nodiscard]] virtual analysis_key<AnalysisT> key() const = 0;
    [[nodiscard]] virtual AnalysisT compute(const Subject& subject, pass_context<Subject>& context) = 0;

    [[nodiscard]] pass_result run(Subject& subject, pass_context<Subject>& context) override {
        const auto k = key();
        const auto value = compute(static_cast<const Subject&>(subject), context);
        context.template put_analysis<AnalysisT>(k.id, value);
        pass_result out;
        out.changed = false;
        out.success = true;
        out.invalidation_policy = analysis_invalidation_policy::preserve_all;
        out.preserved_analyses.push_back(k.id);
        return out;
    }
};

/**
 * @brief Per-pass pipeline execution record.
 */
struct pass_run_record {
    std::string pass_name;
    pass_kind kind { pass_kind::transform };
    bool success = true;
    bool changed = false;
    analysis_invalidation_policy invalidation_policy { analysis_invalidation_policy::preserve_all };
    std::size_t preserved_analysis_count = 0;
    std::size_t analysis_cache_before = 0;
    std::size_t analysis_cache_after = 0;
    std::size_t analysis_generation_before = 0;
    std::size_t analysis_generation_after = 0;
    std::string invalidation_summary;
};

/**
 * @brief Aggregate pipeline run result.
 */
struct pipeline_result {
    bool success = true;
    bool changed = false;
    std::vector<pass_run_record> records;
    std::vector<pass_diagnostic> diagnostics;
    std::size_t final_analysis_generation = 0;
    std::size_t final_cached_analyses = 0;
};

namespace detail {

[[nodiscard]] inline std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

[[nodiscard]] inline const char* to_cstr(pass_kind kind) {
    switch (kind) {
        case pass_kind::analysis: return "analysis";
        case pass_kind::transform: return "transform";
        case pass_kind::rewrite: return "rewrite";
        case pass_kind::eqsat: return "eqsat";
        case pass_kind::validation: return "validation";
        case pass_kind::exporter: return "exporter";
    }
    return "unknown";
}

[[nodiscard]] inline const char* to_cstr(diagnostic_severity sev) {
    switch (sev) {
        case diagnostic_severity::info: return "info";
        case diagnostic_severity::warning: return "warning";
        case diagnostic_severity::error: return "error";
    }
    return "info";
}

[[nodiscard]] inline const char* to_cstr(analysis_invalidation_policy p) {
    switch (p) {
        case analysis_invalidation_policy::preserve_all: return "preserve_all";
        case analysis_invalidation_policy::invalidate_all: return "invalidate_all";
        case analysis_invalidation_policy::invalidate_except_list: return "invalidate_except_list";
    }
    return "preserve_all";
}

} // namespace detail

/**
 * @brief Exports pipeline run report in human-readable text.
 */
[[nodiscard]] inline std::string to_text_report(const pipeline_result& result) {
    std::ostringstream oss;
    oss << "pipeline.success=" << std::boolalpha << result.success
        << " changed=" << result.changed
        << " final_analysis_generation=" << result.final_analysis_generation
        << " final_cached_analyses=" << result.final_cached_analyses << "\n";
    oss << "passes(" << result.records.size() << "):\n";
    for (const auto& r : result.records) {
        oss << "  - " << r.pass_name
            << " kind=" << detail::to_cstr(r.kind)
            << " success=" << r.success
            << " changed=" << r.changed
            << " invalidation=" << detail::to_cstr(r.invalidation_policy)
            << " preserved=" << r.preserved_analysis_count
            << " cache=" << r.analysis_cache_before << "->" << r.analysis_cache_after
            << " gen=" << r.analysis_generation_before << "->" << r.analysis_generation_after;
        if (!r.invalidation_summary.empty()) oss << " (" << r.invalidation_summary << ")";
        oss << "\n";
    }
    oss << "diagnostics(" << result.diagnostics.size() << "):\n";
    for (const auto& d : result.diagnostics) {
        oss << "  - [" << detail::to_cstr(d.severity) << "] " << d.pass_name << ": " << d.message << "\n";
    }
    return oss.str();
}

/**
 * @brief Exports pipeline run report as JSON.
 */
[[nodiscard]] inline std::string to_json_report(const pipeline_result& result) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"success\":" << (result.success ? "true" : "false") << ",";
    oss << "\"changed\":" << (result.changed ? "true" : "false") << ",";
    oss << "\"final_analysis_generation\":" << result.final_analysis_generation << ",";
    oss << "\"final_cached_analyses\":" << result.final_cached_analyses << ",";
    oss << "\"records\":[";
    for (std::size_t i = 0; i < result.records.size(); ++i) {
        const auto& r = result.records[i];
        if (i) oss << ",";
        oss << "{"
            << "\"pass_name\":\"" << detail::escape_json(r.pass_name) << "\","
            << "\"kind\":\"" << detail::to_cstr(r.kind) << "\","
            << "\"success\":" << (r.success ? "true" : "false") << ","
            << "\"changed\":" << (r.changed ? "true" : "false") << ","
            << "\"invalidation_policy\":\"" << detail::to_cstr(r.invalidation_policy) << "\","
            << "\"preserved_analysis_count\":" << r.preserved_analysis_count << ","
            << "\"analysis_cache_before\":" << r.analysis_cache_before << ","
            << "\"analysis_cache_after\":" << r.analysis_cache_after << ","
            << "\"analysis_generation_before\":" << r.analysis_generation_before << ","
            << "\"analysis_generation_after\":" << r.analysis_generation_after << ","
            << "\"invalidation_summary\":\"" << detail::escape_json(r.invalidation_summary) << "\""
            << "}";
    }
    oss << "],";
    oss << "\"diagnostics\":[";
    for (std::size_t i = 0; i < result.diagnostics.size(); ++i) {
        const auto& d = result.diagnostics[i];
        if (i) oss << ",";
        oss << "{"
            << "\"severity\":\"" << detail::to_cstr(d.severity) << "\","
            << "\"pass_name\":\"" << detail::escape_json(d.pass_name) << "\","
            << "\"message\":\"" << detail::escape_json(d.message) << "\""
            << "}";
    }
    oss << "]";
    oss << "}";
    return oss.str();
}

/**
 * @brief Pipeline execution configuration.
 */
struct pipeline_options {
    bool emit_invalidation_diagnostics = false;
};

/**
 * @brief Result for pass-registry lookup/build operations.
 */
enum class pass_registry_status {
    ok,
    duplicate_name,
    not_found,
    factory_failed
};

/**
 * @brief Result for declarative pipeline assembly.
 */
struct pipeline_build_result {
    pass_registry_status status { pass_registry_status::ok };
    std::string failing_pass;
    std::size_t added_count = 0;
    std::size_t requested_count = 0;

    [[nodiscard]] bool ok() const noexcept { return status == pass_registry_status::ok; }
};

/**
 * @brief Named declarative pipeline presets.
 */
enum class pipeline_preset {
    analysis_first,
    normalize_rewrite_eqsat,
    validate_export
};

/**
 * @brief Sequential pass pipeline with shared diagnostics context.
 */
template<class Subject>
class PassPipeline {
private:
    std::vector<std::unique_ptr<IGraphPass<Subject>>> passes_;
    pipeline_options options_;

    [[nodiscard]] std::vector<std::string> merged_preserved_analyses(
        const pass_metadata& meta,
        const pass_result& res
    ) const {
        std::unordered_set<std::string> merged(
            meta.declares_preserved_analyses.begin(),
            meta.declares_preserved_analyses.end()
        );
        for (const auto& key : res.preserved_analyses) merged.insert(key);
        return std::vector<std::string>(merged.begin(), merged.end());
    }

public:
    PassPipeline() = default;
    explicit PassPipeline(pipeline_options options) : options_(std::move(options)) {}

    void set_options(pipeline_options options) { options_ = std::move(options); }
    [[nodiscard]] const pipeline_options& options() const noexcept { return options_; }

    void add_pass(std::unique_ptr<IGraphPass<Subject>> pass) {
        passes_.push_back(std::move(pass));
    }

    [[nodiscard]] pipeline_result run(Subject& subject) const {
        pipeline_result out;
        pass_context<Subject> ctx;
        ctx.original_snapshot = subject;

        for (const auto& pass : passes_) {
            const auto meta = pass->metadata();
            const auto cache_before = ctx.analysis_cache.size();
            const auto gen_before = ctx.analysis_generation;
            const auto res = pass->run(subject, ctx);
            const auto preserved = merged_preserved_analyses(meta, res);
            std::string invalidation_summary;

            if (res.invalidation_policy == analysis_invalidation_policy::invalidate_all) {
                const auto before = ctx.analysis_cache.size();
                ctx.invalidate_all_analyses();
                invalidation_summary = "removed=" + std::to_string(before);
                if (options_.emit_invalidation_diagnostics) {
                    ctx.diagnostics.push_back(pass_diagnostic{
                        .severity = diagnostic_severity::info,
                        .pass_name = meta.name,
                        .message = "Invalidated all analyses: " + invalidation_summary
                    });
                }
            } else if (res.invalidation_policy == analysis_invalidation_policy::invalidate_except_list) {
                const auto before = ctx.analysis_cache.size();
                ctx.invalidate_all_except(preserved);
                invalidation_summary = "kept=" + std::to_string(preserved.size()) +
                                       ", removed=" + std::to_string(before - ctx.analysis_cache.size());
                if (options_.emit_invalidation_diagnostics) {
                    ctx.diagnostics.push_back(pass_diagnostic{
                        .severity = diagnostic_severity::info,
                        .pass_name = meta.name,
                        .message = "Invalidated analyses except declared/preserved: " + invalidation_summary
                    });
                }
            } else {
                invalidation_summary = "preserved=" + std::to_string(preserved.size());
                if (options_.emit_invalidation_diagnostics) {
                    ctx.diagnostics.push_back(pass_diagnostic{
                        .severity = diagnostic_severity::info,
                        .pass_name = meta.name,
                        .message = "Preserved analyses: " + std::to_string(preserved.size())
                    });
                }
            }

            out.records.push_back(pass_run_record{
                .pass_name = meta.name,
                .kind = meta.kind,
                .success = res.success,
                .changed = res.changed,
                .invalidation_policy = res.invalidation_policy,
                .preserved_analysis_count = preserved.size(),
                .analysis_cache_before = cache_before,
                .analysis_cache_after = ctx.analysis_cache.size(),
                .analysis_generation_before = gen_before,
                .analysis_generation_after = ctx.analysis_generation,
                .invalidation_summary = invalidation_summary
            });
            out.changed = out.changed || res.changed;
            if (!res.success) {
                out.success = false;
                break;
            }
        }
        out.final_analysis_generation = ctx.analysis_generation;
        out.final_cached_analyses = ctx.analysis_cache.size();
        out.diagnostics = std::move(ctx.diagnostics);
        return out;
    }
};

template<class Subject>
class PassRegistry {
public:
    using factory_fn = std::function<std::unique_ptr<IGraphPass<Subject>>()>;

private:
    struct entry {
        pass_metadata metadata;
        factory_fn factory;
    };
    std::unordered_map<std::string, entry> entries_;

public:
    [[nodiscard]] pass_registry_status register_factory(
        pass_metadata metadata,
        factory_fn factory
    ) {
        if (metadata.name.empty()) return pass_registry_status::factory_failed;
        if (!factory) return pass_registry_status::factory_failed;
        if (entries_.contains(metadata.name)) return pass_registry_status::duplicate_name;
        // Capture the key before moving `metadata` into the entry: the previous
        // `emplace(metadata.name, entry{std::move(metadata), ...})` read the key
        // from the same object it moved in the same (unsequenced) call, so on
        // some compilers the key was read after the move and the pass landed
        // under the empty key "" -- breaking every later lookup/preset.
        std::string key = metadata.name;
        entries_.emplace(std::move(key), entry{std::move(metadata), std::move(factory)});
        return pass_registry_status::ok;
    }

    [[nodiscard]] pass_registry_status has(std::string_view name) const {
        return entries_.contains(std::string{name}) ? pass_registry_status::ok : pass_registry_status::not_found;
    }

    [[nodiscard]] std::vector<pass_metadata> list() const {
        std::vector<pass_metadata> out;
        out.reserve(entries_.size());
        for (const auto& [_, e] : entries_) out.push_back(e.metadata);
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
        return out;
    }

    [[nodiscard]] std::vector<pass_metadata> list_by_kind(pass_kind kind) const {
        std::vector<pass_metadata> out;
        for (const auto& [_, e] : entries_) {
            if (e.metadata.kind == kind) out.push_back(e.metadata);
        }
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
        return out;
    }

    [[nodiscard]] std::vector<std::string> names_by_kind(pass_kind kind) const {
        std::vector<std::string> out;
        for (const auto& meta : list_by_kind(kind)) out.push_back(meta.name);
        return out;
    }

    [[nodiscard]] std::pair<pass_registry_status, std::unique_ptr<IGraphPass<Subject>>> make(std::string_view name) const {
        auto it = entries_.find(std::string{name});
        if (it == entries_.end()) return {pass_registry_status::not_found, nullptr};
        auto pass = it->second.factory();
        if (!pass) return {pass_registry_status::factory_failed, nullptr};
        return {pass_registry_status::ok, std::move(pass)};
    }

    [[nodiscard]] pipeline_build_result append_to_pipeline(
        PassPipeline<Subject>& pipeline,
        const std::vector<std::string>& names
    ) const {
        pipeline_build_result out;
        out.requested_count = names.size();
        for (const auto& name : names) {
            auto [st, pass] = make(name);
            if (st != pass_registry_status::ok) {
                out.status = st;
                out.failing_pass = name;
                return out;
            }
            pipeline.add_pass(std::move(pass));
            ++out.added_count;
        }
        out.status = pass_registry_status::ok;
        return out;
    }

    [[nodiscard]] std::vector<std::string> preset_names(pipeline_preset preset) const {
        std::vector<std::string> names;
        auto append = [&](const std::vector<std::string>& src) {
            names.insert(names.end(), src.begin(), src.end());
        };
        switch (preset) {
            case pipeline_preset::analysis_first:
                append(names_by_kind(pass_kind::analysis));
                append(names_by_kind(pass_kind::transform));
                append(names_by_kind(pass_kind::rewrite));
                append(names_by_kind(pass_kind::eqsat));
                append(names_by_kind(pass_kind::validation));
                append(names_by_kind(pass_kind::exporter));
                break;
            case pipeline_preset::normalize_rewrite_eqsat:
                append(names_by_kind(pass_kind::transform));
                append(names_by_kind(pass_kind::rewrite));
                append(names_by_kind(pass_kind::eqsat));
                break;
            case pipeline_preset::validate_export:
                append(names_by_kind(pass_kind::validation));
                append(names_by_kind(pass_kind::exporter));
                break;
        }
        return names;
    }

    [[nodiscard]] pipeline_build_result append_preset_to_pipeline(
        PassPipeline<Subject>& pipeline,
        pipeline_preset preset
    ) const {
        return append_to_pipeline(pipeline, preset_names(preset));
    }
};

template<class Subject, class AnalysisT, class ComputeFn>
class LambdaAnalysisPass final : public IAnalysisPass<Subject, AnalysisT> {
private:
    pass_metadata meta_;
    analysis_key<AnalysisT> key_;
    ComputeFn compute_;

public:
    LambdaAnalysisPass(pass_metadata meta, analysis_key<AnalysisT> key, ComputeFn compute)
        : meta_(std::move(meta)), key_(std::move(key)), compute_(std::move(compute)) {
        meta_.kind = pass_kind::analysis;
    }

    [[nodiscard]] pass_metadata metadata() const override { return meta_; }
    [[nodiscard]] analysis_key<AnalysisT> key() const override { return key_; }
    [[nodiscard]] AnalysisT compute(const Subject& subject, pass_context<Subject>& context) override {
        return std::invoke(compute_, subject, context);
    }
};

template<class Subject, class AnalysisT, class ComputeFn>
[[nodiscard]] std::unique_ptr<IAnalysisPass<Subject, AnalysisT>>
make_analysis_pass(
    pass_metadata meta,
    analysis_key<AnalysisT> key,
    ComputeFn compute
) {
    return std::make_unique<LambdaAnalysisPass<Subject, AnalysisT, ComputeFn>>(
        std::move(meta),
        std::move(key),
        std::move(compute)
    );
}

/**
 * @brief Options for the built-in equality-saturation transform pass.
 */
struct eqsat_pass_options {
    std::vector<eqsat::rewrite_rule> rules;
    eqsat::saturation_options saturation;
    eqsat::rule_schedule_policy schedule { eqsat::rule_schedule_policy::stable };
    std::optional<eqsat::extraction_options> extraction;
};

/**
 * @brief Diagnostics and artifacts produced by eqsat pass execution.
 */
struct eqsat_pass_result {
    eqsat::saturation_summary summary;
    eqsat::saturation_trace trace;
    std::optional<eqsat::extraction_result> extraction;
    std::optional<eqsat::extraction_explanation> extraction_explanation;
};

/**
 * @brief Eqsat transform pass wrapper for pipeline integration.
 *
 * @tparam Subject Pipeline subject type.
 * @tparam ToTerm Converts subject to `eqsat::term`.
 * @tparam ApplyExtracted Applies extracted term back to subject.
 */
template<class Subject, class ToTerm, class ApplyExtracted>
class EQSatTransformPass final : public IGraphPass<Subject> {
private:
    pass_metadata meta_;
    eqsat_pass_options options_;
    ToTerm to_term_;
    ApplyExtracted apply_extracted_;
    std::optional<eqsat_pass_result> last_;

public:
    EQSatTransformPass(
        pass_metadata meta,
        eqsat_pass_options options,
        ToTerm to_term,
        ApplyExtracted apply_extracted
    )
        : meta_(std::move(meta))
        , options_(std::move(options))
        , to_term_(std::move(to_term))
        , apply_extracted_(std::move(apply_extracted)) {
        meta_.kind = pass_kind::eqsat;
    }

    [[nodiscard]] pass_metadata metadata() const override {
        return meta_;
    }

    [[nodiscard]] const std::optional<eqsat_pass_result>& last_result() const noexcept {
        return last_;
    }

    [[nodiscard]] pass_result run(Subject& subject, pass_context<Subject>& context) override {
        pass_result out;
        if (options_.rules.empty()) {
            context.diagnostics.push_back(pass_diagnostic{
                .severity = diagnostic_severity::warning,
                .pass_name = meta_.name,
                .message = "No eqsat rules configured; pass performed no work."
            });
            out.changed = false;
            out.success = true;
            return out;
        }

        eqsat::egraph eg;
        const auto root = eg.add_term(std::invoke(to_term_, subject));

        eqsat_pass_result artifacts;
        artifacts.summary = eqsat::saturate(eg, options_.rules, artifacts.trace, options_.saturation, options_.schedule);

        if (options_.extraction.has_value()) {
            auto [best, explanation] = eqsat::extract_best_with_explanation(eg, root, *options_.extraction);
            artifacts.extraction = std::move(best);
            artifacts.extraction_explanation = std::move(explanation);
            if (artifacts.extraction->found) {
                if constexpr (std::is_same_v<std::invoke_result_t<ApplyExtracted, Subject&, const eqsat::term&>, bool>) {
                    out.changed = std::invoke(apply_extracted_, subject, artifacts.extraction->best);
                } else {
                    std::invoke(apply_extracted_, subject, artifacts.extraction->best);
                    out.changed = true;
                }
            }
        }

        context.diagnostics.push_back(pass_diagnostic{
            .severity = diagnostic_severity::info,
            .pass_name = meta_.name,
            .message = "eqsat iterations=" + std::to_string(artifacts.summary.iterations) +
                       ", merges=" + std::to_string(artifacts.summary.merges) +
                       ", trace=" + std::to_string(artifacts.summary.trace_events)
        });

        last_ = std::move(artifacts);
        out.success = true;
        out.invalidation_policy = out.changed
            ? analysis_invalidation_policy::invalidate_all
            : analysis_invalidation_policy::preserve_all;
        return out;
    }
};

template<class Subject, class ToTerm, class ApplyExtracted>
[[nodiscard]] std::unique_ptr<EQSatTransformPass<Subject, ToTerm, ApplyExtracted>>
make_eqsat_transform_pass(
    pass_metadata meta,
    eqsat_pass_options options,
    ToTerm to_term,
    ApplyExtracted apply_extracted
) {
    return std::make_unique<EQSatTransformPass<Subject, ToTerm, ApplyExtracted>>(
        std::move(meta),
        std::move(options),
        std::move(to_term),
        std::move(apply_extracted)
    );
}

template<class Subject, class ToTerm, class ApplyExtracted>
[[nodiscard]] pass_registry_status register_eqsat_transform_pass(
    PassRegistry<Subject>& registry,
    pass_metadata meta,
    eqsat_pass_options options,
    ToTerm to_term,
    ApplyExtracted apply_extracted
) {
    auto factory = [meta, options, to_term, apply_extracted]() mutable -> std::unique_ptr<IGraphPass<Subject>> {
        return make_eqsat_transform_pass<Subject>(
            meta,
            options,
            to_term,
            apply_extracted
        );
    };
    return registry.register_factory(std::move(meta), std::move(factory));
}

} // namespace grafitt::plugin
