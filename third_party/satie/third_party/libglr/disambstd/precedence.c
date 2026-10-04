#include <glr/disambiguate.h>
#include <stdlib.h>

typedef struct
{
  glr_disambig_int_resolver_fn resolver;
  glr_disambig_destroy_fn destroy;
  void *user_data;
} glr_precedence_state_t;

static void
glr_precedence_state_destroy (void *user_data)
{
  glr_precedence_state_t *state = user_data;

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

static int
glr_precedence_value (const glr_disambig_context_t *context,
                      const glr_disambig_candidate_t *candidate,
                      glr_precedence_state_t *state)
{
  if (state->resolver != NULL)
    {
      return state->resolver (context, candidate, state->user_data);
    }

  return candidate->precedence;
}

static glr_disambig_result_t
glr_precedence_hook (glr_disambig_context_t *context, size_t *winner_index,
                     void *user_data)
{
  glr_precedence_state_t *state = user_data;
  int best = 0;
  bool have_best = false;
  size_t i;

  for (i = 0; i < context->candidate_count; i++)
    {
      if (!glr_disambig_candidate_is_active (&context->candidates[i]))
        {
          continue;
        }

      {
        int value
            = glr_precedence_value (context, &context->candidates[i], state);
        if (!have_best || value > best)
          {
            best = value;
            have_best = true;
          }
      }
    }

  if (!have_best)
    {
      return GLR_DISAMBIG_NO_MATCH;
    }

  for (i = 0; i < context->candidate_count; i++)
    {
      if (!glr_disambig_candidate_is_active (&context->candidates[i]))
        {
          continue;
        }

      if (glr_precedence_value (context, &context->candidates[i], state) < best)
        {
          glr_disambig_context_reject_candidate (context, i);
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

glr_disambig_hook_t *
glr_disambig_precedence_hook_create (const char *name,
                                     unsigned int priority,
                                     glr_disambig_int_resolver_fn resolver,
                                     void *user_data,
                                     glr_disambig_destroy_fn destroy)
{
  glr_precedence_state_t *state = calloc (1, sizeof (*state));

  if (state == NULL)
    {
      return NULL;
    }

  state->resolver = resolver;
  state->destroy = destroy;
  state->user_data = user_data;

  return glr_disambig_hook_create (
      name != NULL ? name : "precedence", priority, glr_precedence_hook, state,
      glr_precedence_state_destroy);
}
