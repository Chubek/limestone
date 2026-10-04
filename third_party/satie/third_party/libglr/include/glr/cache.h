// include/glr/cache.h
#ifndef GLR_CACHE_H
#define GLR_CACHE_H

#include <glr/glr.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file cache.h
 * @brief libmdbx-based caching for incremental parsing in libglr
 */

/**
 * Opaque cache handle
 */
typedef struct glr_cache_t glr_cache_t;

/**
 * Cache configuration
 */
typedef struct {
    const char* mdbx_path;      /**< Path to libmdbx database directory */
    size_t map_size;            /**< Maximum database size (default: 1GB) */
    uint32_t max_readers;       /**< Max concurrent readers (default: 126) */
    bool use_async;             /**< Async writes (default: false) */
    uint64_t ttl_seconds;       /**< Time-to-live for entries (default: 86400 = 1 day) */
} glr_cache_config_t;

/**
 * Default cache configuration
 */
extern const glr_cache_config_t GLR_CACHE_DEFAULT_CONFIG;

/**
 * Open or create a cache database
 * 
 * @param config Cache configuration
 * @return Cache handle, or NULL on error
 */
glr_cache_t* glr_cache_open(const glr_cache_config_t* config);

/**
 * Close cache and flush all pending writes
 * 
 * @param cache Cache handle (will be freed)
 */
void glr_cache_close(glr_cache_t* cache);

/**
 * Flush all pending writes to disk
 * 
 * @param cache Cache handle
 * @return 0 on success, -1 on error
 */
int glr_cache_sync(glr_cache_t* cache);

/**
 * Compute SHA-256 hash of a byte range
 * 
 * @param data Input data
 * @param len Data length
 * @param hash Output buffer (32 bytes)
 */
void glr_cache_compute_hash(const uint8_t* data, size_t len, uint8_t hash[32]);

/**
 * Cache key for a parse forest
 */
typedef struct {
    uint8_t content_hash[32];   /**< SHA-256 of source text */
    uint32_t grammar_crc;       /**< Grammar version CRC */
    uint32_t start_symbol;      /**< Start symbol ID */
} glr_forest_cache_key_t;

/**
 * Look up a parse forest in cache
 * 
 * @param cache Cache handle
 * @param key Forest cache key
 * @param out_forest Output forest (must be freed with glr_forest_free)
 * @return 1 if found, 0 if not found, -1 on error
 */
int glr_cache_lookup_forest(glr_cache_t* cache,
                            const glr_forest_cache_key_t* key,
                            glr_forest_t** out_forest);

/**
 * Store a parse forest in cache
 * 
 * @param cache Cache handle
 * @param key Forest cache key
 * @param forest Forest to store (will be serialized)
 * @return 0 on success, -1 on error
 */
int glr_cache_store_forest(glr_cache_t* cache,
                           const glr_forest_cache_key_t* key,
                           const glr_forest_t* forest);

/**
 * Cache key for a GSS node
 */
typedef struct {
    uint32_t state_id;          /**< Parser state ID */
    uint32_t position;          /**< Source position (byte offset) */
    uint32_t grammar_crc;       /**< Grammar version CRC */
} glr_gss_cache_key_t;

/**
 * Look up a GSS node in cache
 * 
 * @param cache Cache handle
 * @param key GSS cache key
 * @param out_node Output GSS node (must be freed with glr_stack_node_free)
 * @return 1 if found, 0 if not found, -1 on error
 */
int glr_cache_lookup_gss_node(glr_cache_t* cache,
                              const glr_gss_cache_key_t* key,
                              glr_stack_node_t** out_node);

/**
 * Store a GSS node in cache
 * 
 * @param cache Cache handle
 * @param key GSS cache key
 * @param node GSS node to store
 * @return 0 on success, -1 on error
 */
int glr_cache_store_gss_node(glr_cache_t* cache,
                             const glr_gss_cache_key_t* key,
                             const glr_stack_node_t* node);

/**
 * Cache key for an AST subtree
 */
typedef struct {
    uint8_t content_hash[32];   /**< SHA-256 of subtree text */
    uint32_t production_id;     /**< Production rule ID */
    uint32_t grammar_crc;       /**< Grammar version CRC */
} glr_subtree_cache_key_t;

/**
 * Look up an AST subtree in cache
 * 
 * @param cache Cache handle
 * @param key Subtree cache key
 * @param out_data Output serialized subtree (must be freed)
 * @param out_len Output data length
 * @return 1 if found, 0 if not found, -1 on error
 */
int glr_cache_lookup_subtree(glr_cache_t* cache,
                             const glr_subtree_cache_key_t* key,
                             uint8_t** out_data,
                             size_t* out_len);

/**
 * Store an AST subtree in cache
 * 
 * @param cache Cache handle
 * @param key Subtree cache key
 * @param data Subtree serialized data
 * @param len Data length
 * @return 0 on success, -1 on error
 */
int glr_cache_store_subtree(glr_cache_t* cache,
                            const glr_subtree_cache_key_t* key,
                            const uint8_t* data,
                            size_t len);

/**
 * Invalidate all cache entries that depend on a source range
 * 
 * @param cache Cache handle
 * @param start_byte Start byte offset
 * @param end_byte End byte offset (exclusive)
 * @return 0 on success, -1 on error
 */
int glr_cache_invalidate_range(glr_cache_t* cache,
                               uint32_t start_byte,
                               uint32_t end_byte);

/**
 * Get cache statistics
 */
typedef struct glr_cache_stats_t {
    uint64_t forest_hits;
    uint64_t forest_misses;
    uint64_t gss_hits;
    uint64_t gss_misses;
    uint64_t subtree_hits;
    uint64_t subtree_misses;
    uint64_t cache_size_bytes;
    uint32_t forest_count;
    uint32_t gss_count;
    uint32_t subtree_count;
} glr_cache_stats_t;

/**
 * Get cache statistics
 * 
 * @param cache Cache handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int glr_cache_get_stats(glr_cache_t* cache, glr_cache_stats_t* stats);

/**
 * Clear all cache entries
 * 
 * @param cache Cache handle
 * @return 0 on success, -1 on error
 */
int glr_cache_clear(glr_cache_t* cache);

/**
 * Vacuum the cache database (remove unused space)
 * 
 * @param cache Cache handle
 * @return 0 on success, -1 on error
 */
int glr_cache_vacuum(glr_cache_t* cache);

#ifdef __cplusplus
}
#endif

#endif /* GLR_CACHE_H */
