/**
 * @file nuperf-api.h
 * @brief NuPERF public C API
 *
 * Main entry point for the NuPERF library. Provides functions for creating
 * keysets, building perfect hash tables, and emitting output.
 */

#ifndef NUPERF_NUPERF_API_H
#define NUPERF_NUPERF_API_H

#include <nuperf/nuperf-types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup nuperf_api Public API
 * @brief Core NuPERF library functions.
 * @{
 */

/** Forward declarations. */
typedef struct nuperf_keyset_t nuperf_keyset_t;
typedef struct nuperf_table_t nuperf_table_t;
typedef struct nuperf_method_t nuperf_method_t;
typedef struct nuperf_target_t nuperf_target_t;

/**
 * @brief Get library version information.
 *
 * @return Version structure.
 */
nuperf_version_t nuperf_version(void);

/**
 * @brief Convert status code to human-readable string.
 *
 * @param status Status code.
 * @return Borrowed string describing the status.
 */
const char *nuperf_strerror(nuperf_status_t status);

/**
 * @brief Initialize the library with optional configuration.
 *
 * @param config Optional configuration, or NULL for defaults.
 * @return Status code.
 */
nuperf_status_t nuperf_init(void);

/**
 * @brief Shut down the library and release global resources.
 */
void nuperf_shutdown(void);

/* ========================================================================== */
/* Keyset API                                                                 */
/* ========================================================================== */

/**
 * @brief Create a new keyset.
 *
 * @param out_keyset Receives the created keyset handle.
 * @return Status code.
 */
nuperf_status_t nuperf_keyset_create(nuperf_keyset_t **out_keyset);

/**
 * @brief Destroy a keyset.
 *
 * @param keyset Keyset handle, or NULL.
 */
void nuperf_keyset_destroy(nuperf_keyset_t *keyset);

/**
 * @brief Clear all keys from a keyset.
 *
 * @param keyset Keyset handle.
 */
void nuperf_keyset_clear(nuperf_keyset_t *keyset);

/**
 * @brief Get the number of keys in a keyset.
 *
 * @param keyset Keyset handle.
 * @return Number of keys, or 0 if keyset is NULL.
 */
size_t nuperf_keyset_size(const nuperf_keyset_t *keyset);

/**
 * @brief Get the key type of a keyset.
 *
 * @param keyset Keyset handle.
 * @return Key type, or NUPERF_KEY_TYPE_UNKNOWN if keyset is NULL or empty.
 */
nuperf_key_type_t nuperf_keyset_type(const nuperf_keyset_t *keyset);

/**
 * @brief Add a string key to a keyset.
 *
 * @param keyset Keyset handle.
 * @param key Null-terminated string key.
 * @return Status code.
 */
nuperf_status_t nuperf_keyset_add_string(nuperf_keyset_t *keyset, const char *key);

/**
 * @brief Add a binary key to a keyset.
 *
 * @param keyset Keyset handle.
 * @param data Binary key data.
 * @param size Size of binary key in bytes.
 * @return Status code.
 */
nuperf_status_t nuperf_keyset_add_binary(
    nuperf_keyset_t *keyset,
    const void *data,
    size_t size);

/**
 * @brief Add a 32-bit unsigned integer key to a keyset.
 *
 * @param keyset Keyset handle.
 * @param key Integer key.
 * @return Status code.
 */
nuperf_status_t nuperf_keyset_add_uint32(nuperf_keyset_t *keyset, uint32_t key);

/**
 * @brief Add a 64-bit unsigned integer key to a keyset.
 *
 * @param keyset Keyset handle.
 * @param key Integer key.
 * @return Status code.
 */
nuperf_status_t nuperf_keyset_add_uint64(nuperf_keyset_t *keyset, uint64_t key);

/* ========================================================================== */
/* Table API                                                                  */
/* ========================================================================== */

/**
 * @brief Create a new table builder.
 *
 * @param out_table Receives the created table handle.
 * @return Status code.
 */
nuperf_status_t nuperf_table_create(nuperf_table_t **out_table);

/**
 * @brief Destroy a table builder.
 *
 * @param table Table handle, or NULL.
 */
void nuperf_table_destroy(nuperf_table_t *table);

/**
 * @brief Set the construction method for a table.
 *
 * @param table Table handle.
 * @param name Method name.
 * @return Status code.
 */
nuperf_status_t nuperf_table_set_method(nuperf_table_t *table, const char *name);

/**
 * @brief Get the current method name.
 *
 * @param table Table handle.
 * @param buffer Output buffer, or NULL to query size.
 * @param inout_size Input: buffer capacity. Output: required/written size.
 * @return Status code.
 */
nuperf_status_t nuperf_table_get_method(
    const nuperf_table_t *table,
    char *buffer,
    size_t *inout_size);

/**
 * @brief Set the output target for a table.
 *
 * @param table Table handle.
 * @param name Target name.
 * @return Status code.
 */
nuperf_status_t nuperf_table_set_target(nuperf_table_t *table, const char *name);

/**
 * @brief Get the current target name.
 *
 * @param table Table handle.
 * @param buffer Output buffer, or NULL to query size.
 * @param inout_size Input: buffer capacity. Output: required/written size.
 * @return Status code.
 */
nuperf_status_t nuperf_table_get_target(
    const nuperf_table_t *table,
    char *buffer,
    size_t *inout_size);

/**
 * @brief Set a method or target option.
 *
 * @param table Table handle.
 * @param key Option name.
 * @param value Option value as string.
 * @return Status code.
 */
nuperf_status_t nuperf_table_set_option(
    nuperf_table_t *table,
    const char *key,
    const char *value);

/**
 * @brief Get a method or target option.
 *
 * @param table Table handle.
 * @param key Option name.
 * @param buffer Output buffer, or NULL to query size.
 * @param inout_size Input: buffer capacity. Output: required/written size.
 * @return Status code.
 */
nuperf_status_t nuperf_table_get_option(
    const nuperf_table_t *table,
    const char *key,
    char *buffer,
    size_t *inout_size);

/**
 * @brief Associate a keyset with a table.
 *
 * @param table Table handle.
 * @param keyset Keyset handle. Ownership is not transferred.
 * @return Status code.
 */
nuperf_status_t nuperf_table_set_keyset(nuperf_table_t *table, nuperf_keyset_t *keyset);

/**
 * @brief Build the perfect hash table.
 *
 * @param table Table handle.
 * @return Status code.
 */
nuperf_status_t nuperf_table_build(nuperf_table_t *table);

/**
 * @brief Check if a table has been built.
 *
 * @param table Table handle.
 * @return NUPERF_TRUE if built, NUPERF_FALSE otherwise.
 */
nuperf_bool_t nuperf_table_is_built(const nuperf_table_t *table);

/**
 * @brief Get the number of keys in a built table.
 *
 * @param table Table handle.
 * @return Number of keys, or 0 if not built or NULL.
 */
size_t nuperf_table_key_count(const nuperf_table_t *table);

/**
 * @brief Get the key type of a built table.
 *
 * @param table Table handle.
 * @return Key type, or NUPERF_KEY_TYPE_UNKNOWN if not built or NULL.
 */
nuperf_key_type_t nuperf_table_key_type(const nuperf_table_t *table);

/**
 * @brief Get build statistics for a built table.
 *
 * @param table Table handle.
 * @param out_stats Receives build statistics.
 * @return Status code.
 */
nuperf_status_t nuperf_table_get_stats(
    const nuperf_table_t *table,
    nuperf_build_stats_t *out_stats);

/**
 * @brief Emit table output to a file.
 *
 * @param table Table handle.
 * @param path Output file path.
 * @return Status code.
 */
nuperf_status_t nuperf_table_emit_file(
    nuperf_table_t *table,
    const char *path);

/**
 * @brief Emit table output to a memory buffer.
 *
 * @param table Table handle.
 * @param buffer Output buffer, or NULL to query size.
 * @param inout_size Input: buffer capacity. Output: required/written size.
 * @return Status code.
 */
nuperf_status_t nuperf_table_emit_buffer(
    nuperf_table_t *table,
    void *buffer,
    size_t *inout_size);

/**
 * @brief Emit table output using a writer callback.
 *
 * @param table Table handle.
 * @param writer Writer callback.
 * @param user_data User pointer passed to writer.
 * @return Status code.
 */
nuperf_status_t nuperf_table_emit_writer(
    nuperf_table_t *table,
    nuperf_write_callback_t writer,
    void *user_data);

/* ========================================================================== */
/* Registry API                                                               */
/* ========================================================================== */

/**
 * @brief Get the number of registered methods.
 *
 * @return Method count.
 */
size_t nuperf_method_count(void);

/**
 * @brief Get the name of a registered method by index.
 *
 * @param index Method index (0 to nuperf_method_count() - 1).
 * @return Borrowed method name, or NULL if index is out of range.
 */
const char *nuperf_method_name(size_t index);

/**
 * @brief Get a registered method descriptor by index.
 *
 * @param index Method index (0 to nuperf_method_count() - 1).
 * @return Borrowed method descriptor, or NULL if index is out of range.
 */
const nuperf_method_t *nuperf_method_at(size_t index);

/**
 * @brief Get the number of registered targets.
 *
 * @return Target count.
 */
size_t nuperf_target_count(void);

/**
 * @brief Get the name of a registered target by index.
 *
 * @param index Target index (0 to nuperf_target_count() - 1).
 * @return Borrowed target name, or NULL if index is out of range.
 */
const char *nuperf_target_name(size_t index);

/**
 * @brief Get a registered target descriptor by index.
 *
 * @param index Target index (0 to nuperf_target_count() - 1).
 * @return Borrowed target descriptor, or NULL if index is out of range.
 */
const nuperf_target_t *nuperf_target_at(size_t index);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* NUPERF_NUPERF_API_H */
