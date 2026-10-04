#include <glr/stack.h>
#include "containers.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef void *stack_slot_t;
#define T stack_slot_t
#define P
#include <ctl/vec.h>
typedef glr_stack_node_t *stack_parent_t;
#define T stack_parent_t
#define P
#include <ctl/vec.h>

struct glr_stack_node
{
  uint32_t state_id;
  uint32_t position;
  struct glr_stack_node **parents;
  size_t parent_count;
  size_t parent_capacity;
  glr_forest_node_t *forest_node;
  atomic_size_t refcount; /* references held by independent stacks */
  void *state;
  size_t height;
  struct glr_stack_node *parent;
  struct glr_stack_node *child;
  struct glr_stack_node *sibling;
};

glr_stack_t *
glr_stack_create (void)
{
  glr_stack_t *stack = calloc (1, sizeof (glr_stack_t));
  if (stack == NULL)
    {
      return NULL;
    }

  stack->capacity = 16;
  stack->states = calloc (stack->capacity, sizeof (void *));
  if (stack->states == NULL)
    {
      free (stack);
      return NULL;
    }

  stack->height = 0;
  stack->root = NULL;

  return stack;
}

void
glr_stack_destroy (glr_stack_t *stack)
{
  size_t i;

  if (stack == NULL)
    {
      return;
    }

  /* Only stacks declared as GSS-owning hold heap nodes in their slots;
     plain state pointers must never be dereferenced. Shared entries are
     released when the last referencing stack goes away. */
  if (stack->owns_gss_entries)
    {
      for (i = 0; i < stack->height; i++)
        {
          glr_stack_node_t *entry = (glr_stack_node_t *) stack->states[i];
          if (entry == NULL)
            {
              continue;
            }
          glr_stack_node_release (entry);
        }
    }

  free (stack->states);
  free (stack);
}

glr_stack_t *
glr_stack_fork (glr_stack_t *stack, size_t height)
{
  if (stack == NULL || height > stack->height)
    {
      return NULL;
    }

  /* Create new stack */
  glr_stack_t *fork = calloc (1, sizeof (glr_stack_t));
  if (fork == NULL)
    {
      return NULL;
    }

  fork->capacity = stack->capacity;
  fork->states = malloc (fork->capacity * sizeof (void *));
  if (fork->states == NULL)
    {
      free (fork);
      return NULL;
    }

  /* Copy states up to fork height. GSS entries are shared between the
     fork and its parent, so their reference counts are raised. */
  fork->height = height;
  fork->owns_gss_entries = stack->owns_gss_entries;
  memcpy (fork->states, stack->states, height * sizeof (void *));
  if (fork->owns_gss_entries)
    {
      for (size_t i = 0; i < height; i++)
        {
          glr_stack_node_t *entry = (glr_stack_node_t *) fork->states[i];
          if (entry != NULL)
            {
              atomic_fetch_add_explicit (&entry->refcount, 1, memory_order_relaxed);
            }
        }
    }

  return fork;
}

int
glr_stack_push (glr_stack_t *stack, void *state)
{
  vec_stack_slot_t view = vec_stack_slot_t_init ();
  stack_slot_t *grown;

  if (stack == NULL)
    {
      return -1;
    }

  if (stack->height == SIZE_MAX)
    return -1;
  view.value = stack->states;
  view.size = stack->height;
  view.capacity = stack->capacity;
  grown = GLR_VECTOR_RESERVE (&view, view.size + 1);
  if (grown == NULL)
    return -1;
  view.value = grown;
  vec_stack_slot_t_push_back (&view, state);
  stack->states = view.value;
  stack->height = view.size;
  stack->capacity = view.capacity;

  return 0;
}

void *
glr_stack_pop (glr_stack_t *stack)
{
  if (stack == NULL || stack->height == 0)
    {
      return NULL;
    }

  stack->height--;
  void *state = stack->states[stack->height];
  stack->states[stack->height] = NULL;

  return state;
}

void *
glr_stack_peek (glr_stack_t *stack)
{
  if (stack == NULL || stack->height == 0)
    {
      return NULL;
    }

  return stack->states[stack->height - 1];
}

void *
glr_stack_get (glr_stack_t *stack, size_t height)
{
  if (stack == NULL || height >= stack->height)
    {
      return NULL;
    }

  return stack->states[height];
}

size_t
glr_stack_height (glr_stack_t *stack)
{
  return stack != NULL ? stack->height : 0;
}

int
glr_stack_reset (glr_stack_t *stack)
{
  if (stack == NULL)
    {
      return -1;
    }
  for (size_t i = 0; i < stack->height; i++)
    {
      if (stack->owns_gss_entries)
        glr_stack_node_release (stack->states[i]);
      stack->states[i] = NULL;
    }
  stack->height = 0;
  return 0;
}

glr_stack_node_t *
glr_stack_node_create (uint32_t state_id, uint32_t position)
{
  glr_stack_node_t *node = calloc (1, sizeof (*node));
  if (node == NULL)
    {
      return NULL;
    }
  node->state_id = state_id;
  node->position = position;
  node->parents = NULL;
  node->parent_count = 0;
  node->parent_capacity = 0;
  node->forest_node = NULL;
  atomic_init (&node->refcount, 1);
  node->state = NULL;
  node->height = 0;
  node->parent = NULL;
  node->child = NULL;
  node->sibling = NULL;
  return node;
}

void
glr_stack_node_destroy (glr_stack_node_t *node)
{
  if (node == NULL)
    {
      return;
    }
  free (node->parents);
  free (node);
}

void
glr_stack_node_free (glr_stack_node_t *node)
{
  glr_stack_node_destroy (node);
}

int
glr_stack_node_add_parent (glr_stack_node_t *node, glr_stack_node_t *parent)
{
  vec_stack_parent_t view = vec_stack_parent_t_init ();
  stack_parent_t *grown;

  if (node == NULL || parent == NULL)
    {
      return -1;
    }
  if (node->parent_count == SIZE_MAX)
    return -1;
  view.value = node->parents;
  view.size = node->parent_count;
  view.capacity = node->parent_capacity;
  grown = GLR_VECTOR_RESERVE (&view, view.size + 1);
  if (grown == NULL)
    return -1;
  view.value = grown;
  vec_stack_parent_t_push_back (&view, parent);
  node->parents = view.value;
  node->parent_count = view.size;
  node->parent_capacity = view.capacity;
  return 0;
}

uint32_t
glr_stack_node_get_state (const glr_stack_node_t *node)
{
  return node != NULL ? node->state_id : 0;
}

uint32_t
glr_stack_node_get_position (const glr_stack_node_t *node)
{
  return node != NULL ? node->position : 0;
}

size_t
glr_stack_node_get_parent_count (const glr_stack_node_t *node)
{
  return node != NULL ? node->parent_count : 0;
}

glr_stack_node_t *
glr_stack_node_get_parent (const glr_stack_node_t *node, size_t index)
{
  if (node == NULL || index >= node->parent_count)
    {
      return NULL;
    }
  return node->parents[index];
}

int
glr_stack_node_set_forest_node (glr_stack_node_t *node,
                                glr_forest_node_t *forest_node)
{
  if (node == NULL)
    {
      return -1;
    }
  node->forest_node = forest_node;
  return 0;
}

glr_forest_node_t *
glr_stack_node_get_forest_node (const glr_stack_node_t *node)
{
  return node != NULL ? node->forest_node : NULL;
}

glr_stack_t *
glr_stack_copy (const glr_stack_t *stack)
{
  glr_stack_t *copy;

  if (stack == NULL)
    {
      return NULL;
    }

  copy = glr_stack_create ();
  if (copy == NULL)
    {
      return NULL;
    }

  copy->owns_gss_entries = stack->owns_gss_entries;

  for (size_t i = 0; i < stack->height; i++)
    {
      glr_stack_node_t *entry_copy = NULL;

      if (stack->owns_gss_entries)
        {
          entry_copy = glr_stack_node_copy (
              (const glr_stack_node_t *) stack->states[i]);
          if (entry_copy == NULL)
            {
              glr_stack_destroy (copy);
              return NULL;
            }
        }
      if (glr_stack_push (copy, entry_copy != NULL ? (void *) entry_copy
                                                  : stack->states[i])
          != 0)
        {
          glr_stack_node_destroy (entry_copy);
          glr_stack_destroy (copy);
          return NULL;
        }
    }

  copy->owns_gss_entries = stack->owns_gss_entries;
  return copy;
}

glr_stack_node_t *
glr_stack_node_copy (const glr_stack_node_t *node)
{
  glr_stack_node_t *copy;

  if (node == NULL)
    {
      return NULL;
    }

  copy = glr_stack_node_create (node->state_id, node->position);
  if (copy == NULL)
    {
      return NULL;
    }
  copy->forest_node = node->forest_node;
  copy->state = node->state;
  copy->height = node->height;
  copy->parent = node->parent;
  copy->child = node->child;
  copy->sibling = node->sibling;

  if (node->parent_count > 0)
    {
      copy->parents = malloc (node->parent_count * sizeof (*copy->parents));
      if (copy->parents == NULL)
        {
          glr_stack_node_destroy (copy);
          return NULL;
        }
      memcpy (copy->parents, node->parents,
              node->parent_count * sizeof (*copy->parents));
      copy->parent_count = node->parent_count;
      copy->parent_capacity = node->parent_count;
    }

  return copy;
}

void
glr_stack_node_release (glr_stack_node_t *node)
{
  if (node == NULL)
    {
      return;
    }
  if (atomic_fetch_sub_explicit (&node->refcount, 1, memory_order_acq_rel) == 1)
    glr_stack_node_destroy (node);
}

void
glr_stack_node_detach_last (glr_stack_node_t *node)
{
  if (node == NULL || node->parent_count == 0)
    {
      return;
    }
  node->parent_count--;
  node->parents[node->parent_count] = NULL;
}

void
glr_stack_node_destroy_tree (glr_stack_node_t *node)
{
  size_t i;

  if (node == NULL)
    {
      return;
    }

  for (i = 0; i < node->parent_count; i++)
    {
      glr_stack_node_destroy_tree (node->parents[i]);
    }
  free (node->parents);
  free (node);
}
