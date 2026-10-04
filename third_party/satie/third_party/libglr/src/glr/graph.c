#include <glr/graph.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

glr_graph_t *
glr_graph_create (void)
{
  glr_graph_t *graph = calloc (1, sizeof (glr_graph_t));
  if (graph == NULL)
    {
      return NULL;
    }

  graph->nodes = NULL;
  graph->node_count = 0;
  graph->edges = NULL;
  graph->edge_count = 0;

  return graph;
}

void
glr_graph_destroy (glr_graph_t *graph)
{
  if (graph == NULL)
    {
      return;
    }

  /* Free all nodes and their edges */
  for (size_t i = 0; i < graph->node_count; i++)
    {
      glr_graph_node_t *node = graph->nodes[i];
      if (node != NULL)
        {
          free (node->edges_out);
          free (node->edges_in);
          free (node);
        }
    }
  free (graph->nodes);

  /* Free all edges */
  for (size_t i = 0; i < graph->edge_count; i++)
    {
      free (graph->edges[i]);
    }
  free (graph->edges);

  free (graph);
}

int
glr_graph_add_node (glr_graph_t *graph, void *data)
{
  if (graph == NULL)
    {
      return -1;
    }

  /* Create new node */
  glr_graph_node_t *node = calloc (1, sizeof (glr_graph_node_t));
  if (node == NULL)
    {
      return -1;
    }

  node->id = graph->node_count;
  node->data = data;
  node->edges_out = NULL;
  node->edge_out_count = 0;
  node->edge_out_capacity = 0;
  node->edges_in = NULL;
  node->edge_in_count = 0;
  node->edge_in_capacity = 0;

  /* Expand nodes array */
  glr_graph_node_t **new_nodes = realloc (
      graph->nodes, (graph->node_count + 1) * sizeof (glr_graph_node_t *));
  if (new_nodes == NULL)
    {
      free (node);
      return -1;
    }

  graph->nodes = new_nodes;
  graph->nodes[graph->node_count++] = node;

  return (int)node->id;
}

glr_graph_node_t *
glr_graph_get_node (glr_graph_t *graph, size_t id)
{
  if (graph == NULL || id >= graph->node_count)
    {
      return NULL;
    }

  return graph->nodes[id];
}

glr_graph_edge_t *
glr_graph_add_edge (glr_graph_t *graph, size_t from_id, size_t to_id,
                    int symbol_id)
{
  glr_graph_node_t *from;
  glr_graph_node_t *to;
  glr_graph_edge_t *edge;
  glr_graph_edge_t **new_edges;
  glr_graph_edge_t **new_out;
  glr_graph_edge_t **new_in;
  size_t new_cap;

  if (graph == NULL || from_id >= graph->node_count
      || to_id >= graph->node_count)
    {
      return NULL;
    }

  from = graph->nodes[from_id];
  to = graph->nodes[to_id];
  if (from == NULL || to == NULL)
    {
      return NULL;
    }

  /* Reserve node adjacency capacity before publishing the edge so a
     later allocation failure cannot leave a half-linked edge behind. */
  if (from->edge_out_count >= from->edge_out_capacity)
    {
      new_cap = from->edge_out_capacity == 0 ? 4 : from->edge_out_capacity * 2;
      new_out = realloc (from->edges_out, new_cap * sizeof (*new_out));
      if (new_out == NULL)
        {
          return NULL;
        }
      from->edge_out_capacity = new_cap;
      from->edges_out = new_out;
    }
  if (to->edge_in_count >= to->edge_in_capacity)
    {
      new_cap = to->edge_in_capacity == 0 ? 4 : to->edge_in_capacity * 2;
      new_in = realloc (to->edges_in, new_cap * sizeof (*new_in));
      if (new_in == NULL)
        {
          return NULL;
        }
      to->edge_in_capacity = new_cap;
      to->edges_in = new_in;
    }

  /* Create new edge */
  edge = calloc (1, sizeof (*edge));
  if (edge == NULL)
    {
      return NULL;
    }

  edge->from = from;
  edge->to = to;
  edge->symbol_id = symbol_id;
  edge->data = NULL;

  /* Expand edges array */
  new_edges = realloc (graph->edges,
                       (graph->edge_count + 1) * sizeof (*new_edges));
  if (new_edges == NULL)
    {
      free (edge);
      return NULL;
    }

  graph->edges = new_edges;
  graph->edges[graph->edge_count++] = edge;

  from->edges_out[from->edge_out_count++] = edge;
  to->edges_in[to->edge_in_count++] = edge;

  return edge;
}

static void
glr_graph_unlink_edge (glr_graph_edge_t **list, size_t *count,
                       glr_graph_edge_t *edge)
{
  size_t i;

  for (i = 0; i < *count; i++)
    {
      if (list[i] == edge)
        {
          memmove (&list[i], &list[i + 1],
                   (*count - i - 1) * sizeof (*list));
          (*count)--;
          return;
        }
    }
}

bool
glr_graph_has_edge (const glr_graph_t *graph, size_t from_id, size_t to_id)
{
  size_t i;

  if (graph == NULL || from_id >= graph->node_count
      || to_id >= graph->node_count)
    {
      return false;
    }

  for (i = 0; i < graph->edge_count; i++)
    {
      glr_graph_edge_t *edge = graph->edges[i];
      if (edge != NULL && edge->from == graph->nodes[from_id]
          && edge->to == graph->nodes[to_id])
        {
          return true;
        }
    }

  return false;
}

int
glr_graph_remove_edge (glr_graph_t *graph, size_t from_id, size_t to_id)
{
  size_t i;
  int removed = 0;

  if (graph == NULL || from_id >= graph->node_count
      || to_id >= graph->node_count)
    {
      return -1;
    }

  for (i = 0; i < graph->edge_count;)
    {
      glr_graph_edge_t *edge = graph->edges[i];
      if (edge != NULL && edge->from == graph->nodes[from_id]
          && edge->to == graph->nodes[to_id])
        {
          glr_graph_unlink_edge (edge->from->edges_out,
                                 &edge->from->edge_out_count, edge);
          glr_graph_unlink_edge (edge->to->edges_in,
                                 &edge->to->edge_in_count, edge);
          free (edge);
          memmove (&graph->edges[i], &graph->edges[i + 1],
                   (graph->edge_count - i - 1) * sizeof (*graph->edges));
          graph->edge_count--;
          removed++;
          continue;
        }
      i++;
    }

  return removed;
}

void
glr_graph_clear (glr_graph_t *graph)
{
  size_t i;

  if (graph == NULL)
    {
      return;
    }

  for (i = 0; i < graph->node_count; i++)
    {
      glr_graph_node_t *node = graph->nodes[i];
      if (node != NULL)
        {
          free (node->edges_out);
          node->edges_out = NULL;
          node->edge_out_count = 0;
          node->edge_out_capacity = 0;
          free (node->edges_in);
          node->edges_in = NULL;
          node->edge_in_count = 0;
          node->edge_in_capacity = 0;
          free (node);
        }
    }
  free (graph->nodes);
  graph->nodes = NULL;
  graph->node_count = 0;

  for (i = 0; i < graph->edge_count; i++)
    {
      free (graph->edges[i]);
    }
  free (graph->edges);
  graph->edges = NULL;
  graph->edge_count = 0;
}
