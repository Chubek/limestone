#include <glr/disambiguate.h>
#include <stdlib.h>

typedef struct
{
  glr_disambig_int_resolver_fn precedence_resolver;
  glr_disambig_assoc_resolver_fn associativity_resolver;
  glr_disambig_destroy_fn destroy;
  void *user_data;
} glr_associativity_state_t;

static void
glr_associativity_state_destroy (void *user_data)
{
  glr_associativity_state_t *state = user_data;

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
glr_associativity_precedence (const glr_disambig_context_t *context,
                              const glr_disambig_candidate_t *candidate,
                              glr_associativity_state_t *state)
{
  if (state->precedence_resolver != NULL)
    {
      return state->precedence_resolver (context, candidate, state->user_data);
    }

  return candidate->precedence;
}

static glr_disambig_associativity_t
glr_associativity_value (const glr_disambig_context_t *context,
                         const glr_disambig_candidate_t *candidate,
                         glr_associativity_state_t *state)
{
  if (state->associativity_resolver != NULL)
    {
      return state->associativity_resolver (context, candidate,
                                            state->user_data);
    }

  return candidate->associativity;
}

static glr_disambig_result_t
glr_associativity_hook (glr_disambig_context_t *context, size_t *winner_index,
                        void *user_data)
{
  glr_associativity_state_t *state = user_data;
  size_t best_index = SIZE_MAX;
  glr_disambig_associativity_t mode = GLR_DISAMBIG_ASSOC_NONE;
  int best_precedence = 0;
  bool have_precedence = false;
  size_t i;

  for (i = 0; i < context->candidate_count; i++)
    {
      if (!glr_disambig_candidate_is_active (&context->candidates[i]))
        {
          continue;
        }

      {
        int value = glr_associativity_precedence (context,
                                                  &context->candidates[i],
                                                  state);
        if (!have_precedence || value > best_precedence)
          {
            best_precedence = value;
            have_precedence = true;
          }
      }
    }

  if (!have_precedence)
    {
      return GLR_DISAMBIG_NO_MATCH;
    }

  for (i = 0; i < context->candidate_count; i++)
    {
      glr_disambig_candidate_t *candidate = &context->candidates[i];

      if (!glr_disambig_candidate_is_active (candidate))
        {
          continue;
        }

      if (glr_associativity_precedence (context, candidate, state)
          != best_precedence)
        {
          continue;
        }

      mode = glr_associativity_value (context, candidate, state);
      if (!candidate->has_split_position || mode == GLR_DISAMBIG_ASSOC_NONE)
        {
          continue;
        }

      if (mode == GLR_DISAMBIG_ASSOC_NONASSOC)
        {
          return GLR_DISAMBIG_ERROR;
        }

      if (best_index == SIZE_MAX)
        {
          best_index = i;
          continue;
        }

      if (mode == GLR_DISAMBIG_ASSOC_LEFT
          && candidate->split_position
                 > context->candidates[best_index].split_position)
        {
          best_index = i;
        }
      else if (mode == GLR_DISAMBIG_ASSOC_RIGHT
               && candidate->split_position
                      < context->candidates[best_index].split_position)
        {
          best_index = i;
        }
    }

  if (best_index == SIZE_MAX)
    {
      return GLR_DISAMBIG_NO_MATCH;
    }

  glr_disambig_context_select_candidate (context, best_index);
  if (winner_index != NULL)
    {
      *winner_index = best_index;
    }

  return GLR_DISAMBIG_RESOLVED;
}

glr_disambig_hook_t *
glr_disambig_associativity_hook_create (
    const char *name, unsigned int priority,
    glr_disambig_int_resolver_fn precedence_resolver,
    glr_disambig_assoc_resolver_fn associativity_resolver, void *user_data,
    glr_disambig_destroy_fn destroy)
{
  glr_associativity_state_t *state = calloc (1, sizeof (*state));

  if (state == NULL)
    {
      return NULL;
    }

  state->precedence_resolver = precedence_resolver;
  state->associativity_resolver = associativity_resolver;
  state->destroy = destroy;
  state->user_data = user_data;

  return glr_disambig_hook_create (
      name != NULL ? name : "associativity", priority,
      glr_associativity_hook, state, glr_associativity_state_destroy);
}
