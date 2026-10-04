#ifndef GLR_SEMANTIC_ACTION_H
#define GLR_SEMANTIC_ACTION_H

#include <glr/select.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  GLR_SEMANTIC_SUCCESS = 0,
  GLR_SEMANTIC_ERROR_ARGUMENT,
  GLR_SEMANTIC_ERROR_MEMORY,
  GLR_SEMANTIC_ERROR_AMBIGUOUS,
  GLR_SEMANTIC_ERROR_CYCLE,
  GLR_SEMANTIC_ERROR_ACTION
} glr_semantic_error_t;

typedef struct
{
  glr_select_context_t selection;
  const glr_grammar_t *grammar;
  const glr_forest_node_t *constructor;
  void *user_data; /* Parser/evaluation context, distinct from action_data. */
} glr_semantic_context_t;

/** Return zero on success. Values are opaque, caller-owned pointers; libglr
    never destroys them. Default terminal values are their forest nodes;
    default unary productions forward their child value, and other productions
    return their constructor node. These default node values are borrowed from
    the forest and have its lifetime. Selections always expose the source slice. */
typedef int (*glr_semantic_action_fn) (const glr_semantic_context_t *context,
                                      void **value, void *action_data);
typedef void (*glr_semantic_data_destroy_fn) (void *action_data);
/** Select a zero-based packed alternative, or SIZE_MAX to reject ambiguity. */
typedef size_t (*glr_semantic_resolver_fn) (const glr_forest_node_t *symbol,
                                          void *user_data);

int glr_production_set_semantic_action (glr_grammar_t *grammar, int production_id,
                                        glr_semantic_action_fn action,
                                        void *action_data,
                                        glr_semantic_data_destroy_fn destroy);
bool glr_grammar_has_semantic_actions (const glr_grammar_t *grammar);

/** Evaluate the accepted forest bottom-up. First validate and resolve the
    entire chosen derivation, then invoke callbacks once per shared constructor.
    No callbacks run for unresolved ambiguity or cyclic/invalid derivations.
    An action failure stops evaluation; values already produced remain owned
    by the caller. No evaluation state is stored in forest nodes. */
glr_semantic_error_t glr_semantic_evaluate (const glr_grammar_t *grammar,
                                           const glr_forest_t *forest,
                                           const char *input, size_t length,
                                           glr_semantic_resolver_fn resolver,
                                           void *user_data, void **value);

#ifdef __cplusplus
}
#endif
#endif
