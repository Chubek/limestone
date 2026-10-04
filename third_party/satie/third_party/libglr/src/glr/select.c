#include <glr/select.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
alias_letter (unsigned char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

int
glr_production_set_alias (glr_grammar_t *grammar, int production_id,
                          size_t position, const char *alias)
{
  glr_production_t *production = glr_grammar_get_production (grammar, production_id);
  const char *interned = NULL;
  if (production == NULL || position == 0 || position > production->body_length)
    return -1;
  if (alias != NULL)
    {
      if (!alias_letter ((unsigned char) alias[0])
          || strcmp (alias, "LEXEME") == 0)
        return -1;
      for (size_t i = 1; alias[i] != '\0'; i++)
        if (!(alias_letter ((unsigned char) alias[i])
              || (alias[i] >= '0' && alias[i] <= '9')))
          return -1;
      for (size_t i = 0; i < production->body_length; i++)
        if (i != position - 1 && production->aliases != NULL
            && production->aliases[i] != NULL
            && strcmp (production->aliases[i], alias) == 0)
          return -1;
      interned = glr_stringpool_intern (grammar->strings, alias);
      if (interned == NULL)
        return -1;
    }
  if (production->aliases == NULL)
    {
      if (alias == NULL)
        return 0;
      production->aliases = calloc (production->body_length,
                                     sizeof (*production->aliases));
      if (production->aliases == NULL)
        return -1;
    }
  production->aliases[position - 1] = interned;
  return 0;
}

const char *
glr_production_get_alias (const glr_production_t *production, size_t position)
{
  if (production == NULL || production->aliases == NULL || position == 0
      || position > production->body_length)
    return NULL;
  return production->aliases[position - 1];
}

int
glr_select_position (const glr_select_context_t *context, size_t position,
                     glr_selection_t *selection)
{
  const glr_forest_node_t *node;
  if (selection == NULL)
    return -1;
  memset (selection, 0, sizeof (*selection));
  if (context == NULL || context->production == NULL || context->children == NULL
      || context->production->body == NULL
      || context->input == NULL || position == 0
      || context->child_count != context->production->body_length
      || position > context->child_count)
    return -1;
  node = context->children[position - 1];
  if (context->production->body[position - 1] == NULL
      || node == NULL || node->end_position < node->position
      || node->end_position > context->input_length
      || node->symbol_id != context->production->body[position - 1]->id)
    return -1;
  selection->node = node;
  selection->value = context->values != NULL ? context->values[position - 1] : NULL;
  selection->lexeme = context->input + node->position;
  selection->length = node->end_position - node->position;
  return 0;
}

int
glr_select_alias (const glr_select_context_t *context, const char *alias,
                  glr_selection_t *selection)
{
  if (selection == NULL)
    return -1;
  memset (selection, 0, sizeof (*selection));
  if (context == NULL || context->production == NULL || alias == NULL)
    return -1;
  for (size_t i = 1; i <= context->production->body_length; i++)
    {
      const char *candidate = glr_production_get_alias (context->production, i);
      if (candidate != NULL && strcmp (candidate, alias) == 0)
        return glr_select_position (context, i, selection);
    }
  return -1;
}

int
glr_select (const glr_select_context_t *context, const char *selector,
            glr_selection_t *selection)
{
  size_t position = 0;
  if (selection == NULL)
    return -1;
  memset (selection, 0, sizeof (*selection));
  if (context == NULL || selector == NULL || selector[0] != '$')
    return -1;
  selector++;
  if (strcmp (selector, "LEXEME") == 0)
    {
      if (context->production == NULL || context->production->body == NULL
          || context->production->body_length != 1
          || !glr_symbol_is_terminal (context->production->body[0]))
        return -1;
      return glr_select_position (context, 1, selection);
    }
  if (*selector >= '0' && *selector <= '9')
    {
      do
        {
          unsigned digit = (unsigned) (*selector++ - '0');
          if (position > (SIZE_MAX - digit) / 10)
            return -1;
          position = position * 10 + digit;
        }
      while (*selector >= '0' && *selector <= '9');
      return *selector == '\0' ? glr_select_position (context, position, selection)
                                : -1;
    }
  return glr_select_alias (context, selector, selection);
}
