#include <glr/disambiguate.h>
#include <stdlib.h>

typedef struct
{
  glr_disambig_predicate_fn fn;
  glr_disambig_destroy_fn destroy;
  void *user_data;
} glr_semantic_state_t;

static void
glr_semantic_state_destroy (void *user_data)
{
  glr_semantic_state_t *state = user_data;

  if (state == NULL)
    {
      return;
    }

  if (state->destroy != NULL)
    {
      state->destroy (state->user_data);
    }

  free (state);
}

static glr_disambig_result_t
glr_semantic_hook (glr_disambig_context_t *context, size_t *winner_index,
                   void *user_data)
{
  glr_semantic_state_t *state = user_data;
  size_t i;

  for (i = 0; i < context->candidate_count; i++)
    {
      if (!glr_disambig_candidate_is_active (&context->candidates[i]))
        {
          continue;
        }

      if (!state->fn (context, &context->candidates[i], state->user_data))
        {
          glr_disambig_context_reject_candidate (context, i);
        }
    }

  if (glr_disambig_context_active_count (context) == 0)
    {
      return GLR_DISAMBIG_ERROR;
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

glr_disambig_hook_t *
glr_disambig_semantic_hook_create (const char *name, unsigned int priority,
                                   glr_disambig_predicate_fn fn,
                                   void *user_data,
                                   glr_disambig_destroy_fn destroy)
{
  glr_semantic_state_t *state;

  if (fn == NULL)
    {
      return NULL;
    }

  state = calloc (1, sizeof (*state));
  if (state == NULL)
    {
      return NULL;
    }

  state->fn = fn;
  state->destroy = destroy;
  state->user_data = user_data;

  return glr_disambig_hook_create (
      name != NULL ? name : "semantic", priority, glr_semantic_hook, state,
      glr_semantic_state_destroy);
}
