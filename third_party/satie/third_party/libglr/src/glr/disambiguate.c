#include <glr/disambiguate.h>
#include <glr/parser.h>
#include <stdlib.h>
#include <string.h>

static char *
glr_disambig_strdup (const char *text)
{
  size_t length;
  char *copy;

  if (text == NULL)
    {
      return NULL;
    }

  length = strlen (text);
  copy = malloc (length + 1);
  if (copy == NULL)
    {
      return NULL;
    }

  memcpy (copy, text, length + 1);
  return copy;
}

glr_disambig_hook_t *
glr_disambig_hook_create (const char *name, unsigned int priority,
                          glr_disambig_fn fn, void *user_data,
                          glr_disambig_destroy_fn destroy)
{
  glr_disambig_hook_t *hook;

  if (fn == NULL)
    {
      return NULL;
    }

  hook = calloc (1, sizeof (glr_disambig_hook_t));
  if (hook == NULL)
    {
      return NULL;
    }

  hook->name = glr_disambig_strdup (name);
  hook->priority = priority;
  hook->fn = fn;
  hook->destroy = destroy;
  hook->user_data = user_data;

  return hook;
}

void
glr_disambig_hook_destroy (glr_disambig_hook_t *hook)
{
  if (hook == NULL)
    {
      return;
    }

  if (hook->destroy != NULL)
    {
      hook->destroy (hook->user_data);
    }

  free (hook->name);
  free (hook);
}

int
glr_parser_add_disambiguator (glr_parser_t *parser, glr_disambig_hook_t *hook)
{
  glr_disambig_hook_t **cursor;

  if (parser == NULL || hook == NULL)
    {
      return -1;
    }

  cursor = &parser->disambig_hooks;
  while (*cursor != NULL && (*cursor)->priority >= hook->priority)
    {
      cursor = &(*cursor)->next;
    }

  hook->next = *cursor;
  *cursor = hook;

  return 0;
}

void
glr_parser_clear_disambiguators (glr_parser_t *parser)
{
  glr_disambig_hook_t *hook;

  if (parser == NULL)
    {
      return;
    }

  hook = parser->disambig_hooks;
  parser->disambig_hooks = NULL;

  while (hook != NULL)
    {
      glr_disambig_hook_t *next = hook->next;
      glr_disambig_hook_destroy (hook);
      hook = next;
    }
}

size_t
glr_disambig_context_active_count (const glr_disambig_context_t *context)
{
  size_t count;
  size_t i;

  if (context == NULL || context->candidates == NULL)
    {
      return 0;
    }

  count = 0;
  for (i = 0; i < context->candidate_count; i++)
    {
      if (!context->candidates[i].rejected)
        {
          count++;
        }
    }

  return count;
}

size_t
glr_disambig_context_last_active (const glr_disambig_context_t *context)
{
  size_t i;
  size_t last = SIZE_MAX;

  if (context == NULL || context->candidates == NULL)
    {
      return SIZE_MAX;
    }

  for (i = 0; i < context->candidate_count; i++)
    {
      if (!context->candidates[i].rejected)
        {
          last = i;
        }
    }

  return last;
}

size_t
glr_disambig_context_first_active (const glr_disambig_context_t *context)
{
  size_t i;

  if (context == NULL || context->candidates == NULL)
    {
      return SIZE_MAX;
    }

  for (i = 0; i < context->candidate_count; i++)
    {
      if (!context->candidates[i].rejected)
        {
          return i;
        }
    }

  return SIZE_MAX;
}

size_t
glr_parser_disambiguator_count (const glr_parser_t *parser)
{
  const glr_disambig_hook_t *hook;
  size_t count = 0;

  if (parser == NULL)
    {
      return 0;
    }

  for (hook = parser->disambig_hooks; hook != NULL; hook = hook->next)
    {
      count++;
    }

  return count;
}

int
glr_parser_remove_disambiguator (glr_parser_t *parser, const char *name)
{
  glr_disambig_hook_t **slot;

  if (parser == NULL || name == NULL)
    {
      return -1;
    }

  for (slot = &parser->disambig_hooks; *slot != NULL; slot = &(*slot)->next)
    {
      const char *hook_name = (*slot)->name;
      bool match = hook_name != NULL ? strcmp (hook_name, name) == 0
                                     : name[0] == '\0';
      if (match)
        {
          glr_disambig_hook_t *victim = *slot;
          *slot = victim->next;
          victim->next = NULL;
          glr_disambig_hook_destroy (victim);
          return 0;
        }
    }

  return -1;
}

int
glr_disambig_context_reject_candidate (glr_disambig_context_t *context,
                                       size_t index)
{
  if (context == NULL || context->candidates == NULL
      || index >= context->candidate_count)
    {
      return -1;
    }

  context->candidates[index].rejected = true;
  return 0;
}

int
glr_disambig_context_select_candidate (glr_disambig_context_t *context,
                                       size_t index)
{
  size_t i;

  if (context == NULL || context->candidates == NULL
      || index >= context->candidate_count)
    {
      return -1;
    }

  for (i = 0; i < context->candidate_count; i++)
    {
      context->candidates[i].rejected = i != index;
    }

  return 0;
}

glr_disambig_result_t
glr_parser_run_disambiguators (glr_parser_t *parser,
                               glr_disambig_context_t *context,
                               size_t *winner_index)
{
  glr_disambig_hook_t *hook;

  if (winner_index != NULL)
    {
      *winner_index = SIZE_MAX;
    }

  if (parser == NULL || context == NULL)
    {
      return GLR_DISAMBIG_ERROR;
    }

  if (glr_disambig_context_active_count (context) == 0)
    {
      return GLR_DISAMBIG_NO_MATCH;
    }

  if (glr_disambig_context_active_count (context) == 1)
    {
      if (winner_index != NULL)
        {
          *winner_index = glr_disambig_context_last_active (context);
        }
      return GLR_DISAMBIG_RESOLVED;
    }

  for (hook = parser->disambig_hooks; hook != NULL; hook = hook->next)
    {
      size_t hook_winner = SIZE_MAX;
      glr_disambig_result_t result
          = hook->fn (context, &hook_winner, hook->user_data);

      if (result == GLR_DISAMBIG_ERROR)
        {
          return result;
        }

      if (result == GLR_DISAMBIG_RESOLVED
          || glr_disambig_context_active_count (context) == 1)
        {
          if (hook_winner == SIZE_MAX)
            {
              hook_winner = glr_disambig_context_last_active (context);
            }

          if (winner_index != NULL)
            {
              *winner_index = hook_winner;
            }

          return GLR_DISAMBIG_RESOLVED;
        }
    }

  if (glr_disambig_context_active_count (context) == 1)
    {
      if (winner_index != NULL)
        {
          *winner_index = glr_disambig_context_last_active (context);
        }
      return GLR_DISAMBIG_RESOLVED;
    }

  return GLR_DISAMBIG_NO_MATCH;
}
