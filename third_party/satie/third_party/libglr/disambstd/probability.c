#include <glr/disambiguate.h>
#include <float.h>
#include <stdlib.h>

typedef struct
{
  glr_disambig_score_fn fn;
  glr_disambig_destroy_fn destroy;
  void *user_data;
} glr_probability_state_t;

static void
glr_probability_state_destroy (void *user_data)
{
  glr_probability_state_t *state = user_data;

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

static long double
glr_probability_node (const glr_disambig_context_t *context,
                      const glr_forest_node_t *node,
                      const glr_disambig_candidate_t *candidate,
                      glr_probability_state_t *state)
{
  long double probability = 1.0L;
  size_t i;

  if (node == NULL)
    {
      return 1.0L;
    }

  if (state->fn != NULL)
    {
      probability *= (long double)state->fn (context, node, candidate,
                                             state->user_data);
    }

  for (i = 0; i < node->child_count; i++)
    {
      probability
          *= glr_probability_node (context, node->children[i], candidate, state);
    }

  return probability;
}

static glr_disambig_result_t
glr_probability_hook (glr_disambig_context_t *context, size_t *winner_index,
                      void *user_data)
{
  glr_probability_state_t *state = user_data;
  long double best_probability = -LDBL_MAX;
  size_t best_index = SIZE_MAX;
  bool tied = false;
  size_t i;

  for (i = 0; i < context->candidate_count; i++)
    {
      glr_disambig_candidate_t *candidate = &context->candidates[i];
      long double total;

      if (!glr_disambig_candidate_is_active (candidate))
        {
          continue;
        }

      total = (long double)(candidate->probability > 0.0
                                ? candidate->probability
                                : 1.0)
              * glr_probability_node (context, candidate->node, candidate,
                                      state);

      if (best_index == SIZE_MAX || total > best_probability)
        {
          best_probability = total;
          best_index = i;
          tied = false;
        }
      else if (total == best_probability)
        {
          tied = true;
        }
    }

  if (best_index == SIZE_MAX || tied)
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
glr_disambig_probability_hook_create (const char *name,
                                      unsigned int priority,
                                      glr_disambig_score_fn fn,
                                      void *user_data,
                                      glr_disambig_destroy_fn destroy)
{
  glr_probability_state_t *state = calloc (1, sizeof (*state));

  if (state == NULL)
    {
      return NULL;
    }

  state->fn = fn;
  state->destroy = destroy;
  state->user_data = user_data;

  return glr_disambig_hook_create (
      name != NULL ? name : "probability", priority, glr_probability_hook,
      state, glr_probability_state_destroy);
}
