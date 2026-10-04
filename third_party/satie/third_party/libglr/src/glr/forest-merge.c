#include <glr/forest-merge.h>
#include <glr/forest.h>
#include <stdlib.h>
#include <string.h>

/**
 * Merge three forests into one continuous parse.
 *
 * Each input forest is deep-copied so the merged result owns all of its
 * nodes and edges independently; the inputs remain valid and can be
 * destroyed by the caller without affecting the merge output.
 *
 * Positional layout: left occupies [0, L), middle occupies [L, L+M), and
 * right occupies [L+M, L+M+R). Node and edge positions are shifted by
 * the accumulated offset so the merged forest is continuous.
 */

typedef struct
{
  const glr_forest_node_t *old_node;
  glr_forest_node_t *new_node;
} merge_entry_t;

static glr_forest_node_t *
merge_lookup (merge_entry_t *map, size_t map_size,
              const glr_forest_node_t *old_node)
{
  size_t i;
  for (i = 0; i < map_size; i++)
    {
      if (map[i].old_node == old_node)
        {
          return map[i].new_node;
        }
    }
  return NULL;
}

static int
merge_remember (merge_entry_t **map, size_t *size, size_t *capacity,
                const glr_forest_node_t *old_node,
                glr_forest_node_t *new_node)
{
  merge_entry_t *grown;
  if (*size >= *capacity)
    {
      size_t new_cap = *capacity == 0 ? 64 : *capacity * 2;
      grown = realloc (*map, new_cap * sizeof (**map));
      if (grown == NULL)
        {
          return -1;
        }
      *map = grown;
      *capacity = new_cap;
    }
  (*map)[*size].old_node = old_node;
  (*map)[*size].new_node = new_node;
  (*size)++;
  return 0;
}

static int
merge_append_forest (glr_forest_t *result, const glr_forest_t *source,
                     size_t position_shift, merge_entry_t **map,
                     size_t *map_size, size_t *map_capacity)
{
  size_t pos;

  if (source == NULL)
    {
      return 0;
    }

  /* Ensure destination arrays are large enough. */
  if (source->node_count > 0)
    {
      size_t need = position_shift + source->node_count;
      if (need > result->node_count)
        {
          glr_forest_node_t **grown
              = realloc (result->nodes, need * sizeof (*grown));
          if (grown == NULL)
            {
              return -1;
            }
          memset (grown + result->node_count, 0,
                  (need - result->node_count) * sizeof (*grown));
          result->nodes = grown;
          result->node_count = need;
        }
    }
  if (source->edge_count > 0)
    {
      size_t need = position_shift + source->edge_count;
      if (need > result->edge_count)
        {
          glr_forest_edge_t **grown
              = realloc (result->edges, need * sizeof (*grown));
          if (grown == NULL)
            {
              return -1;
            }
          memset (grown + result->edge_count, 0,
                  (need - result->edge_count) * sizeof (*grown));
          result->edges = grown;
          result->edge_count = need;
        }
    }

  /* Duplicate nodes. */
  for (pos = 0; pos < source->node_count; pos++)
    {
      const glr_forest_node_t *node;
      size_t dest = position_shift + pos;

      for (node = source->nodes[pos]; node != NULL; node = node->next)
        {
          glr_forest_node_t *fresh = calloc (1, sizeof (*fresh));
          if (fresh == NULL)
            {
              return -1;
            }
          fresh->type = node->type;
          fresh->symbol_id = node->symbol_id;
          fresh->position = node->position + position_shift;
          fresh->end_position = node->end_position + position_shift;
          fresh->child_count = node->child_count;
          fresh->capacity = node->child_count;
          fresh->data = NULL;
          fresh->next = result->nodes[dest];
          if (node->child_count > 0)
            {
              fresh->children
                  = calloc (node->child_count, sizeof (*fresh->children));
              if (fresh->children == NULL)
                {
                  free (fresh);
                  return -1;
                }
              /* Temporarily stash originals; remapped below. */
              memcpy (fresh->children, node->children,
                      node->child_count * sizeof (*fresh->children));
            }
          result->nodes[dest] = fresh;
          if (merge_remember (map, map_size, map_capacity, node, fresh)
              != 0)
            {
              return -1;
            }
        }
    }

  /* Duplicate edges with shifted spans. */
  for (pos = 0; pos < source->edge_count; pos++)
    {
      const glr_forest_edge_t *edge;
      size_t dest = position_shift + pos;

      for (edge = source->edges[pos]; edge != NULL; edge = edge->next)
        {
          glr_forest_edge_t *fresh = calloc (1, sizeof (*fresh));
          if (fresh == NULL)
            {
              return -1;
            }
          *fresh = *edge;
          fresh->start_position += position_shift;
          fresh->end_position += position_shift;
          fresh->next = result->edges[dest];
          result->edges[dest] = fresh;
        }
    }

  return 0;
}

int glr_forest_merge(glr_parser_t* parser,
                     const glr_forest_t* left,
                     const glr_forest_t* middle,
                     const glr_forest_t* right,
                     glr_forest_t** out) {
    glr_forest_t* result;
    merge_entry_t* map = NULL;
    size_t map_size = 0;
    size_t map_capacity = 0;
    size_t offset = 0;
    size_t i;

    (void)parser;

    if (!out) return -1;

    if (!left && !middle && !right) {
        *out = glr_forest_create();
        return *out ? 0 : -1;
    }

    result = glr_forest_create();
    if (!result) return -1;

    if (left) {
        if (merge_append_forest(result, left, offset, &map, &map_size,
                                &map_capacity) != 0)
            goto fail;
        offset += left->node_count > left->edge_count ? left->node_count
                                                      : left->edge_count;
        if (left->node_count == 0 && left->edge_count == 0)
            offset += 0;
        /* Recompute offset as max extent so far. */
        offset = result->node_count > result->edge_count
                     ? result->node_count
                     : result->edge_count;
    }
    if (middle) {
        if (merge_append_forest(result, middle, offset, &map, &map_size,
                                &map_capacity) != 0)
            goto fail;
        offset = result->node_count > result->edge_count
                     ? result->node_count
                     : result->edge_count;
    }
    if (right) {
        if (merge_append_forest(result, right, offset, &map, &map_size,
                                &map_capacity) != 0)
            goto fail;
    }

    /* Remap child pointers that reference duplicated sources. */
    for (i = 0; i < map_size; i++)
      {
        /* Find the fresh node via map entry. */
        glr_forest_node_t *fresh = map[i].new_node;
        const glr_forest_node_t *old = map[i].old_node;
        size_t c;
        for (c = 0; c < fresh->child_count; c++)
          {
            glr_forest_node_t *remapped
                = merge_lookup (map, map_size, old->children[c]);
            if (remapped != NULL)
              {
                fresh->children[c] = remapped;
              }
          }
      }

    free (map);
    *out = result;
    return 0;

fail:
    free (map);
    glr_forest_destroy (result);
    return -1;
}

/**
 * Adjust forest node positions after an edit.
 *
 * All node and edge positions >= start_pos move by delta. Deletions
 * that would move a position below zero fail without mutating the
 * forest further than the already-adjusted prefix: callers should
 * treat -1 as "forest left in a partially adjusted state" only when
 * they pass inconsistent deltas, so we validate up front.
 */
int glr_forest_adjust_positions(glr_forest_t* forest,
                                 size_t start_pos,
                                 ssize_t delta) {
    size_t pos;

    if (!forest) return -1;
    if (delta == 0) return 0;

    /* Validate first so a failing call leaves the forest untouched. */
    if (delta < 0)
      {
        size_t magnitude = (size_t) (-(delta + 1)) + 1u;
        for (pos = 0; pos < forest->node_count; pos++) {
            glr_forest_node_t* node = forest->nodes[pos];
            while (node) {
                if (node->position >= start_pos
                    && node->position < magnitude) {
                    return -1;
                }
                node = node->next;
            }
        }
        for (pos = 0; pos < forest->edge_count; pos++) {
            glr_forest_edge_t* edge = forest->edges[pos];
            while (edge) {
                if (edge->start_position >= start_pos
                    && edge->start_position < magnitude) {
                    return -1;
                }
                if (edge->end_position >= start_pos
                    && edge->end_position < magnitude) {
                    return -1;
                }
                edge = edge->next;
            }
        }
      }

    for (pos = 0; pos < forest->node_count; pos++) {
        glr_forest_node_t* node = forest->nodes[pos];
        while (node) {
            if (node->position >= start_pos) {
                if (delta < 0)
                  {
                    node->position -= (size_t) (-(delta + 1)) + 1u;
                  }
                else
                  {
                    node->position += (size_t) delta;
                  }
            }
            node = node->next;
        }
    }

    for (pos = 0; pos < forest->edge_count; pos++) {
        glr_forest_edge_t* edge = forest->edges[pos];
        while (edge) {
            if (edge->start_position >= start_pos) {
                if (delta < 0)
                  {
                    edge->start_position -= (size_t) (-(delta + 1)) + 1u;
                  }
                else
                  {
                    edge->start_position += (size_t) delta;
                  }
            }
            if (edge->end_position >= start_pos) {
                if (delta < 0)
                  {
                    edge->end_position -= (size_t) (-(delta + 1)) + 1u;
                  }
                else
                  {
                    edge->end_position += (size_t) delta;
                  }
            }
            edge = edge->next;
        }
    }

    return 0;
}
