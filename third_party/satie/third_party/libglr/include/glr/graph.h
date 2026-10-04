#ifndef GLR_GRAPH_H
#define GLR_GRAPH_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @file graph.h
 * @brief Graph operations for SPPF
 *
 * This module provides graph operations for manipulating the
 * SPPF structure, including node/edge management and traversal.
 */

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @struct glr_graph_node_t
   * @brief A node in the SPPF graph
   *
   * Graph nodes represent points in the parse forest where
   * different parse paths may converge or diverge.
   */
  typedef struct glr_graph_node
  {
    size_t id;                         ///< Unique node identifier
    void *data;                        ///< Node-specific data
    struct glr_graph_edge **edges_out; ///< Outgoing edges
    size_t edge_out_count;             ///< Number of outgoing edges
    size_t edge_out_capacity;          ///< Outgoing edge capacity
    struct glr_graph_edge **edges_in;  ///< Incoming edges
    size_t edge_in_count;              ///< Number of incoming edges
    size_t edge_in_capacity;           ///< Incoming edge capacity
  } glr_graph_node_t;

  /**
   * @struct glr_graph_edge_t
   * @brief An edge in the SPPF graph
   *
   * Edges connect nodes and represent derivation steps
   * in the parse forest.
   */
  typedef struct glr_graph_edge
  {
    glr_graph_node_t *from; ///< Source node
    glr_graph_node_t *to;   ///< Target node
    int symbol_id;          ///< Symbol this edge derives
    void *data;             ///< Edge-specific data
  } glr_graph_edge_t;

  /**
   * @struct glr_graph_t
   * @brief Graph container
   *
   * Manages all nodes and edges in a graph.
   */
  typedef struct
  {
    glr_graph_node_t **nodes; ///< All nodes
    size_t node_count;        ///< Number of nodes
    glr_graph_edge_t **edges; ///< All edges
    size_t edge_count;        ///< Number of edges
  } glr_graph_t;

  /**
   * @brief Create a new empty graph
   *
   * @return Pointer to new graph, or NULL on failure
   */
  glr_graph_t *glr_graph_create (void);

  /**
   * @brief Destroy a graph and free all associated memory
   *
   * @param graph Pointer to graph to destroy
   */
  void glr_graph_destroy (glr_graph_t *graph);

  /**
   * @brief Create a new node in the graph
   *
   * @param graph Pointer to graph
   * @param data Node data
   * @return Node ID (>= 0) on success, -1 on failure
   */
  int glr_graph_add_node (glr_graph_t *graph, void *data);

  /**
   * @brief Get a node by ID
   *
   * @param graph Pointer to graph
   * @param id Node ID
   * @return Pointer to node, or NULL if not found
   */
  glr_graph_node_t *glr_graph_get_node (glr_graph_t *graph, size_t id);

  /**
   * @brief Add an edge between two nodes
   *
   * @param graph Pointer to graph
   * @param from_id Source node ID
   * @param to_id Target node ID
   * @param symbol_id Symbol ID
   * @return Edge pointer, or NULL on failure
   */
  glr_graph_edge_t *glr_graph_add_edge (glr_graph_t *graph, size_t from_id,
                                        size_t to_id, int symbol_id);

  /**
   * @brief Get outgoing edges from a node
   *
   * @param node Pointer to node
   * @return Array of outgoing edges, or NULL if none
   */
  static inline glr_graph_edge_t **
  glr_graph_node_outgoing (glr_graph_node_t *node)
  {
    return node != NULL ? node->edges_out : NULL;
  }

  /**
   * @brief Get incoming edges to a node
   *
   * @param node Pointer to node
   * @return Array of incoming edges, or NULL if none
   */
  static inline glr_graph_edge_t **
  glr_graph_node_incoming (glr_graph_node_t *node)
  {
    return node != NULL ? node->edges_in : NULL;
  }

  /**
   * @brief Get the number of outgoing edges
   *
   * @param node Pointer to node
   * @return Number of outgoing edges
   */
  static inline size_t
  glr_graph_node_outgoing_count (glr_graph_node_t *node)
  {
    return node != NULL ? node->edge_out_count : 0;
  }

  /**
   * @brief Get the number of incoming edges
   *
   * @param node Pointer to node
   * @return Number of incoming edges
   */
  static inline size_t
  glr_graph_node_incoming_count (glr_graph_node_t *node)
  {
    return node != NULL ? node->edge_in_count : 0;
  }

  /**
   * @brief Get the number of nodes in a graph (null-safe).
   */
  static inline size_t
  glr_graph_node_count (const glr_graph_t *graph)
  {
    return graph != NULL ? graph->node_count : 0;
  }

  /**
   * @brief Get the number of edges in a graph (null-safe).
   */
  static inline size_t
  glr_graph_edge_count (const glr_graph_t *graph)
  {
    return graph != NULL ? graph->edge_count : 0;
  }

  /**
   * @brief Check whether an edge exists between two nodes.
   *
   * @param graph Graph to inspect (may be NULL)
   * @param from_id Source node id
   * @param to_id Target node id
   * @return true when at least one matching edge exists
   */
  bool glr_graph_has_edge (const glr_graph_t *graph, size_t from_id,
                           size_t to_id);

  /**
   * @brief Remove all edges between two nodes.
   *
   * @param graph Graph to update (may be NULL)
   * @param from_id Source node id
   * @param to_id Target node id
   * @return Number of removed edges, or -1 on invalid input
   */
  int glr_graph_remove_edge (glr_graph_t *graph, size_t from_id,
                             size_t to_id);

  /**
   * @brief Remove all nodes and edges without destroying the container.
   *
   * @param graph Graph to clear (may be NULL)
   */
  void glr_graph_clear (glr_graph_t *graph);

#ifdef __cplusplus
}
#endif

#endif /* GLR_GRAPH_H */
