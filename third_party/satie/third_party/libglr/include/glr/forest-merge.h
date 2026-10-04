#ifndef GLR_FOREST_MERGE_H
#define GLR_FOREST_MERGE_H

#include <glr/forest.h>
#include <glr/parser.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file forest-merge.h
 * @brief Forest merging for incremental parsing
 * 
 * Provides functions to merge parse forests from unchanged regions
 * with newly parsed regions after an edit.
 */

/**
 * Merge three forests into one continuous parse
 * 
 * Combines:
 * - left: unchanged prefix parse
 * - middle: newly parsed changed region
 * - right: unchanged suffix parse
 * 
 * @param parser Parser instance (for context)
 * @param left Left (prefix) forest (can be NULL)
 * @param middle Middle (changed) forest (can be NULL)
 * @param right Right (suffix) forest (can be NULL)
 * @param out Output merged forest
 * @return 0 on success, -1 on error
 */
int glr_forest_merge(glr_parser_t* parser,
                     const glr_forest_t* left,
                     const glr_forest_t* middle,
                     const glr_forest_t* right,
                     glr_forest_t** out);

/**
 * Adjust forest node positions after an edit
 * 
 * When text is inserted or deleted, positions in the forest
 * need to be adjusted by the delta.
 * 
 * @param forest Forest to adjust
 * @param start_pos Start position of the edit
 * @param delta Position delta (positive for insertion, negative for deletion)
 * @return 0 on success, -1 on error
 */
int glr_forest_adjust_positions(glr_forest_t* forest,
                                 size_t start_pos,
                                 ssize_t delta);

#ifdef __cplusplus
}
#endif

#endif /* GLR_FOREST_MERGE_H */
