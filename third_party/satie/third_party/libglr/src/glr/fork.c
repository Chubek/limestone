#include <glr/fork.h>
#include <stdlib.h>

glr_fork_t *
glr_fork_create (size_t height)
{
  glr_fork_t *fork = calloc (1, sizeof (glr_fork_t));
  if (fork == NULL)
    {
      return NULL;
    }

  fork->height = height;
  fork->next = NULL;
  fork->context = NULL;

  return fork;
}

void
glr_fork_destroy (glr_fork_t *fork)
{
  if (fork == NULL)
    {
      return;
    }

  /* Destroy next forks in chain */
  if (fork->next != NULL)
    {
      glr_fork_destroy (fork->next);
    }

  free (fork);
}

glr_fork_t *
glr_fork_next (glr_fork_t *fork)
{
  return fork != NULL ? fork->next : NULL;
}
