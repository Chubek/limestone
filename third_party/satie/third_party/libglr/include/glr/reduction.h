#ifndef GLR_REDUCTION_H
#define GLR_REDUCTION_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file reduction.h
 * @brief Reduction operations for GLR parsing
 *
 * This module provides reduction operations used in GLR parsing,
 * including item set management, reduction triggers, and
 * forest node creation.
 */

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @struct glr_item_t
   * @brief LR(0) item
   *
   * An item represents a production with a dot indicating
   * how much of the right-hand side has been recognized.
   */
  typedef struct
  {
    int production_id;     ///< Production ID
    size_t dot;            ///< Dot position (0 = before first symbol)
    size_t start_position; ///< Position where item was created
  } glr_item_t;

  /**
   * @struct glr_item_set_t
   * @brief Set of LR(0) items
   *
   * A collection of items representing parser state.
   */
  typedef struct
  {
    glr_item_t **items; ///< Array of items
    size_t item_count;  ///< Number of items
    size_t capacity;    ///< Item capacity
  } glr_item_set_t;

  /**
   * @struct glr_reduction_t
   * @brief A reduction operation
   *
   * Represents a complete reduction of a production at a given position.
   */
  typedef struct
  {
    int production_id; ///< Production being reduced
    size_t position;   ///< Reduction position
    void *context;     ///< Reduction context
  } glr_reduction_t;

  /**
   * @brief Create an empty item set
   *
   * @return Pointer to new item set, or NULL on failure
   */
  glr_item_set_t *glr_item_set_create (void);

  /**
   * @brief Destroy an item set
   *
   * @param set Pointer to item set
   */
  void glr_item_set_destroy (glr_item_set_t *set);

  /**
   * @brief Remove all items from a set without destroying it.
   *
   * @param set Pointer to item set (may be NULL)
   */
  void glr_item_set_clear (glr_item_set_t *set);

  /**
   * @brief Get the number of items in a set.
   *
   * @param set Pointer to item set (may be NULL)
   * @return Item count, or 0 for NULL
   */
  size_t glr_item_set_size (const glr_item_set_t *set);

  /**
   * @brief Get one item by index.
   *
   * @param set Pointer to item set
   * @param index Item index
   * @return Item pointer, or NULL on invalid input
   */
  const glr_item_t *glr_item_set_get (const glr_item_set_t *set,
                                      size_t index);

  /**
   * @brief Add an item to an item set
   *
   * @param set Pointer to item set
   * @param item Item to add
   * @return 0 on success, -1 on failure
   */
  int glr_item_set_add (glr_item_set_t *set, glr_item_t item);

  /**
   * @brief Check if an item exists in the set
   *
   * @param set Pointer to item set
   * @param item Item to search for
   * @return true if found, false otherwise
   */
  bool glr_item_set_contains (glr_item_set_t *set, glr_item_t item);

  /**
   * @brief Get the production for an item
   *
   * @param item Pointer to item
   * @return Production ID, or -1 if invalid
   */
  static inline int
  glr_item_get_production (glr_item_t *item)
  {
    return item != NULL ? item->production_id : -1;
  }

  /**
   * @brief Check if an item is a complete item (dot at end)
   *
   * @param item Pointer to item
   * @return true if complete, false otherwise
   *
   * @note This predicate only checks that the dot advanced past the
   *       start position. Use glr_item_is_complete_for_production()
   *       when the production length is known.
   */
  static inline bool
  glr_item_is_complete (glr_item_t *item)
  {
    return item != NULL && item->dot > 0;
  }

  /**
   * @brief Check completion against a known production length.
   *
   * @param item Pointer to item (may be NULL)
   * @param body_length Production right-hand-side length
   * @return true when dot >= body_length
   */
  static inline bool
  glr_item_is_complete_for_production (const glr_item_t *item,
                                       size_t body_length)
  {
    return item != NULL && item->dot >= body_length;
  }

  /**
   * @brief Create a new reduction
   *
   * @param production_id Production being reduced
   * @param position Reduction position
   * @return Pointer to reduction, or NULL on failure
   */
  glr_reduction_t *glr_reduction_create (int production_id, size_t position);

  /**
   * @brief Destroy a reduction
   *
   * @param reduction Pointer to reduction
   */
  void glr_reduction_destroy (glr_reduction_t *reduction);

  /**
   * @brief Get the production ID of a reduction
   *
   * @param reduction Pointer to reduction
   * @return Production ID, or -1 if invalid
   */
  static inline int
  glr_reduction_get_production (glr_reduction_t *reduction)
  {
    return reduction != NULL ? reduction->production_id : -1;
  }

#ifdef __cplusplus
}
#endif

#endif /* GLR_REDUCTION_H */
