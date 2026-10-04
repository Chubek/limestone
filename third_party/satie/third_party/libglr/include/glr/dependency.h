#ifndef GLR_DEPENDENCY_H
#define GLR_DEPENDENCY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file dependency.h
 * @brief Dependency tracking for cache invalidation
 * 
 * Tracks which cache entries depend on which byte ranges in the source.
 * When a byte range is edited, all dependent cache entries are invalidated.
 */

/* Forward declaration */
typedef struct glr_cache_t glr_cache_t;

/**
 * Cache entry types
 */
typedef enum {
    GLR_CACHE_ENTRY_FOREST = 1,
    GLR_CACHE_ENTRY_GSS_NODE = 2,
    GLR_CACHE_ENTRY_SUBTREE = 3
} glr_cache_entry_type_t;

/**
 * Dependency record
 */
typedef struct {
    uint32_t start_byte;       /**< Start of dependent byte range */
    uint32_t end_byte;         /**< End of dependent byte range (exclusive) */
    uint64_t cache_key;        /**< Cache entry key */
    uint32_t entry_type;       /**< Type of cache entry */
} glr_dependency_t;

/**
 * Record that a cache entry depends on a byte range
 * 
 * @param cache Cache handle
 * @param cache_key Cache entry key
 * @param entry_type Type of cache entry
 * @param start Start byte offset
 * @param end End byte offset (exclusive)
 * @return 0 on success, -1 on error
 */
int glr_dependency_add(glr_cache_t* cache, uint64_t cache_key,
                       uint32_t entry_type, uint32_t start, uint32_t end);

/**
 * Invalidate all cache entries that depend on a byte range
 * 
 * @param cache Cache handle
 * @param start Start byte offset
 * @param end End byte offset (exclusive)
 * @return 0 on success, -1 on error
 */
int glr_dependency_invalidate_range(glr_cache_t* cache,
                                    uint32_t start, uint32_t end);

/**
 * Get all cache entries affected by a byte range edit
 * 
 * @param cache Cache handle
 * @param start Start byte offset
 * @param end End byte offset (exclusive)
 * @param out_deps Output array of dependencies (must be freed)
 * @param out_count Output count of dependencies
 * @return 0 on success, -1 on error
 */
int glr_dependency_get_affected(glr_cache_t* cache,
                                uint32_t start, uint32_t end,
                                glr_dependency_t** out_deps,
                                size_t* out_count);

/**
 * Free a dependency list returned by glr_dependency_get_affected
 * 
 * @param deps Dependency list to free
 */
void glr_dependency_free_list(glr_dependency_t* deps);

/**
 * Count dependency records tracked for a cache handle.
 *
 * @param cache Cache handle (may be NULL)
 * @return Number of tracked records, or 0 for NULL
 */
size_t glr_dependency_count(const glr_cache_t* cache);

#ifdef __cplusplus
}
#endif

#endif /* GLR_DEPENDENCY_H */
