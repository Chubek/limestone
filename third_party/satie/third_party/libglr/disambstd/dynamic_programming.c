#include <glr/disambiguate.h>
#include <float.h>
#include <stdlib.h>

typedef struct
{
  glr_disambig_score_fn fn;
  glr_disambig_destroy_fn destroy;
  void *user_data;
} glr_dp_state_t;

static void
glr_dp_state_destroy (void *user_data)
{
  glr_dp_state_t *state = user_data;

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

static double
glr_dp_score_node (const glr_disambig_context_t *context,
                   const glr_forest_node_t *node,
                   const glr_disambig_candidate_t *candidate,
                   glr_dp_state_t *state)
{
  double score = 0.0;
  size_t i;

  if (node == NULL)
    {
      return 0.0;
    }

  if (state->fn != NULL)
    {
      score += state->fn (context, node, candidate, state->user_data);
    }

  for (i = 0; i < node->child_count; i++)
    {
      score += glr_dp_score_node (context, node->children[i], candidate, state);
    }

  return score;
}

static glr_disambig_result_t
glr_dp_hook (glr_disambig_context_t *context, size_t *winner_index,
             void *user_data)
{
  glr_dp_state_t *state = user_data;
  double best_score = DBL_MAX;
  size_t best_index = SIZE_MAX;
  bool tied = false;
  size_t i;

  for (i = 0; i < context->candidate_count; i++)
    {
      glr_disambig_candidate_t *candidate = &context->candidates[i];
      double total;

      if (!glr_disambig_candidate_is_active (candidate))
        {
          continue;
        }

      total = candidate->score
              + glr_dp_score_node (context, candidate->node, candidate, state);

      if (best_index == SIZE_MAX || total < best_score)
        {
          best_score = total;
          best_index = i;
          tied = false;
        }
      else if (total == best_score)
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
glr_disambig_dynamic_programming_hook_create (
    const char *name, unsigned int priority, glr_disambig_score_fn fn,
    void *user_data, glr_disambig_destroy_fn destroy)
{
  glr_dp_state_t *state = calloc (1, sizeof (*state));

  if (state == NULL)
    {
      return NULL;
    }

  state->fn = fn;
  state->destroy = destroy;
  state->user_data = user_data;

  return glr_disambig_hook_create (
      name != NULL ? name : "dynamic-programming", priority, glr_dp_hook,
      state, glr_dp_state_destroy);
}
