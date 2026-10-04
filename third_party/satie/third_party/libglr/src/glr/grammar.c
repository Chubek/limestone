#include <glr/grammar.h>
#include "containers.h"
#include "grammar-internal.h"
#include <klib/kvec.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
glr_grammar_symbol_destroy (glr_symbol_t *symbol)
{
  if (symbol == NULL)
    return;
  glr_terminal_pattern_destroy (symbol->pattern);
  free (symbol);
}

void
glr_grammar_production_destroy (glr_production_t *production)
{
  if (production == NULL)
    return;
  if (production->semantic_action != NULL)
    {
      if (production->semantic_action->destroy != NULL)
        production->semantic_action->destroy (production->semantic_action->data);
      free (production->semantic_action);
    }
  free (production->aliases);
  free (production->body);
  free (production->annotation);
  free (production);
}

glr_grammar_t *
glr_grammar_create (void)
{
  glr_grammar_t *grammar = calloc (1, sizeof (glr_grammar_t));
  if (grammar == NULL)
    {
      return NULL;
    }

  grammar->symbols = NULL;
  grammar->symbol_count = 0;
  grammar->productions = NULL;
  grammar->production_count = 0;
  grammar->start_symbol = NULL;
  grammar->name = NULL;
  grammar->parse_table = NULL;
  grammar->owns_parse_table = false;
  grammar->strings = glr_stringpool_create ();
  if (grammar->strings == NULL)
    {
      free (grammar);
      return NULL;
    }

  return grammar;
}

void
glr_grammar_destroy (glr_grammar_t *grammar)
{
  if (grammar == NULL)
    {
      return;
    }

  /* Free all symbols */
  for (size_t i = 0; i < grammar->symbol_count; i++)
    {
      glr_symbol_t *symbol = grammar->symbols[i];
      if (symbol != NULL)
        {
          glr_grammar_symbol_destroy (symbol);
        }
    }
  free (grammar->symbols);

  /* Free all productions */
  for (size_t i = 0; i < grammar->production_count; i++)
    {
      glr_production_t *production = grammar->productions[i];
      if (production != NULL)
        {
          glr_grammar_production_destroy (production);
        }
    }
  free (grammar->productions);

  free (grammar->name);
  glr_stringpool_destroy (grammar->strings);
  if (grammar->owns_parse_table)
    {
      glr_parse_table_destroy (grammar->parse_table);
    }
  free (grammar);
}

int
glr_grammar_add_symbol (glr_grammar_t *grammar, glr_symbol_type_t type,
                        const char *name)
{
  if (grammar == NULL || name == NULL || name[0] == '\0'
      || (type != GLR_SYMBOL_TERMINAL && type != GLR_SYMBOL_NONTERMINAL)
      || grammar->symbol_count >= INT_MAX)
    {
      return -1;
    }

  /* Allocate new symbol */
  glr_symbol_t *symbol = calloc (1, sizeof (glr_symbol_t));
  if (symbol == NULL)
    {
      return -1;
    }

  symbol->type = type;
  symbol->id = (int)grammar->symbol_count;
  symbol->name = (char *) glr_stringpool_intern (grammar->strings, name);
  if (symbol->name == NULL)
    {
      free (symbol);
      return -1;
    }

  kvec_t (glr_symbol_t *) symbols = {
    grammar->symbol_count, grammar->symbol_capacity, grammar->symbols
  };
  glr_symbol_t **new_symbols = glr_array_grow (
      symbols.a, &symbols.m, symbols.n + 1, sizeof (*symbols.a));
  if (new_symbols == NULL)
    {
      free (symbol);
      return -1;
    }

  symbols.a = new_symbols;
  kv_push (glr_symbol_t *, symbols, symbol);
  grammar->symbols = symbols.a;
  grammar->symbol_count = symbols.n;
  grammar->symbol_capacity = symbols.m;

  return (int)(grammar->symbol_count - 1);
}

glr_symbol_t *
glr_grammar_get_symbol (const glr_grammar_t *grammar, int id)
{
  if (grammar == NULL || id < 0 || (size_t)id >= grammar->symbol_count)
    {
      return NULL;
    }

  return grammar->symbols[id];
}

int
glr_grammar_add_production (glr_grammar_t *grammar, int head_id,
                            glr_symbol_t **body, size_t body_length)
{
  if (grammar == NULL || head_id < 0 || grammar->production_count >= INT_MAX
      || body_length > SIZE_MAX / sizeof (*body)
      || (body_length != 0 && body == NULL))
    {
      return -1;
    }

  /* Validate head is a non-terminal */
  glr_symbol_t *head = glr_grammar_get_symbol (grammar, head_id);
  if (head == NULL || !glr_symbol_is_nonterminal (head))
    {
      return -1;
    }

  /* Allocate new production */
  glr_production_t *production = calloc (1, sizeof (glr_production_t));
  if (production == NULL)
    {
      return -1;
    }

  production->id = (int)grammar->production_count;
  production->head = head;
  production->body_length = body_length;

  /* Copy body symbols */
  production->body = calloc (body_length == 0 ? 1 : body_length,
                             sizeof (glr_symbol_t *));
  if (production->body == NULL)
    {
      free (production);
      return -1;
    }

  for (size_t i = 0; i < body_length; i++)
    {
      size_t owned;
      for (owned = 0; owned < grammar->symbol_count; owned++)
        if (grammar->symbols[owned] == body[i])
          break;
      if (body[i] == NULL || owned == grammar->symbol_count)
        {
          free (production->body);
          free (production);
          return -1;
        }
      production->body[i] = body[i];
    }

  production->annotation = NULL;

  kvec_t (glr_production_t *) productions = {
    grammar->production_count, grammar->production_capacity, grammar->productions
  };
  glr_production_t **new_productions = glr_array_grow (
      productions.a, &productions.m, productions.n + 1, sizeof (*productions.a));
  if (new_productions == NULL)
    {
      free (production->body);
      free (production);
      return -1;
    }

  productions.a = new_productions;
  kv_push (glr_production_t *, productions, production);
  grammar->productions = productions.a;
  grammar->production_count = productions.n;
  grammar->production_capacity = productions.m;

  return (int)(grammar->production_count - 1);
}

int
glr_grammar_set_start_symbol (glr_grammar_t *grammar, int symbol_id)
{
  if (grammar == NULL || symbol_id < 0)
    {
      return -1;
    }

  glr_symbol_t *symbol = glr_grammar_get_symbol (grammar, symbol_id);
  if (symbol == NULL || !glr_symbol_is_nonterminal (symbol))
    {
      return -1;
    }

  grammar->start_symbol = symbol;
  return 0;
}

glr_production_t *
glr_grammar_get_production (const glr_grammar_t *grammar, int id)
{
  if (grammar == NULL || id < 0 || (size_t)id >= grammar->production_count)
    {
      return NULL;
    }

  return grammar->productions[id];
}

int
glr_grammar_set_parse_table (glr_grammar_t *grammar,
                             glr_parse_table_t *parse_table,
                             bool take_ownership)
{
  if (grammar == NULL)
    {
      return -1;
    }

  if (grammar->owns_parse_table && grammar->parse_table != NULL
      && grammar->parse_table != parse_table)
    {
      glr_parse_table_destroy (grammar->parse_table);
    }

  grammar->parse_table = parse_table;
  grammar->owns_parse_table = parse_table != NULL && take_ownership;

  return 0;
}

glr_parse_table_t *
glr_grammar_get_parse_table (const glr_grammar_t *grammar)
{
  return grammar != NULL ? grammar->parse_table : NULL;
}

int
glr_grammar_find_symbol (const glr_grammar_t *grammar, const char *name,
                         glr_symbol_type_t type)
{
  size_t i;

  if (grammar == NULL || name == NULL)
    {
      return -1;
    }

  for (i = 0; i < grammar->symbol_count; i++)
    {
      glr_symbol_t *symbol = grammar->symbols[i];
      if (symbol != NULL && symbol->type == type && symbol->name != NULL
          && strcmp (symbol->name, name) == 0)
        {
          return symbol->id;
        }
    }

  return -1;
}

int
glr_grammar_find_symbol_any (const glr_grammar_t *grammar, const char *name)
{
  size_t i;

  if (grammar == NULL || name == NULL)
    {
      return -1;
    }

  for (i = 0; i < grammar->symbol_count; i++)
    {
      glr_symbol_t *symbol = grammar->symbols[i];
      if (symbol != NULL && symbol->name != NULL
          && strcmp (symbol->name, name) == 0)
        {
          return symbol->id;
        }
    }

  return -1;
}

static void
glr_grammar_validation_error (char *error, size_t error_size,
                              const char *message)
{
  if (error != NULL && error_size > 0)
    {
      snprintf (error, error_size, "%s", message);
    }
}

bool
glr_grammar_validate (const glr_grammar_t *grammar, char *error,
                      size_t error_size)
{
  size_t i;
  size_t k;

  if (grammar == NULL)
    {
      glr_grammar_validation_error (error, error_size, "null grammar");
      return false;
    }

  if (grammar->start_symbol == NULL
      || !glr_symbol_is_nonterminal (grammar->start_symbol)
      || glr_grammar_get_symbol (grammar, grammar->start_symbol->id)
             != grammar->start_symbol)
    {
      glr_grammar_validation_error (error, error_size, "no start symbol");
      return false;
    }

  for (i = 0; i < grammar->production_count; i++)
    {
      glr_production_t *production = grammar->productions[i];
      if (production == NULL)
        {
          glr_grammar_validation_error (error, error_size,
                                        "null production entry");
          return false;
        }
      if (production->head == NULL
          || !glr_symbol_is_nonterminal (production->head)
          || glr_grammar_get_symbol (grammar, production->head->id)
                 != production->head
          || (production->body_length > 0 && production->body == NULL))
        {
          glr_grammar_validation_error (error, error_size,
                                        "production head must be a "
                                        "non-terminal");
          return false;
        }
      for (k = 0; k < production->body_length; k++)
        {
          glr_symbol_t *body_symbol = production->body[k];
          bool owned = false;
          size_t s;
          if (body_symbol == NULL)
            {
              glr_grammar_validation_error (error, error_size,
                                            "null body symbol");
              return false;
            }
          for (s = 0; s < grammar->symbol_count; s++)
            {
              if (grammar->symbols[s] == body_symbol)
                {
                  owned = true;
                  break;
                }
            }
          if (!owned)
            {
              glr_grammar_validation_error (error, error_size,
                                            "body symbol not owned by "
                                            "grammar");
              return false;
            }
        }
    }

  if (error != NULL && error_size > 0)
    {
      error[0] = '\0';
    }
  return true;
}
