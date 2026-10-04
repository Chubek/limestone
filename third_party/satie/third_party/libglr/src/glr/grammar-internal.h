#ifndef GLR_GRAMMAR_INTERNAL_H
#define GLR_GRAMMAR_INTERNAL_H
#include <glr/grammar.h>
#include <glr/semantic-action.h>

struct glr_semantic_action
{
  glr_semantic_action_fn callback;
  void *data;
  glr_semantic_data_destroy_fn destroy;
};

void glr_terminal_pattern_destroy (struct glr_terminal_pattern *pattern);
void glr_grammar_symbol_destroy (glr_symbol_t *symbol);
void glr_grammar_production_destroy (glr_production_t *production);
#endif
