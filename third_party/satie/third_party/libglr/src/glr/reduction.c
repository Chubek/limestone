#include <glr/reduction.h>
#include <stdlib.h>
#include <string.h>

glr_item_set_t *
glr_item_set_create (void)
{
  glr_item_set_t *set = calloc (1, sizeof (glr_item_set_t));
  if (set == NULL)
    {
      return NULL;
    }

  set->capacity = 16;
  set->items = calloc (set->capacity, sizeof (glr_item_t *));
  if (set->items == NULL)
    {
      free (set);
      return NULL;
    }

  set->item_count = 0;

  return set;
}

void
glr_item_set_destroy (glr_item_set_t *set)
{
  if (set == NULL)
    {
      return;
    }

  for (size_t i = 0; i < set->item_count; i++)
    {
      free (set->items[i]);
    }
  free (set->items);
  free (set);
}

void
glr_item_set_clear (glr_item_set_t *set)
{
  size_t i;

  if (set == NULL)
    {
      return;
    }

  for (i = 0; i < set->item_count; i++)
    {
      free (set->items[i]);
      set->items[i] = NULL;
    }
  set->item_count = 0;
}

size_t
glr_item_set_size (const glr_item_set_t *set)
{
  return set != NULL ? set->item_count : 0;
}

const glr_item_t *
glr_item_set_get (const glr_item_set_t *set, size_t index)
{
  if (set == NULL || index >= set->item_count)
    {
      return NULL;
    }
  return set->items[index];
}

int
glr_item_set_add (glr_item_set_t *set, glr_item_t item)
{
  if (set == NULL)
    {
      return -1;
    }

  /* Check if item already exists */
  for (size_t i = 0; i < set->item_count; i++)
    {
      glr_item_t *existing = set->items[i];
      if (existing->production_id == item.production_id
          && existing->dot == item.dot
          && existing->start_position == item.start_position)
        {
          return 0; /* Item already exists */
        }
    }

  /* Expand capacity if needed */
  if (set->item_count >= set->capacity)
    {
      size_t new_capacity = set->capacity * 2;
      glr_item_t **new_items
          = realloc (set->items, new_capacity * sizeof (glr_item_t *));
      if (new_items == NULL)
        {
          return -1;
        }

      set->capacity = new_capacity;
      set->items = new_items;
    }

  /* Add new item */
  glr_item_t *new_item = malloc (sizeof (glr_item_t));
  if (new_item == NULL)
    {
      return -1;
    }

  *new_item = item;
  set->items[set->item_count++] = new_item;

  return 0;
}

bool
glr_item_set_contains (glr_item_set_t *set, glr_item_t item)
{
  if (set == NULL)
    {
      return false;
    }

  for (size_t i = 0; i < set->item_count; i++)
    {
      glr_item_t *existing = set->items[i];
      if (existing->production_id == item.production_id
          && existing->dot == item.dot
          && existing->start_position == item.start_position)
        {
          return true;
        }
    }

  return false;
}

glr_reduction_t *
glr_reduction_create (int production_id, size_t position)
{
  glr_reduction_t *reduction = calloc (1, sizeof (glr_reduction_t));
  if (reduction == NULL)
    {
      return NULL;
    }

  reduction->production_id = production_id;
  reduction->position = position;
  reduction->context = NULL;

  return reduction;
}

void
glr_reduction_destroy (glr_reduction_t *reduction)
{
  if (reduction == NULL)
    {
      return;
    }

  free (reduction);
}
