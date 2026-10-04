/**
 * @file nuperf-target.h
 * @brief NuPERF target plugin interface
 *
 * Defines the interface implemented by output/code-generation targets.
 */

#ifndef NUPERF_NUPERF_TARGET_H
#define NUPERF_NUPERF_TARGET_H

#include <stddef.h>
#include <stdint.h>

#include <nuperf/nuperf-api.h>
#include <nuperf/nuperf-method.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup nuperf_target Target Interface
 * @brief Interfaces for emission and code-generation targets.
 * @{
 */

/** Forward declarations. */
typedef struct nuperf_target_t nuperf_target_t;
typedef struct nuperf_target_instance_t nuperf_target_instance_t;
typedef struct nuperf_emit_sink_t nuperf_emit_sink_t;

/**
 * @brief Target capability flags.
 */
typedef enum nuperf_target_flags_t {
    NUPERF_TARGET_FLAG_NONE            = 0,
    NUPERF_TARGET_FLAG_HEADER_ONLY     = 1u << 0,
    NUPERF_TARGET_FLAG_MULTI_FILE      = 1u << 1,
    NUPERF_TARGET_FLAG_BINARY_OUTPUT   = 1u << 2,
    NUPERF_TARGET_FLAG_TEXT_OUTPUT     = 1u << 3,
    NUPERF_TARGET_FLAG_NEEDS_RUNTIME   = 1u << 4,
    NUPERF_TARGET_FLAG_EMBED_METADATA  = 1u << 5
} nuperf_target_flags_t;

/**
 * @brief Output artifact kind.
 */
typedef enum nuperf_artifact_kind_t {
    NUPERF_ARTIFACT_KIND_UNKNOWN = 0,
    NUPERF_ARTIFACT_KIND_SOURCE,
    NUPERF_ARTIFACT_KIND_HEADER,
    NUPERF_ARTIFACT_KIND_SCRIPT,
    NUPERF_ARTIFACT_KIND_BINARY,
    NUPERF_ARTIFACT_KIND_METADATA
} nuperf_artifact_kind_t;

/**
 * @brief Target option descriptor.
 */
typedef struct nuperf_target_option_t {
    const char *name;
    const char *description;
    nuperf_option_type_t type;
    const char *default_value;
} nuperf_target_option_t;

/**
 * @brief Emission sink used by targets to produce one or more artifacts.
 *
 * Targets call @ref begin_artifact, then one or more @ref write calls, then
 * @ref end_artifact for each emitted file/blob.
 */
struct nuperf_emit_sink_t {
    /**
     * @brief Begin a new output artifact.
     *
     * @param sink Sink handle.
     * @param name Logical artifact name or file name.
     * @param kind Artifact kind.
     * @param user_data User-supplied opaque pointer.
     * @return Status code.
     */
    nuperf_status_t (*begin_artifact)(
        nuperf_emit_sink_t *sink,
        const char *name,
        nuperf_artifact_kind_t kind,
        void *user_data);

    /**
     * @brief Write bytes to the current artifact.
     *
     * @param sink Sink handle.
     * @param data Data buffer.
     * @param size Number of bytes to write.
     * @param user_data User-supplied opaque pointer.
     * @return Status code.
     */
    nuperf_status_t (*write)(
        nuperf_emit_sink_t *sink,
        const void *data,
        size_t size,
        void *user_data);

    /**
     * @brief End the current artifact.
     *
     * @param sink Sink handle.
     * @param user_data User-supplied opaque pointer.
     * @return Status code.
     */
    nuperf_status_t (*end_artifact)(
        nuperf_emit_sink_t *sink,
        void *user_data);

    /** User-supplied opaque pointer passed back to callbacks. */
    void *user_data;
};

/**
 * @brief Per-emission target instance.
 */
struct nuperf_target_instance_t {
    const nuperf_target_t *target;
    void *impl;
};

/**
 * @brief Target descriptor and virtual function table.
 */
struct nuperf_target_t {
    /** Stable unique target name, such as "C" or "Rust". */
    const char *name;

    /** Human-readable description. */
    const char *description;

    /** Bitwise OR of @ref nuperf_target_flags_t values. */
    uint32_t flags;

    /**
     * @brief Default file extension for primary output.
     *
     * May be NULL for targets that emit multiple files or non-file output.
     */
    const char *default_extension;

    /**
     * @brief Supported target options.
     *
     * Optional. If non-NULL, @ref option_count entries are valid.
     */
    const nuperf_target_option_t *options;

    /** Number of entries in @ref options. */
    size_t option_count;

    /**
     * @brief Create a target instance.
     *
     * @param target Target descriptor.
     * @param out_instance Receives the created instance.
     * @return Status code.
     */
    nuperf_status_t (*create)(
        const nuperf_target_t *target,
        nuperf_target_instance_t **out_instance);

    /**
     * @brief Destroy a target instance.
     *
     * @param instance Target instance, or NULL.
     */
    void (*destroy)(
        nuperf_target_instance_t *instance);

    /**
     * @brief Set a target option.
     *
     * @param instance Target instance.
     * @param key Option name.
     * @param value Option value as string.
     * @return Status code.
     */
    nuperf_status_t (*set_option)(
        nuperf_target_instance_t *instance,
        const char *key,
        const char *value);

    /**
     * @brief Get a target option.
     *
     * @param instance Target instance.
     * @param key Option name.
     * @param value Destination buffer, or NULL to query size.
     * @param inout_size Input buffer size, output required/written size.
     * @return Status code.
     */
    nuperf_status_t (*get_option)(
        const nuperf_target_instance_t *instance,
        const char *key,
        char *value,
        size_t *inout_size);

    /**
     * @brief Emit output for a built hash result.
     *
     * @param instance Target instance.
     * @param result Hash result produced by a method.
     * @param sink Emission sink for one or more artifacts.
     * @return Status code.
     */
    nuperf_status_t (*emit)(
        nuperf_target_instance_t *instance,
        const nuperf_hash_result_t *result,
        nuperf_emit_sink_t *sink);
};

/**
 * @brief Register a target.
 *
 * The descriptor must remain valid while registered unless documented
 * otherwise by the implementation.
 *
 * @param target Target descriptor.
 * @return Status code.
 */
nuperf_status_t nuperf_target_register(const nuperf_target_t *target);

/**
 * @brief Unregister a previously registered target by name.
 *
 * @param name Target name.
 * @return Status code.
 */
nuperf_status_t nuperf_target_unregister(const char *name);

/**
 * @brief Look up a registered target by name.
 *
 * @param name Target name.
 * @return Borrowed pointer to target descriptor, or NULL if not found.
 */
const nuperf_target_t *nuperf_target_lookup(const char *name);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* NUPERF_NUPERF_TARGET_H */
