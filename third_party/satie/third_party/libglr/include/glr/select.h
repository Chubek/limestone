#ifndef GLR_SELECT_H
#define GLR_SELECT_H

#include <glr/forest.h>
#include <glr/grammar.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  const glr_production_t *production;
  glr_forest_node_t *const *children;
  size_t child_count;
  void *const *values;
  const char *input;
  size_t input_length;
} glr_select_context_t;

typedef struct
{
  const glr_forest_node_t *node;
  void *value;
  const char *lexeme; /* Borrowed slice, not necessarily NUL-terminated. */
  size_t length;
} glr_selection_t;

/** Positions are one-based, like $1. Aliases are unique within a production,
    consist of [A-Za-z_][A-Za-z0-9_]*, and cannot be LEXEME. NULL clears one. */
int glr_production_set_alias (glr_grammar_t *grammar, int production_id,
                              size_t position, const char *alias);
const char *glr_production_get_alias (const glr_production_t *production,
                                     size_t position);
int glr_select_position (const glr_select_context_t *context, size_t position,
                         glr_selection_t *selection);
int glr_select_alias (const glr_select_context_t *context, const char *alias,
                      glr_selection_t *selection);
/** Resolve $1, $alias, or $LEXEME. $LEXEME requires exactly one terminal in
    the production. Errors clear the selection and return -1. */
int glr_select (const glr_select_context_t *context, const char *selector,
                glr_selection_t *selection);

#ifdef __cplusplus
}
#endif
#endif
