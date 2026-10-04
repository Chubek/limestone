/**
 * @file stddef.cpp
 * @brief Standard default target plugin for NuPERF
 *
 * Emits C arrays as the default output format.
 */

#include <nuperf/nuperf-target.h>
#include <nuperf/nuperf-types.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace {

struct StddefOptions {
    std::string array_name = "nuperf_table";
    bool emit_header_guard = true;
    std::string guard_prefix = "NUPERF_TABLE";
};

struct StddefInstance {
    StddefOptions options;
};

const nuperf_target_option_t g_stddef_options[] = {
    {
        "array_name",
        "Name of the generated array symbol",
        NUPERF_OPTION_TYPE_STRING,
        "nuperf_table"
    },
    {
        "emit_header_guard",
        "Whether to emit header guard",
        NUPERF_OPTION_TYPE_BOOL,
        "true"
    },
    {
        "guard_prefix",
        "Prefix for header guard macro",
        NUPERF_OPTION_TYPE_STRING,
        "NUPERF_TABLE"
    }
};

nuperf_status_t stddef_create(
    const nuperf_target_t * /*target*/,
    nuperf_target_instance_t **out_instance) {
    if (!out_instance) {
        return NUPERF_ERR_INVALID_ARGUMENT;
    }

    try {
        auto *instance = new nuperf_target_instance_t();
        instance->impl = new StddefInstance();
        instance->target = nullptr;
        *out_instance = instance;
        return NUPERF_OK;
    } catch (...) {
        return NUPERF_ERR_OUT_OF_MEMORY;
    }
}

void stddef_destroy(nuperf_target_instance_t *instance) {
    if (!instance) {
        return;
    }

    if (instance->impl) {
        delete static_cast<StddefInstance *>(instance->impl);
        instance->impl = nullptr;
    }

    delete instance;
}

nuperf_status_t stddef_set_option(
    nuperf_target_instance_t *instance,
    const char *key,
    const char *value) {
    if (!instance || !instance->impl || !key || !value) {
        return NUPERF_ERR_INVALID_ARGUMENT;
    }

    auto *impl = static_cast<StddefInstance *>(instance->impl);

    if (std::strcmp(key, "array_name") == 0) {
        impl->options.array_name = value;
        return NUPERF_OK;
    }

    if (std::strcmp(key, "emit_header_guard") == 0) {
        if (std::strcmp(value, "true") == 0 || std::strcmp(value, "1") == 0) {
            impl->options.emit_header_guard = true;
        } else if (std::strcmp(value, "false") == 0 || std::strcmp(value, "0") == 0) {
            impl->options.emit_header_guard = false;
        } else {
            return NUPERF_ERR_INVALID_ARGUMENT;
        }
        return NUPERF_OK;
    }

    if (std::strcmp(key, "guard_prefix") == 0) {
        impl->options.guard_prefix = value;
        return NUPERF_OK;
    }

    return NUPERF_ERR_NOT_FOUND;
}

nuperf_status_t stddef_get_option(
    const nuperf_target_instance_t *instance,
    const char *key,
    char *value,
    size_t *inout_size) {
    if (!instance || !instance->impl || !key || !inout_size) {
        return NUPERF_ERR_INVALID_ARGUMENT;
    }

    auto *impl = static_cast<const StddefInstance *>(instance->impl);
    std::string result;

    if (std::strcmp(key, "array_name") == 0) {
        result = impl->options.array_name;
    } else if (std::strcmp(key, "emit_header_guard") == 0) {
        result = impl->options.emit_header_guard ? "true" : "false";
    } else if (std::strcmp(key, "guard_prefix") == 0) {
        result = impl->options.guard_prefix;
    } else {
        return NUPERF_ERR_NOT_FOUND;
    }

    const size_t needed = result.size() + 1;

    if (!value) {
        *inout_size = needed;
        return NUPERF_OK;
    }

    if (*inout_size < needed) {
        *inout_size = needed;
        return NUPERF_ERR_BUFFER_TOO_SMALL;
    }

    std::memcpy(value, result.c_str(), needed);
    *inout_size = needed;
    return NUPERF_OK;
}

nuperf_status_t stddef_emit(
    nuperf_target_instance_t *instance,
    const nuperf_hash_result_t *result,
    nuperf_emit_sink_t *sink) {
    if (!instance || !instance->impl || !result || !sink) {
        return NUPERF_ERR_INVALID_ARGUMENT;
    }

    auto *impl = static_cast<StddefInstance *>(instance->impl);
    const auto &opts = impl->options;

    nuperf_status_t st = sink->begin_artifact(
        sink,
        (opts.array_name + ".h").c_str(),
        NUPERF_ARTIFACT_KIND_HEADER,
        sink->user_data);
    if (st != NUPERF_OK) {
        return st;
    }

    std::string output;

    if (opts.emit_header_guard) {
        output += "#ifndef " + opts.guard_prefix + "_H\n";
        output += "#define " + opts.guard_prefix + "_H\n\n";
    }

    output += "#include <stddef.h>\n";
    output += "#include <stdint.h>\n\n";
    output += "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";

    if (result->data && result->data_size > 0) {
        const uint32_t *data = static_cast<const uint32_t *>(result->data);
        const size_t count = result->data_size / sizeof(uint32_t);

        output += "static const uint32_t " + opts.array_name + "[] = {\n";
        for (size_t i = 0; i < count; ++i) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "    0x%08x", data[i]);
            output += buf;
            if (i + 1 < count) output += ",";
            output += "\n";
        }
        output += "};\n\n";

        char size_buf[64];
        std::snprintf(size_buf, sizeof(size_buf), "static const size_t %s_size = %zu;\n\n",
                     opts.array_name.c_str(), count);
        output += size_buf;
    } else {
        output += "static const uint32_t " + opts.array_name + "[] = {};\n";
        output += "static const size_t " + opts.array_name + "_size = 0;\n\n";
    }

    char meta_buf[256];
    std::snprintf(meta_buf, sizeof(meta_buf),
                 "/* Generated by NuPERF\n * Method: %s\n * Keys: %zu\n */\n\n",
                 result->method_name ? result->method_name : "unknown",
                 result->key_count);
    output += meta_buf;

    output += "#ifdef __cplusplus\n}\n#endif\n\n";

    if (opts.emit_header_guard) {
        output += "#endif /* " + opts.guard_prefix + "_H */\n";
    }

    st = sink->write(sink, output.data(), output.size(), sink->user_data);
    if (st != NUPERF_OK) return st;

    return sink->end_artifact(sink, sink->user_data);
}

const nuperf_target_t g_stddef_target = {
    "stddef",
    "Standard C array output target",
    NUPERF_TARGET_FLAG_HEADER_ONLY | NUPERF_TARGET_FLAG_TEXT_OUTPUT,
    ".h",
    g_stddef_options,
    sizeof(g_stddef_options) / sizeof(g_stddef_options[0]),
    stddef_create,
    stddef_destroy,
    stddef_set_option,
    stddef_get_option,
    stddef_emit
};

} // namespace

extern "C" {

const nuperf_target_t *nuperf_stddef_target(void) {
    return &g_stddef_target;
}

} // extern "C"
