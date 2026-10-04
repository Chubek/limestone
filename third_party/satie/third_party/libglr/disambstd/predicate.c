#include <glr/disambiguate.h>
#include <stdlib.h>

typedef struct
{
  glr_disambig_predicate_fn fn;
  glr_disambig_destroy_fn destroy;
  void *user_data;
} glr_predicate_state_t;

static void
glr_predicate_state_destroy (void *user_data)
{
  glr_predicate_state_t *state = user_data;

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
glr_predicate_hook (glr_disambig_context_t *context, size_t *winner_index,
                    void *user_data)
{
  glr_predicate_state_t *state = user_data;
  size_t i;
  bool changed = false;

  for (i = 0; i < context->candidate_count; i++)
    {
      if (!glr_disambig_candidate_is_active (&context->candidates[i]))
        {
          continue;
        }

      if (!state->fn (context, &context->candidates[i], state->user_data))
        {
          glr_disambig_context_reject_candidate (context, i);
          changed = true;
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

  return changed ? GLR_DISAMBIG_NO_MATCH : GLR_DISAMBIG_NO_MATCH;
}

glr_disambig_hook_t *
glr_disambig_predicate_hook_create (const char *name, unsigned int priority,
                                    glr_disambig_predicate_fn fn,
                                    void *user_data,
                                    glr_disambig_destroy_fn destroy)
{
  glr_predicate_state_t *state;

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
      name != NULL ? name : "predicate", priority, glr_predicate_hook, state,
      glr_predicate_state_destroy);
}
