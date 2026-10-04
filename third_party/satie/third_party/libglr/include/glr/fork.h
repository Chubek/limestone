#ifndef GLR_FORK_H
#define GLR_FORK_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file fork.h
 * @brief Forking mechanism for GLR parser stacks
 *
 * This module provides utilities for managing forks in the DAG-based
 * stack structure. Forks represent alternative parse paths that diverge
 * from a common point.
 */

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @struct glr_fork_t
   * @brief A fork point in the stack DAG
   *
   * A fork represents a divergence point where multiple parse paths
   * share the same history up to this point, then diverge.
   */
  typedef struct glr_fork
  {
    size_t height;         ///< Height at which fork occurred
    struct glr_fork *next; ///< Next fork at same height
    void *context;         ///< Fork-specific context data
  } glr_fork_t;

  /**
   * @brief Create a new fork at the specified height
   *
   * @param height Height at which to create the fork
   * @return Pointer to new fork, or NULL on failure
   */
  glr_fork_t *glr_fork_create (size_t height);

  /**
   * @brief Destroy a fork and its associated data
   *
   * @param fork Pointer to fork to destroy
   */
  void glr_fork_destroy (glr_fork_t *fork);

  /**
   * @brief Get the next fork at the same height
   *
   * @param fork Current fork
   * @return Next fork at same height, or NULL if none
   */
  glr_fork_t *glr_fork_next (glr_fork_t *fork);

  /**
   * @brief Check if a fork has context data
   *
   * @param fork Pointer to fork
   * @return true if has context, false otherwise
   */
  static inline bool
  glr_fork_has_context (glr_fork_t *fork)
  {
    return fork != NULL && fork->context != NULL;
  }

  /**
   * @brief Set context data for a fork
   *
   * @param fork Pointer to fork
   * @param context Context data to associate
   */
  static inline void
  glr_fork_set_context (glr_fork_t *fork, void *context)
  {
    if (fork != NULL)
      {
        fork->context = context;
      }
  }

  /**
   * @brief Get context data from a fork
   *
   * @param fork Pointer to fork
   * @return Context data, or NULL if none
   */
  static inline void *
  glr_fork_get_context (glr_fork_t *fork)
  {
    return fork != NULL ? fork->context : NULL;
  }

#ifdef __cplusplus
}
#endif

#endif /* GLR_FORK_H */
