#include "Grafitt-ModuleHost.hpp"

#include <iostream>

namespace module_base {
static const GM_CapabilityDecl caps[] = {
    { GM_CAP_ANALYSIS, 1u, 0u, { "analysis.base", 13u } }
};
static const GM_ModMetadata_Record meta = {
    .size = static_cast<uint32_t>(sizeof(GM_ModMetadata_Record)),
    .abi_min_version = GM_MODULE_ABI_MIN_SUPPORTED,
    .abi_max_version = GM_MODULE_ABI_MAX_SUPPORTED,
    .module_flags = 0u,
    .module_version = { 0u, 1u, 0u, 0u },
    .thread_safety = GM_THREAD_INTERNAL,
    .reserved0 = 0u,
    .module_name = { "base-module", 11u },
    .vendor = { "grafitt", 7u },
    .build_id = { "dev", 3u },
    .license = { "MIT", 3u },
    .capability_count = 1u,
    .reserved1 = 0u,
    .capabilities = caps,
    .dependency_count = 0u,
    .reserved2 = 0u,
    .dependencies = nullptr,
    .reserved = { 0u, 0u, 0u, 0u }
};
static const GM_ModMetadata_Record* metadata() { return &meta; }
static GM_Status start(const GM_HostAPI* host, GM_CapabilityTable* out, uint32_t* inout_count) {
    if (!inout_count) return GM_STATUS_INVALID_ARGUMENT;
    if (GM_validate_host_api(host) != GM_STATUS_OK) return GM_STATUS_INVALID_ARGUMENT;
    if (!out) { *inout_count = 1u; return GM_STATUS_OK; }
    if (*inout_count < 1u) { *inout_count = 1u; return GM_STATUS_CAPACITY_TOO_SMALL; }
    out[0] = GM_CapabilityTable{
        .size = static_cast<uint32_t>(sizeof(GM_CapabilityTable)),
        .capability_id = GM_CAP_ANALYSIS,
        .interface_version = 1u,
        .reserved0 = 0u,
        .table = static_cast<const void*>(&caps[0]),
        .flags = 0u
    };
    *inout_count = 1u;
    return GM_STATUS_OK;
}
static GM_Status end() { return GM_STATUS_OK; }
} // namespace module_base

namespace module_dependent {
static const GM_CapabilityDecl caps[] = {
    { GM_CAP_EQSAT, 1u, 0u, { "eqsat.dependent", 15u } }
};
static const GM_DependencyDecl deps[] = {
    { { "base-module", 11u }, { 0u, 1u, 0u, 0u }, 1u, 0u }
};
static const GM_ModMetadata_Record meta = {
    .size = static_cast<uint32_t>(sizeof(GM_ModMetadata_Record)),
    .abi_min_version = GM_MODULE_ABI_MIN_SUPPORTED,
    .abi_max_version = GM_MODULE_ABI_MAX_SUPPORTED,
    .module_flags = 0u,
    .module_version = { 0u, 1u, 0u, 0u },
    .thread_safety = GM_THREAD_INTERNAL,
    .reserved0 = 0u,
    .module_name = { "dependent-module", 16u },
    .vendor = { "grafitt", 7u },
    .build_id = { "dev", 3u },
    .license = { "MIT", 3u },
    .capability_count = 1u,
    .reserved1 = 0u,
    .capabilities = caps,
    .dependency_count = 1u,
    .reserved2 = 0u,
    .dependencies = deps,
    .reserved = { 0u, 0u, 0u, 0u }
};
static const GM_ModMetadata_Record* metadata() { return &meta; }
static GM_Status start(const GM_HostAPI* host, GM_CapabilityTable* out, uint32_t* inout_count) {
    if (!inout_count) return GM_STATUS_INVALID_ARGUMENT;
    if (GM_validate_host_api(host) != GM_STATUS_OK) return GM_STATUS_INVALID_ARGUMENT;
    if (!out) { *inout_count = 1u; return GM_STATUS_OK; }
    if (*inout_count < 1u) { *inout_count = 1u; return GM_STATUS_CAPACITY_TOO_SMALL; }
    out[0] = GM_CapabilityTable{
        .size = static_cast<uint32_t>(sizeof(GM_CapabilityTable)),
        .capability_id = GM_CAP_EQSAT,
        .interface_version = 1u,
        .reserved0 = 0u,
        .table = static_cast<const void*>(&caps[0]),
        .flags = 0u
    };
    *inout_count = 1u;
    return GM_STATUS_OK;
}
static GM_Status end() { return GM_STATUS_OK; }
} // namespace module_dependent

int main() {
    using namespace grafitt::modulehost;
    module_manager host;

    auto s1 = host.stage_module(module_entry_points{
        .metadata = &module_dependent::metadata,
        .start = &module_dependent::start,
        .end = &module_dependent::end
    });
    if (!s1.ok()) return 1;

    auto s2 = host.stage_module(module_entry_points{
        .metadata = &module_base::metadata,
        .start = &module_base::start,
        .end = &module_base::end
    });
    if (!s2.ok()) return 1;

    auto all = host.start_all();
    if (!all.ok()) return 1;
    const auto [plan_status, plan] = host.startup_plan();
    if (!plan_status.ok()) return 1;
    if (plan.size() != 2) return 1;

    const auto active = host.active_modules();
    if (active.size() != 2) return 1;
    for (const auto& m : active) {
        if (!m.started) return 1;
        if (m.capabilities.empty()) return 1;
    }

    auto stop_dep = host.stop_module("dependent-module");
    if (!stop_dep.ok()) return 1; // explicit stop still works
    auto restarted = host.start_module("dependent-module");
    if (!restarted.ok()) return 1;
    auto stop_all = host.stop_all();
    if (!stop_all.ok()) return 1;
    const auto text = host.to_text_report();
    const auto json = host.to_json_report();
    if (text.empty() || json.empty()) return 1;
    if (json.find("\"lifecycle\"") == std::string::npos) return 1;

    std::cout << "Diagnostics: " << host.diagnostics().size() << "\n";
    return 0;
}
