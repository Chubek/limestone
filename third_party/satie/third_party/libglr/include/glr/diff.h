#ifndef GLR_DIFF_H
#define GLR_DIFF_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file diff.h
 * @brief Text diff computation for incremental parsing
 */

/**
 * Edit operation descriptor
 */
typedef struct {
    size_t old_start;      /**< Start position in old content */
    size_t old_end;        /**< End position in old content (exclusive) */
    size_t new_start;      /**< Start position in new content */
    size_t new_end;        /**< End position in new content (exclusive) */
    bool is_insertion;     /**< True if this is a pure insertion */
    bool is_deletion;      /**< True if this is a pure deletion */
    bool is_replacement;   /**< True if this is a replacement */
} glr_edit_t;

/**
 * Compute the minimal edit between two buffers
 * 
 * @param old_content Old content buffer
 * @param old_len Length of old content
 * @param new_content New content buffer
 * @param new_len Length of new content
 * @param edit Output edit descriptor
 * @return 0 on success, -1 on error
 */
int glr_compute_edit(const char* old_content, size_t old_len,
                     const char* new_content, size_t new_len,
                     glr_edit_t* edit);

/**
 * Find longest common prefix of two strings
 * 
 * @param a First string
 * @param b Second string
 * @param len_a Length of first string
 * @param len_b Length of second string
 * @return Length of common prefix
 */
size_t glr_find_common_prefix(const char* a, const char* b,
                               size_t len_a, size_t len_b);

/**
 * Find longest common suffix of two strings
 * 
 * @param a First string
 * @param b Second string
 * @param len_a Length of first string
 * @param len_b Length of second string
 * @param prefix_len Length of common prefix (to avoid overlap)
 * @return Length of common suffix
 */
size_t glr_find_common_suffix(const char* a, const char* b,
                               size_t len_a, size_t len_b,
                               size_t prefix_len);

/**
 * Check if an edit is empty (no changes)
 * 
 * @param edit Edit descriptor
 * @return true if empty, false otherwise
 */
bool glr_edit_is_empty(const glr_edit_t* edit);

/**
 * Get the length of the old region in an edit
 * 
 * @param edit Edit descriptor
 * @return Length of old region
 */
size_t glr_edit_old_length(const glr_edit_t* edit);

/**
 * Get the length of the new region in an edit
 * 
 * @param edit Edit descriptor
 * @return Length of new region
 */
size_t glr_edit_new_length(const glr_edit_t* edit);

#ifdef __cplusplus
}
#endif

#endif /* GLR_DIFF_H */
