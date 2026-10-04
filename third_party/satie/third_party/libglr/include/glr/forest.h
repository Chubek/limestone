#ifndef GLR_FOREST_H
#define GLR_FOREST_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file forest.h
 * @brief SPPF (Shared Parse Forest) data structure
 *
 * This module provides the SPPF implementation for GLR parsers.
 * The SPPF is a compact representation of all parse trees,
 * sharing common substructures to handle ambiguity efficiently.
 */

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @typedef glr_forest_node_type_t
   * @brief Type of SPPF node
   */
  typedef enum
  {
    GLR_NODE_TERMINAL,    ///< Terminal node
    GLR_NODE_NONTERMINAL, ///< Non-terminal (split) node
    GLR_NODE_CONSTRUCTOR  ///< Constructor node
  } glr_forest_node_type_t;

  /**
   * @struct glr_forest_node_t
   * @brief A node in the SPPF
   *
   * Nodes represent either terminal symbols, non-terminal splits,
   * or constructor nodes that combine child nodes.
   */
  typedef struct glr_forest_node
  {
    glr_forest_node_type_t type; ///< Node type
    int symbol_id;               ///< Symbol ID (terminal or non-terminal)
    size_t position;             ///< Input position where the node begins
    size_t end_position;         ///< Input position just past the node
    struct glr_forest_node **children; ///< Child nodes (for non-terminals)
    size_t child_count;                ///< Number of children
    size_t capacity;                   ///< Child capacity
    void *data;                        ///< Node-specific data
    struct glr_forest_node *next;      ///< Next sibling in same position
  } glr_forest_node_t;

  /**
   * @struct glr_forest_edge_t
   * @brief An edge in the SPPF
   *
   * Edges connect nodes across different positions in the input.
   */
  typedef struct glr_forest_edge
  {
    int nonterminal_id;           ///< Non-terminal on left-hand side
    size_t start_position;        ///< Starting position
    size_t end_position;          ///< Ending position
    struct glr_forest_edge *next; ///< Next edge at same position
  } glr_forest_edge_t;

  /**
   * @struct glr_forest_t
   * @brief Complete SPPF container
   *
   * Manages all nodes and edges in the shared parse forest.
   */
  typedef struct
  {
    glr_forest_node_t **nodes; ///< All nodes indexed by position
    size_t node_count;         ///< Number of positions
    glr_forest_edge_t **edges; ///< All edges indexed by position
    size_t edge_count;         ///< Number of positions with edges
    /**
     * Node produced by the final reduction of an accepted parse, or NULL
     * when the parse failed or the forest was built by hand. The node
     * spans the whole input, so it is the entry point for tree walks.
     */
    glr_forest_node_t *root;
    size_t node_capacity; ///< Internal position-vector capacity
    size_t edge_capacity; ///< Internal edge-vector capacity
  } glr_forest_t;

  /**
   * @brief Create a new empty forest
   *
   * @return Pointer to new forest, or NULL on failure
   */
  glr_forest_t *glr_forest_create (void);

  /**
   * @brief Destroy a forest and free all associated memory
   *
   * @param forest Pointer to forest to destroy
   */
  void glr_forest_destroy (glr_forest_t *forest);

  /**
   * @brief Get or create a node at a specific position
   *
   * @param forest Pointer to forest
   * @param type Node type
   * @param symbol_id Symbol ID
   * @param position Input position
   * @return Pointer to node, or NULL on failure
   */
  glr_forest_node_t *glr_forest_get_node (glr_forest_t *forest,
                                          glr_forest_node_type_t type,
                                          int symbol_id, size_t position);

  /**
   * @brief Destroy a single node without touching its children.
   *
   * The packed forest owns every node, so a node is normally released with its
   * forest. Pruning is the exception: an incremental update invalidates a
   * range of nodes while the rest of the forest stays valid, and the children
   * of a pruned node are forest nodes in their own right.
   *
   * @param node Node to destroy (NULL is a no-op)
   */
  void glr_forest_node_destroy (glr_forest_node_t *node);

  /**
   * @brief Add a child to a non-terminal node
   *
   * @param parent Parent node
   * @param child Child node
   * @return 0 on success, -1 on failure
   */
  int glr_forest_add_child (glr_forest_node_t *parent,
                            glr_forest_node_t *child);

  /**
   * @brief Get children of a non-terminal node
   *
   * @param node Non-terminal node
   * @return Array of children, or NULL if invalid
   */
  glr_forest_node_t **glr_forest_get_children (glr_forest_node_t *node);

  /**
   * @brief Add an edge to the forest
   *
   * @param forest Pointer to forest
   * @param edge Edge to add; the forest stores an internal copy
   * @return 0 on success, -1 on failure
   */
  int glr_forest_add_edge (glr_forest_t *forest, glr_forest_edge_t *edge);

  /**
   * @brief Get edges at a specific position
   *
   * @param forest Pointer to forest
   * @param position Input position
   * @return List of edges at position, or NULL
   */
  glr_forest_edge_t *glr_forest_get_edges (glr_forest_t *forest,
                                           size_t position);

  /**
   * @brief Count nodes stored at one position (sibling chain length).
   *
   * @param forest Forest to inspect (may be NULL)
   * @param position Position index
   * @return Chain length, or 0 on invalid input
   */
  size_t glr_forest_node_count_at (const glr_forest_t *forest,
                                   size_t position);

  /**
   * @brief Count all nodes in a forest across every position.
   *
   * @param forest Forest to inspect (may be NULL)
   * @return Total node count
   */
  size_t glr_forest_total_nodes (const glr_forest_t *forest);

  /**
   * @brief Deep-copy a forest.
   *
   * Every node object is duplicated and child pointers are remapped to
   * the copies, so the clone can be destroyed independently of the
   * source. Edge lists are duplicated as value copies.
   *
   * @param source Forest to copy (may be NULL)
   * @return New forest, or NULL on invalid input / allocation failure
   */
  glr_forest_t *glr_forest_clone (const glr_forest_t *source);

  /**
   * @brief Remove all nodes and edges without destroying the container.
   *
   * @param forest Forest to clear (may be NULL)
   */
  void glr_forest_clear (glr_forest_t *forest);

  /**
   * @brief Callback invoked once per node by glr_forest_visit.
   *
   * @param node Node being visited
   * @param depth Distance from the traversal root
   * @param user_data Caller context
   */
  typedef void (*glr_forest_visit_fn) (glr_forest_node_t *node, size_t depth,
                                       void *user_data);

  /**
   * @brief Walk a forest breadth-first without revisiting shared nodes.
   *
   * The forest is a directed acyclic graph, so a node reached through two
   * derivations is visited once. When @p root is NULL the traversal starts
   * at every node stored in the forest.
   *
   * @param forest Forest to walk (may be NULL)
   * @param root Start node, or NULL to start from the whole forest
   * @param visit Callback invoked per node
   * @param user_data Context passed to @p visit
   * @return Number of nodes visited
   */
  size_t glr_forest_visit (const glr_forest_t *forest,
                           const glr_forest_node_t *root,
                           glr_forest_visit_fn visit, void *user_data);

  /**
   * @brief Get or create a constructor node for one reduction.
   *
   * SPPF packing for a reduction must key on the whole span, not just the
   * start: a left-recursive rule such as `E -> E + T` reduces twice over the
   * same start position with different end positions, and those are two
   * distinct derivations. Only (production, start, end) keeps them apart and
   * guarantees a node never becomes its own descendant.
   *
   * @param forest Forest to update
   * @param production_id Production that was reduced
   * @param start Span start in the input
   * @param end Span end in the input
   * @return Packed node, or NULL on allocation failure
   */
  glr_forest_node_t *glr_forest_get_constructor (glr_forest_t *forest,
                                                int production_id,
                                                size_t start, size_t end);

  /**
   * @brief Get or create the symbol node for a non-terminal occurrence.
   *
   * The counterpart of glr_forest_get_constructor: a symbol node identifies
   * one non-terminal over one span, and the constructor nodes packed under it
   * are the alternative ways to derive that span. Keying on the full span
   * matters for left recursion, where the same non-terminal occurs at the
   * same start with different ends and those are *not* alternatives.
   *
   * @param forest Forest to update
   * @param nonterminal_id Non-terminal symbol id
   * @param start Span start in the input
   * @param end Span end in the input
   * @return Packed symbol node, or NULL on allocation failure
   */
  glr_forest_node_t *glr_forest_get_symbol (glr_forest_t *forest,
                                             int nonterminal_id, size_t start,
                                             size_t end);

  /** Pack a terminal by its complete byte span. */
  glr_forest_node_t *glr_forest_get_terminal (glr_forest_t *forest,
                                             int terminal_id, size_t start,
                                             size_t end);

  /** Pack a production by its span AND ordered body children. Distinct splits
      of the same production over the same span remain distinct alternatives.
      Repeated/empty body positions are preserved, rather than deduplicated. */
  glr_forest_node_t *glr_forest_pack_production (glr_forest_t *forest,
                                                int production_id, size_t start,
                                                size_t end,
                                                glr_forest_node_t *const *children,
                                                size_t child_count);

  /**
   * @brief Report whether a forest encodes more than one derivation.
   *
   * The SPPF is a packed graph, not a tree: sub-parses are shared, and a
   * left-recursive rule such as `E -> E + T` makes the graph cyclic, since
   * the symbol node for a non-terminal occurrence links back to itself
   * through a constructor over a longer span. That sharing is exactly what
   * ambiguity looks like, and it is why this predicate exists instead of a
   * derivation count: enumerating the trees of a cyclic packed forest means
   * unrolling the cycles, which is a separate algorithm.
   *
   * A forest is ambiguous when some symbol node has more than one packed
   * constructor, or when any sub-forest reachable from it is ambiguous.
   *
   * @param node Node to inspect (may be NULL)
   * @return true when more than one derivation is encoded
   */
  bool glr_forest_is_ambiguous (const glr_forest_node_t *node);

  /**
   * @brief Check if a node is a terminal
   *
   * @param node Pointer to node
   * @return true if terminal, false otherwise
   */
  static inline bool
  glr_forest_node_is_terminal (glr_forest_node_t *node)
  {
    return node != NULL && node->type == GLR_NODE_TERMINAL;
  }

  /**
   * @brief Check if a node is a non-terminal
   *
   * @param node Pointer to node
   * @return true if non-terminal, false otherwise
   */
  static inline bool
  glr_forest_node_is_nonterminal (glr_forest_node_t *node)
  {
    return node != NULL && node->type == GLR_NODE_NONTERMINAL;
  }

#ifdef __cplusplus
}
#endif

#endif /* GLR_FOREST_H */
