/**
 * @file nuperf-types.h
 * @brief NuPERF common shared types
 *
 * This header centralizes fundamental enums, callbacks, and small utility
 * structs used by the public API, method interface, and target interface.
 */

#ifndef NUPERF_NUPERF_TYPES_H
#define NUPERF_NUPERF_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup nuperf_types Common Types
 * @brief Shared types used throughout NuPERF.
 * @{
 */

/**
 * @brief API status codes.
 */
typedef enum nuperf_status_t {
    /** Operation completed successfully. */
    NUPERF_OK = 0,

    /** One or more arguments were invalid. */
    NUPERF_ERR_INVALID_ARGUMENT,

    /** Memory allocation failed. */
    NUPERF_ERR_OUT_OF_MEMORY,

    /** Requested item was not found. */
    NUPERF_ERR_NOT_FOUND,

    /** Item already exists. */
    NUPERF_ERR_ALREADY_EXISTS,

    /** Operation requires a built table/result. */
    NUPERF_ERR_NOT_BUILT,

    /** Object has already been built/finalized. */
    NUPERF_ERR_ALREADY_BUILT,

    /** Requested feature or operation is unsupported. */
    NUPERF_ERR_UNSUPPORTED,

    /** Input data was invalid or malformed. */
    NUPERF_ERR_INVALID_DATA,

    /** I/O operation failed. */
    NUPERF_ERR_IO,

    /** Provided output buffer was too small. */
    NUPERF_ERR_BUFFER_TOO_SMALL,

    /** Object is in an invalid state for the requested operation. */
    NUPERF_ERR_INVALID_STATE,

    /** Build operation failed. */
    NUPERF_ERR_BUILD_FAILED,

    /** Emit/output generation failed. */
    NUPERF_ERR_EMIT_FAILED,

    /** Internal error. */
    NUPERF_ERR_INTERNAL
} nuperf_status_t;

/**
 * @brief Supported input key types.
 */
typedef enum nuperf_key_type_t {
    NUPERF_KEY_TYPE_UNKNOWN = 0,
    NUPERF_KEY_TYPE_STRING,
    NUPERF_KEY_TYPE_BINARY,
    NUPERF_KEY_TYPE_UINT32,
    NUPERF_KEY_TYPE_UINT64
} nuperf_key_type_t;

/**
 * @brief Generic option value types.
 *
 * Used by method and target option descriptors.
 */
typedef enum nuperf_option_type_t {
    NUPERF_OPTION_TYPE_STRING = 0,
    NUPERF_OPTION_TYPE_BOOL,
    NUPERF_OPTION_TYPE_INT,
    NUPERF_OPTION_TYPE_UINT,
    NUPERF_OPTION_TYPE_FLOAT
} nuperf_option_type_t;

/**
 * @brief Boolean type for C interfaces.
 */
typedef enum nuperf_bool_t {
    NUPERF_FALSE = 0,
    NUPERF_TRUE = 1
} nuperf_bool_t;

/**
 * @brief Binary key view.
 */
typedef struct nuperf_binary_key_t {
    /** Pointer to binary key bytes. */
    const void *data;

    /** Size of @ref data in bytes. */
    size_t size;
} nuperf_binary_key_t;

/**
 * @brief Library version information.
 */
typedef struct nuperf_version_t {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;

    /**
     * @brief Optional version suffix such as "dev" or "rc1".
     *
     * May be NULL or empty for release builds.
     */
    const char *suffix;
} nuperf_version_t;

/**
 * @brief Build statistics for a generated hash.
 */
typedef struct nuperf_build_stats_t {
    /** Number of input keys. */
    size_t key_count;

    /** Time spent building, in microseconds if available. */
    uint64_t build_time_us;

    /** Peak or approximate memory usage during build, in bytes if available. */
    size_t build_memory_bytes;

    /** Final serialized/output table size in bytes if available. */
    size_t table_size_bytes;

    /** Approximate bits per key if available. */
    double bits_per_key;

    /**
     * @brief Optional method-specific statistics string.
     *
     * Borrowed string owned by the implementation unless documented otherwise.
     * May be NULL.
     */
    const char *method_stats;
} nuperf_build_stats_t;

/**
 * @brief Logging levels.
 */
typedef enum nuperf_log_level_t {
    NUPERF_LOG_LEVEL_TRACE = 0,
    NUPERF_LOG_LEVEL_DEBUG,
    NUPERF_LOG_LEVEL_INFO,
    NUPERF_LOG_LEVEL_WARN,
    NUPERF_LOG_LEVEL_ERROR,
    NUPERF_LOG_LEVEL_FATAL
} nuperf_log_level_t;

/**
 * @brief Log callback signature.
 *
 * @param level Log severity.
 * @param message Null-terminated log message.
 * @param user_data User-supplied opaque pointer.
 */
typedef void (*nuperf_log_callback_t)(
    nuperf_log_level_t level,
    const char *message,
    void *user_data);

/**
 * @brief Progress callback signature.
 *
 * @param current Current progress position.
 * @param total Total work units, or 0 if unknown.
 * @param user_data User-supplied opaque pointer.
 * @return Non-zero to continue, zero to request cancellation.
 */
typedef int (*nuperf_progress_callback_t)(
    size_t current,
    size_t total,
    void *user_data);

/**
 * @brief Generic writer callback used for streaming output.
 *
 * @param data Data buffer.
 * @param size Number of bytes to write.
 * @param user_data User-supplied opaque pointer.
 * @return Number of bytes consumed/written. Returning fewer than @p size
 *         indicates failure or short write.
 */
typedef size_t (*nuperf_write_callback_t)(
    const void *data,
    size_t size,
    void *user_data);

/**
 * @brief Memory allocation callback.
 *
 * @param size Requested allocation size in bytes.
 * @param user_data User-supplied opaque pointer.
 * @return Allocated memory, or NULL on failure.
 */
typedef void *(*nuperf_alloc_fn_t)(
    size_t size,
    void *user_data);

/**
 * @brief Memory reallocation callback.
 *
 * @param ptr Existing allocation, or NULL.
 * @param new_size New size in bytes.
 * @param user_data User-supplied opaque pointer.
 * @return Reallocated memory, or NULL on failure.
 */
typedef void *(*nuperf_realloc_fn_t)(
    void *ptr,
    size_t new_size,
    void *user_data);

/**
 * @brief Memory free callback.
 *
 * @param ptr Pointer previously allocated by @ref nuperf_alloc_fn_t or
 *            @ref nuperf_realloc_fn_t. May be NULL.
 * @param user_data User-supplied opaque pointer.
 */
typedef void (*nuperf_free_fn_t)(
    void *ptr,
    void *user_data);

/**
 * @brief Custom allocator descriptor.
 */
typedef struct nuperf_allocator_t {
    nuperf_alloc_fn_t alloc;
    nuperf_realloc_fn_t realloc;
    nuperf_free_fn_t free;
    void *user_data;
} nuperf_allocator_t;

/**
 * @brief Optional library configuration used at initialization time.
 */
typedef struct nuperf_config_t {
    /**
     * @brief Custom allocator callbacks.
     *
     * If any callback is NULL, the implementation may fall back to defaults or
     * reject the configuration.
     */
    const nuperf_allocator_t *allocator;

    /** Optional log callback. */
    nuperf_log_callback_t log_callback;

    /** User pointer passed to @ref log_callback. */
    void *log_user_data;

    /** Optional progress callback. */
    nuperf_progress_callback_t progress_callback;

    /** User pointer passed to @ref progress_callback. */
    void *progress_user_data;
} nuperf_config_t;

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* NUPERF_NUPERF_TYPES_H */
