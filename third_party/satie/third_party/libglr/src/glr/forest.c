#include <glr/forest.h>
#include "containers.h"

#include <klib/khash.h>
#include <string.h>

typedef glr_forest_node_t *forest_node_ptr_t;
#define T forest_node_ptr_t
#define P
#include <ctl/vec.h>

typedef struct
{
  const glr_forest_node_t *node;
  size_t depth;
} forest_frame_t;
#define T forest_frame_t
#define P
#include <ctl/vec.h>

KHASH_MAP_INIT_INT64 (forest_nodes, glr_forest_node_t *)
KHASH_SET_INIT_INT64 (forest_seen)

glr_forest_t *
glr_forest_create (void)
{
  return calloc (1, sizeof (glr_forest_t));
}

static int
collect_node (vec_forest_node_ptr_t *nodes, khash_t (forest_seen) *seen,
               glr_forest_node_t *node)
{
  int inserted;
  forest_node_ptr_t *grown;
  if (node == NULL || kh_get (forest_seen, seen, (uintptr_t) node) != kh_end (seen))
    return 0;
  grown = GLR_VECTOR_RESERVE (nodes, nodes->size + 1);
  if (grown == NULL)
    return -1;
  nodes->value = grown;
  kh_put (forest_seen, seen, (uintptr_t) node, &inserted);
  if (inserted < 0)
    return -1;
  vec_forest_node_ptr_t_push_back (nodes, node);
  return 0;
}

/* Deserialized forests can contain nodes only reachable through children.
   The same visited set handles those, ordinary sharing, and cycles. */
static int
collect_forest (const glr_forest_t *forest, vec_forest_node_ptr_t *nodes)
{
  khash_t (forest_seen) *seen = kh_init (forest_seen);
  int rc = -1;
  if (seen == NULL)
    return -1;
  for (size_t pos = 0; pos < forest->node_count; pos++)
    for (glr_forest_node_t *node = forest->nodes[pos]; node != NULL; node = node->next)
      if (collect_node (nodes, seen, node) != 0)
        goto cleanup;
  for (size_t i = 0; i < nodes->size; i++)
    for (size_t c = 0; c < nodes->value[i]->child_count; c++)
      if (collect_node (nodes, seen, nodes->value[i]->children[c]) != 0)
        goto cleanup;
  rc = 0;
cleanup:
  kh_destroy (forest_seen, seen);
  return rc;
}

void
glr_forest_node_destroy (glr_forest_node_t *node)
{
  if (node == NULL)
    return;
  free (node->children);
  free (node);
}

void
glr_forest_clear (glr_forest_t *forest)
{
  vec_forest_node_ptr_t nodes = vec_forest_node_ptr_t_init ();
  if (forest == NULL)
    return;
  collect_forest (forest, &nodes);
  for (size_t i = 0; i < nodes.size; i++)
    glr_forest_node_destroy (nodes.value[i]);
  vec_forest_node_ptr_t_free (&nodes);
  free (forest->nodes);
  for (size_t pos = 0; pos < forest->edge_count; pos++)
    {
      glr_forest_edge_t *edge = forest->edges[pos];
      while (edge != NULL)
        {
          glr_forest_edge_t *next = edge->next;
          free (edge);
          edge = next;
        }
    }
  free (forest->edges);
  memset (forest, 0, sizeof (*forest));
}

void
glr_forest_destroy (glr_forest_t *forest)
{
  if (forest != NULL)
    {
      glr_forest_clear (forest);
      free (forest);
    }
}

static int
reserve_positions (glr_forest_t *forest, size_t position)
{
  glr_forest_node_t **grown;
  if (position < forest->node_count)
    return 0;
  if (position == SIZE_MAX)
    return -1;
  grown = glr_array_grow (forest->nodes, &forest->node_capacity, position + 1,
                         sizeof (*forest->nodes));
  if (grown == NULL)
    return -1;
  memset (grown + forest->node_count, 0,
          (position + 1 - forest->node_count) * sizeof (*grown));
  forest->nodes = grown;
  forest->node_count = position + 1;
  return 0;
}

static glr_forest_node_t *
create_node (glr_forest_t *forest, glr_forest_node_type_t type, int id,
              size_t start, size_t end)
{
  glr_forest_node_t *node = calloc (1, sizeof (*node));
  if (node == NULL)
    return NULL;
  node->type = type;
  node->symbol_id = id;
  node->position = start;
  node->end_position = end;
  node->next = forest->nodes[start];
  forest->nodes[start] = node;
  return node;
}

glr_forest_node_t *
glr_forest_get_node (glr_forest_t *forest, glr_forest_node_type_t type,
                      int symbol_id, size_t position)
{
  if (forest == NULL || reserve_positions (forest, position) != 0)
    return NULL;
  for (glr_forest_node_t *node = forest->nodes[position]; node != NULL; node = node->next)
    if (node->type == type && node->symbol_id == symbol_id)
      return node;
  return create_node (forest, type, symbol_id, position, position);
}

static glr_forest_node_t *
get_packed (glr_forest_t *forest, glr_forest_node_type_t type, int id,
             size_t start, size_t end)
{
  if (forest == NULL || reserve_positions (forest, start) != 0)
    return NULL;
  if (end < start)
    end = start;
  for (glr_forest_node_t *node = forest->nodes[start]; node != NULL; node = node->next)
    if (node->type == type && node->symbol_id == id && node->end_position == end)
      return node;
  return create_node (forest, type, id, start, end);
}

glr_forest_node_t *
glr_forest_get_terminal (glr_forest_t *forest, int id, size_t start, size_t end)
{
  return get_packed (forest, GLR_NODE_TERMINAL, id, start, end);
}

glr_forest_node_t *
glr_forest_get_constructor (glr_forest_t *forest, int id, size_t start, size_t end)
{
  return get_packed (forest, GLR_NODE_CONSTRUCTOR, id, start, end);
}

glr_forest_node_t *
glr_forest_get_symbol (glr_forest_t *forest, int id, size_t start, size_t end)
{
  return get_packed (forest, GLR_NODE_NONTERMINAL, id, start, end);
}

glr_forest_node_t *
glr_forest_pack_production (glr_forest_t *forest, int id, size_t start,
                             size_t end, glr_forest_node_t *const *children,
                             size_t count)
{
  glr_forest_node_t *node;
  glr_forest_node_t **copy = NULL;
  if (forest == NULL || end < start || (count != 0 && children == NULL)
      || count > SIZE_MAX / sizeof (*copy) || reserve_positions (forest, start) != 0)
    return NULL;
  for (size_t i = 0; i < count; i++)
    if (children[i] == NULL)
      return NULL;
  for (node = forest->nodes[start]; node != NULL; node = node->next)
    if (node->type == GLR_NODE_CONSTRUCTOR && node->symbol_id == id
        && node->end_position == end && node->child_count == count
        && (count == 0 || memcmp (node->children, children, count * sizeof (*copy)) == 0))
      return node;
  if (count != 0)
    {
      copy = malloc (count * sizeof (*copy));
      if (copy == NULL)
        return NULL;
      memcpy (copy, children, count * sizeof (*copy));
    }
  node = create_node (forest, GLR_NODE_CONSTRUCTOR, id, start, end);
  if (node == NULL)
    {
      free (copy);
      return NULL;
    }
  node->children = copy;
  node->child_count = node->capacity = count;
  return node;
}

int
glr_forest_add_child (glr_forest_node_t *parent, glr_forest_node_t *child)
{
  vec_forest_node_ptr_t view = vec_forest_node_ptr_t_init ();
  forest_node_ptr_t *grown;
  if (parent == NULL || child == NULL || parent == child
      || parent->type == GLR_NODE_TERMINAL || parent->child_count == SIZE_MAX)
    return -1;
  view.value = parent->children;
  view.size = parent->child_count;
  view.capacity = parent->capacity;
  grown = GLR_VECTOR_RESERVE (&view, view.size + 1);
  if (grown == NULL)
    return -1;
  view.value = grown;
  vec_forest_node_ptr_t_push_back (&view, child);
  parent->children = view.value;
  parent->child_count = view.size;
  parent->capacity = view.capacity;
  return 0;
}

glr_forest_node_t **
glr_forest_get_children (glr_forest_node_t *node)
{
  return node != NULL && node->type != GLR_NODE_TERMINAL ? node->children : NULL;
}

int
glr_forest_add_edge (glr_forest_t *forest, glr_forest_edge_t *edge)
{
  glr_forest_edge_t *copy;
  if (forest == NULL || edge == NULL || edge->end_position == SIZE_MAX)
    return -1;
  if (edge->end_position >= forest->edge_count)
    {
      size_t count = edge->end_position + 1;
      glr_forest_edge_t **grown = glr_array_grow (forest->edges, &forest->edge_capacity,
                                                count, sizeof (*forest->edges));
      if (grown == NULL)
        return -1;
      memset (grown + forest->edge_count, 0, (count - forest->edge_count) * sizeof (*grown));
      forest->edges = grown;
      forest->edge_count = count;
    }
  copy = malloc (sizeof (*copy));
  if (copy == NULL)
    return -1;
  *copy = *edge;
  copy->next = forest->edges[edge->end_position];
  forest->edges[edge->end_position] = copy;
  return 0;
}

glr_forest_edge_t *
glr_forest_get_edges (glr_forest_t *forest, size_t position)
{
  return forest != NULL && position < forest->edge_count ? forest->edges[position] : NULL;
}

size_t
glr_forest_node_count_at (const glr_forest_t *forest, size_t position)
{
  size_t count = 0;
  if (forest != NULL && position < forest->node_count)
    for (const glr_forest_node_t *node = forest->nodes[position]; node != NULL; node = node->next)
      count++;
  return count;
}

size_t
glr_forest_total_nodes (const glr_forest_t *forest)
{
  size_t count = 0;
  if (forest != NULL)
    for (size_t i = 0; i < forest->node_count; i++)
      count += glr_forest_node_count_at (forest, i);
  return count;
}

glr_forest_t *
glr_forest_clone (const glr_forest_t *source)
{
  glr_forest_t *copy;
  vec_forest_node_ptr_t nodes = vec_forest_node_ptr_t_init ();
  khash_t (forest_nodes) *map = NULL;
  if (source == NULL)
    return NULL;
  copy = glr_forest_create ();
  map = kh_init (forest_nodes);
  if (copy == NULL || map == NULL || collect_forest (source, &nodes) != 0)
    goto fail;
  if (source->node_count != 0 && reserve_positions (copy, source->node_count - 1) != 0)
    goto fail;
  /* Register every duplicate in its position table, including detached nodes.
     Child remapping is hash-based and preserves shared/cyclic graphs. */
  for (size_t i = 0; i < nodes.size; i++)
    {
      const glr_forest_node_t *old = nodes.value[i];
      glr_forest_node_t *fresh;
      khint_t slot;
      int inserted;
      if (reserve_positions (copy, old->position) != 0)
        goto fail;
      fresh = create_node (copy, old->type, old->symbol_id, old->position, old->end_position);
      if (fresh == NULL)
        goto fail;
      if (old->child_count != 0)
        {
          fresh->children = calloc (old->child_count, sizeof (*fresh->children));
          if (fresh->children == NULL)
            goto fail;
        }
      fresh->child_count = fresh->capacity = old->child_count;
      slot = kh_put (forest_nodes, map, (uintptr_t) old, &inserted);
      if (inserted < 0)
        goto fail;
      kh_val (map, slot) = fresh;
    }
  for (size_t i = 0; i < nodes.size; i++)
    {
      const glr_forest_node_t *old = nodes.value[i];
      glr_forest_node_t *fresh = kh_val (map, kh_get (forest_nodes, map, (uintptr_t) old));
      for (size_t c = 0; c < old->child_count; c++)
        if (old->children[c] != NULL)
          fresh->children[c] = kh_val (map, kh_get (forest_nodes, map, (uintptr_t) old->children[c]));
    }
  if (source->root != NULL)
    {
      khint_t slot = kh_get (forest_nodes, map, (uintptr_t) source->root);
      if (slot != kh_end (map))
        copy->root = kh_val (map, slot);
    }
  for (size_t pos = 0; pos < source->edge_count; pos++)
    for (glr_forest_edge_t *edge = source->edges[pos]; edge != NULL; edge = edge->next)
      if (glr_forest_add_edge (copy, edge) != 0)
        goto fail;
  kh_destroy (forest_nodes, map);
  vec_forest_node_ptr_t_free (&nodes);
  return copy;
fail:
  kh_destroy (forest_nodes, map);
  vec_forest_node_ptr_t_free (&nodes);
  glr_forest_destroy (copy);
  return NULL;
}

static int
visit_enqueue (vec_forest_frame_t *queue, khash_t (forest_seen) *seen,
                 const glr_forest_node_t *node, size_t depth)
{
  forest_frame_t frame = { node, depth };
  forest_frame_t *grown;
  int inserted;
  if (node == NULL || kh_get (forest_seen, seen, (uintptr_t) node) != kh_end (seen))
    return 0;
  grown = GLR_VECTOR_RESERVE (queue, queue->size + 1);
  if (grown == NULL)
    return -1;
  queue->value = grown;
  kh_put (forest_seen, seen, (uintptr_t) node, &inserted);
  if (inserted < 0)
    return -1;
  vec_forest_frame_t_push_back (queue, frame);
  return 0;
}

size_t
glr_forest_visit (const glr_forest_t *forest, const glr_forest_node_t *root,
                   glr_forest_visit_fn visit, void *user_data)
{
  vec_forest_frame_t queue = vec_forest_frame_t_init ();
  khash_t (forest_seen) *seen;
  size_t visited = 0;
  if (visit == NULL)
    return 0;
  seen = kh_init (forest_seen);
  if (seen == NULL)
    return 0;
  if (root != NULL)
    {
      if (visit_enqueue (&queue, seen, root, 0) != 0)
        goto cleanup;
    }
  else if (forest != NULL)
    for (size_t pos = 0; pos < forest->node_count; pos++)
      for (const glr_forest_node_t *node = forest->nodes[pos]; node != NULL; node = node->next)
        if (visit_enqueue (&queue, seen, node, 0) != 0)
          goto cleanup;
  for (size_t head = 0; head < queue.size; head++)
    {
      forest_frame_t frame = queue.value[head];
      visit ((glr_forest_node_t *) frame.node, frame.depth, user_data);
      visited++;
      for (size_t c = 0; c < frame.node->child_count; c++)
        if (visit_enqueue (&queue, seen, frame.node->children[c], frame.depth + 1) != 0)
          goto cleanup;
    }
cleanup:
  kh_destroy (forest_seen, seen);
  vec_forest_frame_t_free (&queue);
  return visited;
}

static void
inspect_ambiguity (glr_forest_node_t *node, size_t depth, void *data)
{
  bool *ambiguous = data;
  (void) depth;
  if (node->type == GLR_NODE_NONTERMINAL && node->child_count > 1)
    *ambiguous = true;
}

bool
glr_forest_is_ambiguous (const glr_forest_node_t *node)
{
  bool ambiguous = false;
  glr_forest_visit (NULL, node, inspect_ambiguity, &ambiguous);
  return ambiguous;
}
