#ifndef GLR_STACK_H
#define GLR_STACK_H

#include <glr/forest.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @file stack.h
 * @brief DAG-based stack for GLR parser
 *
 * This module provides a DAG-based stack implementation for GLR parsers.
 * The stack supports forking, which is essential for handling ambiguity
 * in GLR parsing. Each stack node can have multiple children (forks),
 * and the structure is shared across different parse paths.
 */

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @struct glr_stack_node_t
   * @brief A node in the DAG-based stack (GSS node)
   *
   * Each node represents a stack frame and can fork into multiple
   * child nodes to represent different parse paths. Nodes are heap
   * allocated and reference-counted by ownership transfer: the creator
   * owns the node until it is attached to a cache, forest, or destroyed.
   */
  typedef struct glr_stack_node glr_stack_node_t;

  /**
   * @brief Create a GSS node with a parser state and input position.
   *
   * @param state_id Parser automaton state id
   * @param position Input byte offset associated with the state
   * @return New node, or NULL on allocation failure
   */
  glr_stack_node_t *glr_stack_node_create (uint32_t state_id,
                                           uint32_t position);

  /**
   * @brief Destroy a GSS node and release its parent list (not parents).
   *
   * Parent nodes are not destroyed; only the parent pointer array is freed.
   *
   * @param node Node to destroy (may be NULL)
   */
  void glr_stack_node_destroy (glr_stack_node_t *node);

  /**
   * @brief Alias for glr_stack_node_destroy (cache layer naming).
   *
   * @param node Node to free (may be NULL)
   */
  void glr_stack_node_free (glr_stack_node_t *node);

  /**
   * @brief Attach a parent link to a GSS node.
   *
   * @param node Child node
   * @param parent Parent node (stored as a borrowed reference)
   * @return 0 on success, -1 on invalid input or allocation failure
   */
  int glr_stack_node_add_parent (glr_stack_node_t *node,
                                 glr_stack_node_t *parent);

  /**
   * @brief Get the automaton state stored in a GSS node.
   */
  uint32_t glr_stack_node_get_state (const glr_stack_node_t *node);

  /**
   * @brief Get the input position stored in a GSS node.
   */
  uint32_t glr_stack_node_get_position (const glr_stack_node_t *node);

  /**
   * @brief Get the number of parent links of a GSS node.
   */
  size_t glr_stack_node_get_parent_count (const glr_stack_node_t *node);

  /**
   * @brief Get one parent link of a GSS node.
   *
   * @param node Node to inspect
   * @param index Parent index (must be < parent count)
   * @return Parent node, or NULL on invalid input
   */
  glr_stack_node_t *glr_stack_node_get_parent (const glr_stack_node_t *node,
                                               size_t index);

  /**
   * @brief Detach the most recently attached parent (no destroy).
   *
   * Used for error-path rollback. The detached parent pointer is
   * returned to the caller through no channel; the caller must have
   * obtained it beforehand if it needs destroying.
   *
   * @param node Node to update (may be NULL)
   */
  void glr_stack_node_detach_last (glr_stack_node_t *node);

  /**
   * @brief Attach an SPPF node to a GSS node.
   *
   * The parser stores one GSS node per stack entry, pairing the automaton
   * state with the forest node produced for that prefix of the input.
   *
   * @param node GSS node
   * @param forest_node Forest node (borrowed; may be NULL)
   * @return 0 on success, -1 on invalid input
   */
  int glr_stack_node_set_forest_node (glr_stack_node_t *node,
                                      glr_forest_node_t *forest_node);

  /**
   * @brief Get the SPPF node attached to a GSS node.
   */
  glr_forest_node_t *glr_stack_node_get_forest_node (
      const glr_stack_node_t *node);

  /**
   * @brief Drop one reference to a GSS node.
   *
   * Stack entries are shared between a stack and its forks, so a node
   * popped from a stack must be released through this call; the node is
   * destroyed only when the last reference goes away.
   *
   * @param node Node to release (may be NULL)
   */
  void glr_stack_node_release (glr_stack_node_t *node);
  /**
   * @brief Deep-copy a GSS node, parents included.
   *
   * The copy owns its own parents and holds the same forest node pointer,
   * which is what makes a snapshot usable across parses: the packed forest is
   * shared, while the stack structure is not.
   *
   * @param node Node to copy (may be NULL)
   * @return New node owned by the caller, or NULL on failure / NULL input
   */
  glr_stack_node_t *glr_stack_node_copy (const glr_stack_node_t *node);



  /**
   * @brief Deep-destroy a parent-owned tree without sharing.
   *
   * Destroys the node and every parent reachable through parent links.
   * Only use when parents are exclusively owned by this tree (as
   * produced by glr_deserialize_stack_node); shared DAG parents would
   * be double-freed.
   *
   * @param node Root to destroy (may be NULL)
   */
  void glr_stack_node_destroy_tree (glr_stack_node_t *node);

  /**
   * @struct glr_stack_t
   * @brief DAG-based stack container
   *
   * Manages the stack of parse states and handles forking operations.
   */
  typedef struct
  {
    glr_stack_node_t *root; ///< Stack root node
    size_t height;          ///< Current stack height
    void **states;          ///< Array of parser states
    size_t capacity;        ///< Stack capacity
    /**
     * When true, every slot in @ref states holds a heap-allocated
     * @ref glr_stack_node_t owned (possibly shared) by this stack, so
     * glr_stack_destroy() releases them. The parser enables this; callers
     * that push plain state pointers leave it false.
     */
    bool owns_gss_entries;
  } glr_stack_t;

  /**
   * @brief Declare that this stack's slots hold owned GSS nodes.
   *
   * @param stack Stack to update
   * @param owns true when every pushed slot is a GSS node
   */
  static inline void
  glr_stack_set_gss_entries (glr_stack_t *stack, bool owns)
  {
    if (stack != NULL)
      {
        stack->owns_gss_entries = owns;
      }
  }

  /**
   * @brief Create a new empty stack
   *
   * @return Pointer to new stack, or NULL on failure
   */
  glr_stack_t *glr_stack_create (void);

  /**
   * @brief Destroy a stack and free all associated memory
   *
   * @param stack Pointer to stack to destroy
   */
  void glr_stack_destroy (glr_stack_t *stack);

  /**
   * @brief Fork the stack at the current height
   *
   * Creates a copy of the stack from the root to the specified height.
   * The original stack is not modified.
   *
   * @param stack Pointer to stack to fork
   * @param height Height at which to fork (0 = root)
   * @return Pointer to new forked stack, or NULL on failure
   */
  glr_stack_t *glr_stack_fork (glr_stack_t *stack, size_t height);

  /**
   * @brief Copy a whole parse stack, deep-copying its GSS entries.
   *
   * @param stack Stack to copy (may be NULL)
   * @return New stack owned by the caller, or NULL on failure / NULL input
   *
   * @note The copy owns fresh GSS entries but shares the forest nodes they
   *       pack, which is what makes a snapshot cheap and lets a resumed parse
   *       keep referring to the prefix it reused.
   */
  glr_stack_t *glr_stack_copy (const glr_stack_t *stack);

  int glr_stack_reset (glr_stack_t *stack);

  /**
   * @brief Const-friendly accessors, for inspecting a stack the caller does not
   *        own, such as a snapshot handed to a resume.
   * @param stack Stack to inspect (may be NULL)
   * @return Height of the stack, or 0
   */
  static inline size_t
  glr_stack_snapshot_height (const glr_stack_t *stack)
  {
    return stack != NULL ? stack->height : 0;
  }

  /**
   * @brief Read one entry of a stack the caller does not own.
   * @param stack Stack to inspect (may be NULL)
   * @param index Zero-based index from the bottom
   * @return Borrowed slot, or NULL when out of range
   */
  static inline void *
  glr_stack_snapshot_get (const glr_stack_t *stack, size_t index)
  {
    if (stack == NULL || index >= stack->height)
      {
        return NULL;
      }
    return stack->states[index];
  }

  /**
   * @brief Push a state onto the stack
   *
   * @param stack Pointer to stack
   * @param state Pointer to parser state to push
   * @return 0 on success, -1 on failure
   */
  int glr_stack_push (glr_stack_t *stack, void *state);

  /**
   * @brief Pop a state from the stack
   *
   * @param stack Pointer to stack
   * @return Popped state pointer, or NULL on failure/empty stack
   */
  void *glr_stack_pop (glr_stack_t *stack);

  /**
   * @brief Peek at the top state without removing it
   *
   * @param stack Pointer to stack
   * @return Top state pointer, or NULL if empty
   */
  void *glr_stack_peek (glr_stack_t *stack);

  /**
   * @brief Get the state at a specific height
   *
   * @param stack Pointer to stack
   * @param height Height to get state from (0 = bottom)
   * @return State pointer, or NULL if invalid height
   */
  void *glr_stack_get (glr_stack_t *stack, size_t height);

  /**
   * @brief Get the current stack height
   *
   * @param stack Pointer to stack
   * @return Current height
   */
  size_t glr_stack_height (glr_stack_t *stack);

  /**
   * @brief Check if the stack is empty
   *
   * @param stack Pointer to stack
   * @return true if empty, false otherwise
   */
  static inline bool
  glr_stack_empty (glr_stack_t *stack)
  {
    return stack == NULL || stack->height == 0;
  }

  /**
   * @brief Check if the stack is full
   *
   * @param stack Pointer to stack
   * @return true if at capacity, false otherwise
   */
  static inline bool
  glr_stack_full (glr_stack_t *stack)
  {
    return stack != NULL && stack->height >= stack->capacity;
  }

  /**
   * @brief Remove all states from a stack without destroying it.
   *
   * @param stack Pointer to stack
   */
  static inline void
  glr_stack_clear (glr_stack_t *stack)
  {
    if (stack != NULL)
      {
        glr_stack_reset (stack);
      }
  }

  /**
   * @brief Check whether a stack is empty (null-safe alias).
   *
   * @param stack Pointer to stack
   * @return true if NULL or height == 0
   */
  static inline bool
  glr_stack_is_empty (const glr_stack_t *stack)
  {
    return stack == NULL || stack->height == 0;
  }

  /**
   * @brief Get the number of GSS nodes logically held by a stack.
   *
   * For the array-backed stack this equals the current height.
   *
   * @param stack Pointer to stack
   * @return Height, or 0 for NULL
   */
  static inline size_t
  glr_stack_get_node_count (const glr_stack_t *stack)
  {
    return stack != NULL ? stack->height : 0;
  }

  /**
   * @brief Clear a stack and reset its height (explicit function form).
   *
   * @param stack Pointer to stack
   * @return 0 on success, -1 if stack is NULL
   */
  int glr_stack_reset (glr_stack_t *stack);

#ifdef __cplusplus
}
#endif

#endif /* GLR_STACK_H */
