#ifndef GLR_CONTAINERS_H
#define GLR_CONTAINERS_H

#include <stdint.h>
#include <stdlib.h>

/* Reserve before calling CTL/klib's infallible push operations. Keep the old
   allocation and capacity intact if allocation fails. */
static inline void *
glr_array_grow (void *array, size_t *capacity, size_t count, size_t element_size)
{
  size_t next = *capacity;
  void *grown;
  if (count <= next)
    return array;
  if (element_size == 0 || count > SIZE_MAX / element_size)
    return NULL;
  if (next == 0)
    next = 16;
  while (next < count)
    {
      if (next > SIZE_MAX / 2)
        {
          next = count;
          break;
        }
      next *= 2;
    }
  if (next > SIZE_MAX / element_size)
    next = count;
  grown = realloc (array, next * element_size);
  if (grown != NULL)
    *capacity = next;
  return grown;
}

#define GLR_VECTOR_RESERVE(v, count) \
  glr_array_grow ((v)->value, &(v)->capacity, (count), sizeof (*(v)->value))

#endif
