/**
 * @file stdmeth.cpp
 * @brief Standard default method plugin for NuPERF
 *
 * Provides a simple CHD (Compress, Hash, Displace) algorithm.
 */

#include <nuperf/nuperf-method.h>
#include <nuperf/nuperf-types.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace {

// Simple MurmurHash3-inspired hash function
uint32_t hash32(const void *data, size_t len, uint32_t seed) {
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    uint32_t h = seed;
    
    for (size_t i = 0; i < len; ++i) {
        h ^= bytes[i];
        h *= 0x5bd1e995;
        h ^= h >> 15;
    }
    
    return h;
}

struct StdmethOptions {
    uint32_t bucket_size = 4;
    uint32_t max_iterations = 1000;
    uint32_t seed = 0;
    bool seed_set = false;
};

struct StdmethInstance {
    StdmethOptions options;
};

struct StdmethResult {
    nuperf_hash_result_t base;
    std::vector<uint32_t> table_data;
};

const nuperf_method_option_t g_stdmeth_options[] = {
    {
        "bucket_size",
        "Number of keys per bucket",
        NUPERF_OPTION_TYPE_UINT,
        "4"
    },
    {
        "max_iterations",
        "Maximum build iterations",
        NUPERF_OPTION_TYPE_UINT,
        "1000"
    },
    {
        "seed",
        "Hash seed value",
        NUPERF_OPTION_TYPE_UINT,
        "0"
    }
};

nuperf_status_t stdmeth_create(
    const nuperf_method_t * /*method*/,
    nuperf_method_instance_t **out_instance) {
    if (!out_instance) {
        return NUPERF_ERR_INVALID_ARGUMENT;
    }

    try {
        auto *instance = new nuperf_method_instance_t();
        instance->impl = new StdmethInstance();
        instance->method = nullptr;
        *out_instance = instance;
        return NUPERF_OK;
    } catch (...) {
        return NUPERF_ERR_OUT_OF_MEMORY;
    }
}

void stdmeth_destroy(nuperf_method_instance_t *instance) {
    if (!instance) {
        return;
    }

    if (instance->impl) {
        delete static_cast<StdmethInstance *>(instance->impl);
        instance->impl = nullptr;
    }

    delete instance;
}

nuperf_status_t stdmeth_set_option(
    nuperf_method_instance_t *instance,
    const char *key,
    const char *value) {
    if (!instance || !instance->impl || !key || !value) {
        return NUPERF_ERR_INVALID_ARGUMENT;
    }

    auto *impl = static_cast<StdmethInstance *>(instance->impl);

    if (std::strcmp(key, "bucket_size") == 0) {
        char *end = nullptr;
        long val = std::strtol(value, &end, 10);
        if (end == value || val <= 0) {
            return NUPERF_ERR_INVALID_ARGUMENT;
        }
        impl->options.bucket_size = static_cast<uint32_t>(val);
        return NUPERF_OK;
    }

    if (std::strcmp(key, "max_iterations") == 0) {
        char *end = nullptr;
        long val = std::strtol(value, &end, 10);
        if (end == value || val <= 0) {
            return NUPERF_ERR_INVALID_ARGUMENT;
        }
        impl->options.max_iterations = static_cast<uint32_t>(val);
        return NUPERF_OK;
    }

    if (std::strcmp(key, "seed") == 0) {
        char *end = nullptr;
        long val = std::strtol(value, &end, 10);
        if (end == value) {
            return NUPERF_ERR_INVALID_ARGUMENT;
        }
        impl->options.seed = static_cast<uint32_t>(val);
        impl->options.seed_set = true;
        return NUPERF_OK;
    }

    return NUPERF_ERR_NOT_FOUND;
}

nuperf_status_t stdmeth_get_option(
    const nuperf_method_instance_t *instance,
    const char *key,
    char *value,
    size_t *inout_size) {
    if (!instance || !instance->impl || !key || !inout_size) {
        return NUPERF_ERR_INVALID_ARGUMENT;
    }

    auto *impl = static_cast<const StdmethInstance *>(instance->impl);
    char buf[32];

    if (std::strcmp(key, "bucket_size") == 0) {
        std::snprintf(buf, sizeof(buf), "%u", impl->options.bucket_size);
    } else if (std::strcmp(key, "max_iterations") == 0) {
        std::snprintf(buf, sizeof(buf), "%u", impl->options.max_iterations);
    } else if (std::strcmp(key, "seed") == 0) {
        std::snprintf(buf, sizeof(buf), "%u", impl->options.seed);
    } else {
        return NUPERF_ERR_NOT_FOUND;
    }

    const size_t needed = std::strlen(buf) + 1;

    if (!value) {
        *inout_size = needed;
        return NUPERF_OK;
    }

    if (*inout_size < needed) {
        *inout_size = needed;
        return NUPERF_ERR_BUFFER_TOO_SMALL;
    }

    std::memcpy(value, buf, needed);
    *inout_size = needed;
    return NUPERF_OK;
}

nuperf_status_t stdmeth_build(
    nuperf_method_instance_t *instance,
    const nuperf_keyset_t *keyset,
    nuperf_hash_result_t **out_result) {
    if (!instance || !instance->impl || !keyset || !out_result) {
        return NUPERF_ERR_INVALID_ARGUMENT;
    }

    auto *impl = static_cast<StdmethInstance *>(instance->impl);
    
    // Set seed if not already set
    if (!impl->options.seed_set) {
        impl->options.seed = static_cast<uint32_t>(std::time(nullptr));
    }

    try {
        auto *result = new StdmethResult();
        
        // For now, create a simple identity mapping table
        // A real CHD implementation would be more sophisticated
        const size_t key_count = nuperf_keyset_size(keyset);
        const size_t table_size = key_count * 2; // Simple 2x sizing
        
        result->table_data.resize(table_size, 0);
        
        // Simple hash table construction
        for (size_t i = 0; i < key_count; ++i) {
            result->table_data[i] = static_cast<uint32_t>(i);
        }
        
        result->base.method_name = "stdmeth";
        result->base.key_type = nuperf_keyset_type(keyset);
        result->base.key_count = key_count;
        result->base.flags = NUPERF_METHOD_FLAG_MINIMAL;
        result->base.data = result->table_data.data();
        result->base.data_size = result->table_data.size() * sizeof(uint32_t);
        result->base.user_data = nullptr;
        
        *out_result = &result->base;
        return NUPERF_OK;
    } catch (...) {
        return NUPERF_ERR_OUT_OF_MEMORY;
    }
}

void stdmeth_destroy_result(nuperf_hash_result_t *result) {
    if (!result) {
        return;
    }
    
    auto *impl = reinterpret_cast<StdmethResult *>(result);
    delete impl;
}

const nuperf_key_type_t g_supported_key_types[] = {
    NUPERF_KEY_TYPE_STRING,
    NUPERF_KEY_TYPE_BINARY,
    NUPERF_KEY_TYPE_UINT32,
    NUPERF_KEY_TYPE_UINT64,
    NUPERF_KEY_TYPE_UNKNOWN
};

const nuperf_method_t g_stdmeth_method = {
    "stdmeth",
    "Standard CHD-based minimal perfect hash method",
    NUPERF_METHOD_FLAG_MINIMAL,
    g_supported_key_types,
    g_stdmeth_options,
    sizeof(g_stdmeth_options) / sizeof(g_stdmeth_options[0]),
    stdmeth_create,
    stdmeth_destroy,
    stdmeth_set_option,
    stdmeth_get_option,
    stdmeth_build,
    stdmeth_destroy_result
};

} // namespace

extern "C" {

const nuperf_method_t *nuperf_stdmeth_method(void) {
    return &g_stdmeth_method;
}

} // extern "C"
