/**
 * @file nuperf-method.h
 * @brief NuPERF method plugin interface
 *
 * Defines the interface implemented by perfect-hash construction methods.
 */

#ifndef NUPERF_NUPERF_METHOD_H
#define NUPERF_NUPERF_METHOD_H

#include <stddef.h>
#include <stdint.h>

#include <nuperf/nuperf-api.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup nuperf_method Method Interface
 * @brief Interfaces for hash construction methods.
 * @{
 */

/** Forward declarations. */
typedef struct nuperf_method_t nuperf_method_t;
typedef struct nuperf_method_instance_t nuperf_method_instance_t;
typedef struct nuperf_hash_result_t nuperf_hash_result_t;

/**
 * @brief Method capability flags.
 */
typedef enum nuperf_method_flags_t {
    NUPERF_METHOD_FLAG_NONE        = 0,
    NUPERF_METHOD_FLAG_MINIMAL     = 1u << 0,
    NUPERF_METHOD_FLAG_ORDERED     = 1u << 1,
    NUPERF_METHOD_FLAG_INCREMENTAL = 1u << 2,
    NUPERF_METHOD_FLAG_STATIC_ONLY = 1u << 3
} nuperf_method_flags_t;

/**
 * @brief Method option descriptor.
 */
typedef struct nuperf_method_option_t {
    const char *name;
    const char *description;
    nuperf_option_type_t type;
    const char *default_value;
} nuperf_method_option_t;

/**
 * @brief Built hash result produced by a method.
 *
 * This structure is intentionally generic. Targets can inspect or interpret
 * the payload using method-defined conventions.
 */
struct nuperf_hash_result_t {
    const char *method_name;
    nuperf_key_type_t key_type;
    size_t key_count;
    uint32_t flags;
    const void *data;
    size_t data_size;
    void *user_data;
};

/**
 * @brief Per-build method instance.
 */
struct nuperf_method_instance_t {
    const nuperf_method_t *method;
    void *impl;
};

/**
 * @brief Method virtual table and metadata.
 */
struct nuperf_method_t {
    /** Stable unique method name. */
    const char *name;

    /** Human-readable description. */
    const char *description;

    /** Bitwise OR of @ref nuperf_method_flags_t values. */
    uint32_t flags;

    /**
     * @brief Null-terminated array of supported key types.
     *
     * Optional. If NULL, the method is assumed to accept all key types.
     */
    const nuperf_key_type_t *supported_key_types;

    /**
     * @brief Array of supported options.
     *
     * Optional. If non-NULL, @p option_count entries are valid.
     */
    const nuperf_method_option_t *options;

    /** Number of entries in @p options. */
    size_t option_count;

    /**
     * @brief Create a method instance.
     *
     * @param method Method descriptor.
     * @param out_instance Receives the created instance.
     * @return Status code.
     */
    nuperf_status_t (*create)(
        const nuperf_method_t *method,
        nuperf_method_instance_t **out_instance);

    /**
     * @brief Destroy a method instance.
     *
     * @param instance Method instance, or NULL.
     */
    void (*destroy)(
        nuperf_method_instance_t *instance);

    /**
     * @brief Set a method option.
     *
     * @param instance Method instance.
     * @param key Option name.
     * @param value Option value as string.
     * @return Status code.
     */
    nuperf_status_t (*set_option)(
        nuperf_method_instance_t *instance,
        const char *key,
        const char *value);

    /**
     * @brief Get a method option.
     *
     * @param instance Method instance.
     * @param key Option name.
     * @param value Destination buffer, or NULL to query size.
     * @param inout_size Input buffer size, output required/written size.
     * @return Status code.
     */
    nuperf_status_t (*get_option)(
        const nuperf_method_instance_t *instance,
        const char *key,
        char *value,
        size_t *inout_size);

    /**
     * @brief Build a perfect hash structure from a keyset.
     *
     * @param instance Method instance.
     * @param keyset Input keyset.
     * @param out_result Receives the build result.
     * @return Status code.
     */
    nuperf_status_t (*build)(
        nuperf_method_instance_t *instance,
        const nuperf_keyset_t *keyset,
        nuperf_hash_result_t **out_result);

    /**
     * @brief Destroy a build result created by @ref build.
     *
     * @param result Result handle, or NULL.
     */
    void (*destroy_result)(
        nuperf_hash_result_t *result);
};

/**
 * @brief Register a construction method.
 *
 * The pointed-to descriptor must remain valid for the lifetime of the
 * registration unless the implementation documents otherwise.
 *
 * @param method Method descriptor.
 * @return Status code.
 */
nuperf_status_t nuperf_method_register(const nuperf_method_t *method);

/**
 * @brief Unregister a previously registered method by name.
 *
 * @param name Method name.
 * @return Status code.
 */
nuperf_status_t nuperf_method_unregister(const char *name);

/**
 * @brief Look up a registered method by name.
 *
 * @param name Method name.
 * @return Borrowed pointer to the method descriptor, or NULL if not found.
 */
const nuperf_method_t *nuperf_method_lookup(const char *name);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* NUPERF_NUPERF_METHOD_H */
