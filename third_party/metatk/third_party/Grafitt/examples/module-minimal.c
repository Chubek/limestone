#include "Grafitt-Module.h"

static const GM_CapabilityDecl g_caps[] = {
    { GM_CAP_ANALYSIS, 1u, 0u, { "analysis.sample", 15u } }
};

static const GM_ModMetadata_Record g_meta = {
    .size = (uint32_t)sizeof(GM_ModMetadata_Record),
    .abi_min_version = GM_MODULE_ABI_MIN_SUPPORTED,
    .abi_max_version = GM_MODULE_ABI_MAX_SUPPORTED,
    .module_flags = 0u,
    .module_version = { 0u, 1u, 0u, 0u },
    .thread_safety = GM_THREAD_EXTERNALLY_SYNCHRONIZED,
    .reserved0 = 0u,
    .module_name = { "module-minimal", 14u },
    .vendor = { "grafitt-example", 15u },
    .build_id = { "dev", 3u },
    .license = { "MIT", 3u },
    .capability_count = 1u,
    .reserved1 = 0u,
    .capabilities = g_caps,
    .dependency_count = 0u,
    .reserved2 = 0u,
    .dependencies = 0,
    .reserved = { 0u, 0u, 0u, 0u }
};

const GM_ModMetadata_Record* GM_ModMetadata(void) {
    return &g_meta;
}

GM_Status GM_ModStart(const GM_HostAPI* host_api, GM_CapabilityTable* out_tables, uint32_t* inout_count) {
    GM_Status st;
    GM_CapabilityTable table;
    if (inout_count == 0) return GM_STATUS_INVALID_ARGUMENT;

    st = GM_validate_host_api(host_api);
    if (st != GM_STATUS_OK) return st;

    if (out_tables == 0) {
        *inout_count = 1u;
        return GM_STATUS_OK;
    }
    if (*inout_count < 1u) {
        *inout_count = 1u;
        return GM_STATUS_CAPACITY_TOO_SMALL;
    }

    table.size = (uint32_t)sizeof(GM_CapabilityTable);
    table.capability_id = GM_CAP_ANALYSIS;
    table.interface_version = 1u;
    table.reserved0 = 0u;
    table.table = (const void*)&g_caps[0];
    table.flags = 0u;
    out_tables[0] = table;
    *inout_count = 1u;

    if (host_api->register_capability_fn != 0) {
        st = host_api->register_capability_fn(host_api->user_data, &out_tables[0]);
        if (st != GM_STATUS_OK) return st;
    }
    return GM_STATUS_OK;
}

GM_Status GM_ModEnd(void) {
    return GM_STATUS_OK;
}

int main(void) {
    GM_Status st;
    uint32_t count = 0u;
    st = GM_validate_metadata(GM_ModMetadata(), GM_MODULE_ABI_VERSION);
    if (st != GM_STATUS_OK) return 1;

    st = GM_ModStart(0, 0, &count);
    if (st != GM_STATUS_INVALID_ARGUMENT) return 1;
    return 0;
}
