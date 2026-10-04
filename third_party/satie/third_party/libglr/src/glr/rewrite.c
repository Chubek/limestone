#include <glr/rewrite.h>
#include "grammar-internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../third_party/sfsexp/src/sexp.h"

static char *glr_rewrite_strdup (const char *text);
static void glr_rewrite_set_error (char *buffer, size_t size,
                                   const char *message);
static glr_symbol_t *glr_rewrite_find_symbol (const glr_grammar_t *grammar,
                                              const char *name);
static void glr_rewrite_free_rule (glr_rewrite_rule_t *rule);
static glr_rewrite_status_t glr_rewrite_reserve_rules (
    glr_rewrite_program_t *program, size_t extra);
static glr_rewrite_status_t glr_rewrite_append_rule_copy (
    glr_rewrite_program_t *program, const glr_rewrite_rule_t *rule);
static bool glr_rewrite_production_equals_names (
    const glr_production_t *production, const char *head_name,
    const char *const *body, size_t body_length);
static void glr_rewrite_remove_production_at (glr_grammar_t *grammar,
                                              size_t index);
static void glr_rewrite_remove_symbol_at (glr_grammar_t *grammar,
                                          size_t index);
static bool glr_rewrite_production_exists (const glr_grammar_t *grammar,
                                           const glr_symbol_t *head,
                                           glr_symbol_t *const *body,
                                           size_t body_length);
static glr_rewrite_status_t glr_rewrite_rebuild_grammar (
    glr_grammar_t *grammar, bool *keep_symbols, bool *keep_productions);
static glr_rewrite_status_t glr_rewrite_expand_nullable (
    glr_grammar_t *grammar, bool *nullable);
static glr_rewrite_status_t glr_rewrite_expand_indirect_left_recursion (
    glr_grammar_t *grammar, glr_symbol_t *ai, glr_symbol_t *aj);
static glr_rewrite_status_t glr_rewrite_eliminate_direct_left_recursion (
    glr_grammar_t *grammar, glr_symbol_t *symbol, size_t suffix_index);
static glr_rewrite_status_t glr_rewrite_factor_head (glr_grammar_t *grammar,
                                                     glr_symbol_t *head,
                                                     size_t *generated_count);
static bool glr_rewrite_is_atom (const sexp_t *node, const char *value);
static const char *glr_rewrite_atom_text (const sexp_t *node);
static glr_rewrite_status_t glr_rewrite_parse_rule (glr_rewrite_program_t *p,
                                                    const sexp_t *node,
                                                    char *error_buffer,
                                                    size_t error_size);

static char *
glr_rewrite_strdup (const char *text)
{
  size_t length;
  char *copy;

  if (text == NULL)
    {
      return NULL;
    }

  length = strlen (text);
  copy = malloc (length + 1);
  if (copy == NULL)
    {
      return NULL;
    }

  memcpy (copy, text, length + 1);
  return copy;
}

static void
glr_rewrite_set_error (char *buffer, size_t size, const char *message)
{
  if (buffer == NULL || size == 0)
    {
      return;
    }

  if (message == NULL)
    {
      buffer[0] = '\0';
      return;
    }

  snprintf (buffer, size, "%s", message);
}

static glr_symbol_t *
glr_rewrite_find_symbol (const glr_grammar_t *grammar, const char *name)
{
  size_t i;

  if (grammar == NULL || name == NULL)
    {
      return NULL;
    }

  for (i = 0; i < grammar->symbol_count; i++)
    {
      glr_symbol_t *symbol = grammar->symbols[i];
      if (symbol != NULL && symbol->name != NULL
          && strcmp (symbol->name, name) == 0)
        {
          return symbol;
        }
    }

  return NULL;
}

static void
glr_rewrite_free_rule (glr_rewrite_rule_t *rule)
{
  size_t i;

  if (rule == NULL)
    {
      return;
    }

  switch (rule->kind)
    {
    case GLR_REWRITE_RULE_ADD_SYMBOL:
    case GLR_REWRITE_RULE_DROP_SYMBOL:
      free (rule->data.symbol.name);
      break;
    case GLR_REWRITE_RULE_RENAME_SYMBOL:
      free (rule->data.rename_symbol.old_name);
      free (rule->data.rename_symbol.new_name);
      break;
    case GLR_REWRITE_RULE_SET_START:
      free (rule->data.start_symbol_name);
      break;
    case GLR_REWRITE_RULE_ADD_PRODUCTION:
    case GLR_REWRITE_RULE_DROP_PRODUCTION:
      free (rule->data.production.head_name);
      for (i = 0; i < rule->data.production.body_length; i++)
        {
          free (rule->data.production.body[i].name);
        }
      free (rule->data.production.body);
      break;
    default:
      break;
    }
}

static glr_rewrite_status_t
glr_rewrite_reserve_rules (glr_rewrite_program_t *program, size_t extra)
{
  glr_rewrite_rule_t *new_rules;
  size_t new_capacity;

  if (program == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  if (program->rule_count + extra <= program->rule_capacity)
    {
      return GLR_REWRITE_STATUS_OK;
    }

  new_capacity = program->rule_capacity == 0 ? 8 : program->rule_capacity * 2;
  while (new_capacity < program->rule_count + extra)
    {
      new_capacity *= 2;
    }

  new_rules = realloc (program->rules, new_capacity * sizeof (*new_rules));
  if (new_rules == NULL)
    {
      return GLR_REWRITE_STATUS_MEMORY_ERROR;
    }

  program->rules = new_rules;
  program->rule_capacity = new_capacity;
  return GLR_REWRITE_STATUS_OK;
}

static glr_rewrite_status_t
glr_rewrite_append_rule_copy (glr_rewrite_program_t *program,
                              const glr_rewrite_rule_t *rule)
{
  glr_rewrite_rule_t copy;
  size_t i;
  glr_rewrite_status_t status;

  if (program == NULL || rule == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  memset (&copy, 0, sizeof (copy));
  copy.kind = rule->kind;

  switch (rule->kind)
    {
    case GLR_REWRITE_RULE_ADD_SYMBOL:
    case GLR_REWRITE_RULE_DROP_SYMBOL:
      copy.data.symbol.type = rule->data.symbol.type;
      copy.data.symbol.name = glr_rewrite_strdup (rule->data.symbol.name);
      if (copy.data.symbol.name == NULL)
        {
          return GLR_REWRITE_STATUS_MEMORY_ERROR;
        }
      break;
    case GLR_REWRITE_RULE_RENAME_SYMBOL:
      copy.data.rename_symbol.old_name
          = glr_rewrite_strdup (rule->data.rename_symbol.old_name);
      copy.data.rename_symbol.new_name
          = glr_rewrite_strdup (rule->data.rename_symbol.new_name);
      if (copy.data.rename_symbol.old_name == NULL
          || copy.data.rename_symbol.new_name == NULL)
        {
          glr_rewrite_free_rule (&copy);
          return GLR_REWRITE_STATUS_MEMORY_ERROR;
        }
      break;
    case GLR_REWRITE_RULE_SET_START:
      copy.data.start_symbol_name
          = glr_rewrite_strdup (rule->data.start_symbol_name);
      if (copy.data.start_symbol_name == NULL)
        {
          return GLR_REWRITE_STATUS_MEMORY_ERROR;
        }
      break;
    case GLR_REWRITE_RULE_ADD_PRODUCTION:
    case GLR_REWRITE_RULE_DROP_PRODUCTION:
      copy.data.production.head_name
          = glr_rewrite_strdup (rule->data.production.head_name);
      copy.data.production.body_length = rule->data.production.body_length;
      copy.data.production.body = calloc (
          rule->data.production.body_length == 0 ? 1
                                                 : rule->data.production.body_length,
          sizeof (*copy.data.production.body));
      if (copy.data.production.head_name == NULL
          || copy.data.production.body == NULL)
        {
          glr_rewrite_free_rule (&copy);
          return GLR_REWRITE_STATUS_MEMORY_ERROR;
        }

      for (i = 0; i < copy.data.production.body_length; i++)
        {
          copy.data.production.body[i].type = rule->data.production.body[i].type;
          copy.data.production.body[i].name
              = glr_rewrite_strdup (rule->data.production.body[i].name);
          if (copy.data.production.body[i].name == NULL)
            {
              glr_rewrite_free_rule (&copy);
              return GLR_REWRITE_STATUS_MEMORY_ERROR;
            }
        }
      break;
    default:
      break;
    }

  status = glr_rewrite_reserve_rules (program, 1);
  if (status != GLR_REWRITE_STATUS_OK)
    {
      glr_rewrite_free_rule (&copy);
      return status;
    }

  program->rules[program->rule_count++] = copy;
  return GLR_REWRITE_STATUS_OK;
}

static bool
glr_rewrite_production_equals_names (const glr_production_t *production,
                                     const char *head_name,
                                     const char *const *body,
                                     size_t body_length)
{
  size_t i;

  if (production == NULL || head_name == NULL)
    {
      return false;
    }

  if (production->head == NULL || production->head->name == NULL
      || strcmp (production->head->name, head_name) != 0
      || production->body_length != body_length)
    {
      return false;
    }

  for (i = 0; i < body_length; i++)
    {
      if (production->body[i] == NULL || production->body[i]->name == NULL
          || strcmp (production->body[i]->name, body[i]) != 0)
        {
          return false;
        }
    }

  return true;
}

static void
glr_rewrite_remove_production_at (glr_grammar_t *grammar, size_t index)
{
  size_t i;
  glr_production_t *production;

  if (grammar == NULL || index >= grammar->production_count)
    {
      return;
    }

  production = grammar->productions[index];
  glr_grammar_production_destroy (production);

  for (i = index + 1; i < grammar->production_count; i++)
    {
      grammar->productions[i - 1] = grammar->productions[i];
      grammar->productions[i - 1]->id = (int)(i - 1);
    }

  grammar->production_count--;
}

static void
glr_rewrite_remove_symbol_at (glr_grammar_t *grammar, size_t index)
{
  size_t i;
  glr_symbol_t *symbol;

  if (grammar == NULL || index >= grammar->symbol_count)
    {
      return;
    }

  symbol = grammar->symbols[index];
  if (grammar->start_symbol == symbol)
    {
      grammar->start_symbol = NULL;
    }

  glr_grammar_symbol_destroy (symbol);

  for (i = index + 1; i < grammar->symbol_count; i++)
    {
      grammar->symbols[i - 1] = grammar->symbols[i];
      grammar->symbols[i - 1]->id = (int)(i - 1);
    }

  grammar->symbol_count--;
}

static bool
glr_rewrite_production_exists (const glr_grammar_t *grammar,
                               const glr_symbol_t *head,
                               glr_symbol_t *const *body, size_t body_length)
{
  size_t i;
  size_t j;

  if (grammar == NULL || head == NULL)
    {
      return false;
    }

  for (i = 0; i < grammar->production_count; i++)
    {
      const glr_production_t *production = grammar->productions[i];
      if (production->head != head || production->body_length != body_length)
        {
          continue;
        }

      for (j = 0; j < body_length; j++)
        {
          if (production->body[j] != body[j])
            {
              break;
            }
        }

      if (j == body_length)
        {
          return true;
        }
    }

  return false;
}

static glr_rewrite_status_t
glr_rewrite_rebuild_grammar (glr_grammar_t *grammar, bool *keep_symbols,
                             bool *keep_productions)
{
  glr_symbol_t **symbols;
  glr_production_t **productions;
  size_t new_symbol_count = 0;
  size_t new_production_count = 0;
  size_t i;

  if (grammar == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  symbols = calloc (grammar->symbol_count == 0 ? 1 : grammar->symbol_count,
                    sizeof (*symbols));
  productions = calloc (grammar->production_count == 0 ? 1
                                                       : grammar->production_count,
                        sizeof (*productions));
  if (symbols == NULL || productions == NULL)
    {
      free (symbols);
      free (productions);
      return GLR_REWRITE_STATUS_MEMORY_ERROR;
    }

  for (i = 0; i < grammar->symbol_count; i++)
    {
      if (!keep_symbols[i])
        {
          if (grammar->start_symbol == grammar->symbols[i])
            {
              grammar->start_symbol = NULL;
            }
          glr_grammar_symbol_destroy (grammar->symbols[i]);
          continue;
        }

      grammar->symbols[i]->id = (int)new_symbol_count;
      symbols[new_symbol_count++] = grammar->symbols[i];
    }

  for (i = 0; i < grammar->production_count; i++)
    {
      if (!keep_productions[i])
        {
          glr_grammar_production_destroy (grammar->productions[i]);
          continue;
        }

      grammar->productions[i]->id = (int)new_production_count;
      productions[new_production_count++] = grammar->productions[i];
    }

  free (grammar->symbols);
  free (grammar->productions);
  grammar->symbols = symbols;
  grammar->symbol_count = new_symbol_count;
  grammar->symbol_capacity = new_symbol_count;
  grammar->productions = productions;
  grammar->production_count = new_production_count;
  grammar->production_capacity = new_production_count;

  return GLR_REWRITE_STATUS_OK;
}

static glr_rewrite_status_t
glr_rewrite_expand_nullable (glr_grammar_t *grammar, bool *nullable)
{
  size_t i;
  size_t original_count;

  if (grammar == NULL || nullable == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  original_count = grammar->production_count;
  for (i = 0; i < original_count; i++)
    {
      glr_production_t *production = grammar->productions[i];
      size_t positions[32];
      size_t nullable_count = 0;
      size_t mask;

      if (production->body_length == 0)
        {
          continue;
        }

      if (production->body_length > 31)
        {
          continue;
        }

      for (size_t j = 0; j < production->body_length; j++)
        {
          glr_symbol_t *symbol = production->body[j];
          if (symbol != NULL && symbol->type == GLR_SYMBOL_NONTERMINAL
              && nullable[symbol->id])
            {
              positions[nullable_count++] = j;
            }
        }

      if (nullable_count == 0)
        {
          continue;
        }

      for (mask = 1; mask < ((size_t)1U << nullable_count); mask++)
        {
          glr_symbol_t *body[64];
          size_t body_length = 0;

          if (production->body_length > 64)
            {
              return GLR_REWRITE_STATUS_UNSUPPORTED;
            }

          for (size_t j = 0; j < production->body_length; j++)
            {
              bool drop = false;
              for (size_t bit = 0; bit < nullable_count; bit++)
                {
                  if (positions[bit] == j && ((mask >> bit) & 1U) != 0U)
                    {
                      drop = true;
                      break;
                    }
                }

              if (!drop)
                {
                  body[body_length++] = production->body[j];
                }
            }

          if (body_length == 0 && production->head != grammar->start_symbol)
            {
              continue;
            }

          if (!glr_rewrite_production_exists (grammar, production->head, body,
                                              body_length))
            {
              if (glr_grammar_add_production (grammar, production->head->id, body,
                                              body_length)
                  < 0)
                {
                  return GLR_REWRITE_STATUS_MEMORY_ERROR;
                }
            }
        }
    }

  return GLR_REWRITE_STATUS_OK;
}

static glr_rewrite_status_t
glr_rewrite_expand_indirect_left_recursion (glr_grammar_t *grammar,
                                            glr_symbol_t *ai,
                                            glr_symbol_t *aj)
{
  size_t i;
  size_t original_count;

  if (grammar == NULL || ai == NULL || aj == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  original_count = grammar->production_count;
  for (i = 0; i < original_count;)
    {
      glr_production_t *production = grammar->productions[i];
      size_t generated = 0;

      if (production->head != ai || production->body_length == 0
          || production->body[0] != aj)
        {
          i++;
          continue;
        }

      for (size_t j = 0; j < original_count; j++)
        {
          glr_production_t *prefix = grammar->productions[j];
          glr_symbol_t *body[128];
          size_t body_length = 0;

          if (prefix->head != aj)
            {
              continue;
            }

          if (prefix->body_length + production->body_length > 128)
            {
              return GLR_REWRITE_STATUS_UNSUPPORTED;
            }

          for (size_t k = 0; k < prefix->body_length; k++)
            {
              body[body_length++] = prefix->body[k];
            }
          for (size_t k = 1; k < production->body_length; k++)
            {
              body[body_length++] = production->body[k];
            }

          if (!glr_rewrite_production_exists (grammar, ai, body, body_length))
            {
              if (glr_grammar_add_production (grammar, ai->id, body, body_length)
                  < 0)
                {
                  return GLR_REWRITE_STATUS_MEMORY_ERROR;
                }
              generated++;
            }
        }

      glr_rewrite_remove_production_at (grammar, i);
      original_count--;
      if (generated == 0)
        {
          continue;
        }
    }

  return GLR_REWRITE_STATUS_OK;
}

static glr_rewrite_status_t
glr_rewrite_eliminate_direct_left_recursion (glr_grammar_t *grammar,
                                             glr_symbol_t *symbol,
                                             size_t suffix_index)
{
  typedef struct
  {
    glr_symbol_t *body[128];
    size_t body_length;
  } glr_rewrite_body_copy_t;

  size_t alpha_count = 0;
  size_t beta_count = 0;
  glr_rewrite_body_copy_t *alpha = NULL;
  glr_rewrite_body_copy_t *beta = NULL;
  glr_symbol_t *suffix;
  char generated_name[128];
  size_t i;

  if (grammar == NULL || symbol == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  for (i = 0; i < grammar->production_count; i++)
    {
      glr_production_t *production = grammar->productions[i];
      if (production->head != symbol)
        {
          continue;
        }

      if (production->body_length > 0 && production->body[0] == symbol)
        {
          alpha_count++;
        }
      else
        {
          beta_count++;
        }
    }

  if (alpha_count == 0)
    {
      return GLR_REWRITE_STATUS_OK;
    }

  alpha = calloc (alpha_count, sizeof (*alpha));
  beta = calloc (beta_count == 0 ? 1 : beta_count, sizeof (*beta));
  if (alpha == NULL || beta == NULL)
    {
      free (alpha);
      free (beta);
      return GLR_REWRITE_STATUS_MEMORY_ERROR;
    }

  alpha_count = 0;
  beta_count = 0;
  for (i = 0; i < grammar->production_count; i++)
    {
      glr_production_t *production = grammar->productions[i];
      if (production->head != symbol)
        {
          continue;
        }

      if (production->body_length > 0 && production->body[0] == symbol)
        {
          alpha[alpha_count].body_length = production->body_length;
          for (size_t j = 0; j < production->body_length; j++)
            {
              alpha[alpha_count].body[j] = production->body[j];
            }
          alpha_count++;
        }
      else
        {
          beta[beta_count].body_length = production->body_length;
          for (size_t j = 0; j < production->body_length; j++)
            {
              beta[beta_count].body[j] = production->body[j];
            }
          beta_count++;
        }
    }

  snprintf (generated_name, sizeof (generated_name), "%s__lr%zu", symbol->name,
            suffix_index);
  if (glr_rewrite_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, generated_name)
      != GLR_REWRITE_STATUS_OK)
    {
      free (alpha);
      free (beta);
      return GLR_REWRITE_STATUS_MEMORY_ERROR;
    }
  suffix = glr_rewrite_find_symbol (grammar, generated_name);

  for (i = grammar->production_count; i > 0; i--)
    {
      glr_production_t *production = grammar->productions[i - 1];
      if (production->head == symbol)
        {
          glr_rewrite_remove_production_at (grammar, i - 1);
        }
    }

  if (beta_count == 0)
    {
      glr_symbol_t *body[1] = { suffix };
      if (glr_grammar_add_production (grammar, symbol->id, body, 1) < 0)
        {
          free (alpha);
          free (beta);
          return GLR_REWRITE_STATUS_MEMORY_ERROR;
        }
    }

  for (i = 0; i < beta_count; i++)
    {
      glr_symbol_t *body[128];
      size_t body_length = 0;
      if (beta[i].body_length + 1 > 128)
        {
          free (alpha);
          free (beta);
          return GLR_REWRITE_STATUS_UNSUPPORTED;
        }

      for (size_t j = 0; j < beta[i].body_length; j++)
        {
          body[body_length++] = beta[i].body[j];
        }
      body[body_length++] = suffix;
      if (glr_grammar_add_production (grammar, symbol->id, body, body_length)
          < 0)
        {
          free (alpha);
          free (beta);
          return GLR_REWRITE_STATUS_MEMORY_ERROR;
        }
    }

  for (i = 0; i < alpha_count; i++)
    {
      glr_symbol_t *body[128];
      size_t body_length = 0;

      if (alpha[i].body_length + 1 > 128)
        {
          free (alpha);
          free (beta);
          return GLR_REWRITE_STATUS_UNSUPPORTED;
        }

      for (size_t j = 1; j < alpha[i].body_length; j++)
        {
          body[body_length++] = alpha[i].body[j];
        }
      body[body_length++] = suffix;
      if (glr_grammar_add_production (grammar, suffix->id, body, body_length)
          < 0)
        {
          free (alpha);
          free (beta);
          return GLR_REWRITE_STATUS_MEMORY_ERROR;
        }
    }

  if (glr_grammar_add_production (grammar, suffix->id, NULL, 0) < 0)
    {
      free (alpha);
      free (beta);
      return GLR_REWRITE_STATUS_MEMORY_ERROR;
    }

  free (alpha);
  free (beta);
  return GLR_REWRITE_STATUS_OK;
}

static glr_rewrite_status_t
glr_rewrite_factor_head (glr_grammar_t *grammar, glr_symbol_t *head,
                         size_t *generated_count)
{
  size_t i;

  if (grammar == NULL || head == NULL || generated_count == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  for (i = 0; i < grammar->production_count; i++)
    {
      glr_production_t *first = grammar->productions[i];
      if (first->head != head || first->body_length == 0)
        {
          continue;
        }

      for (size_t j = i + 1; j < grammar->production_count; j++)
        {
          glr_production_t *second = grammar->productions[j];
          char generated_name[128];
          glr_symbol_t *factored;
          glr_symbol_t *new_body[2];
          glr_symbol_t *tail_body[128];
          size_t tail_len;
          glr_symbol_t *first_tail[128];
          glr_symbol_t *second_tail[128];

          if (second->head != head || second->body_length == 0
              || second->body[0] != first->body[0])
            {
              continue;
            }

          snprintf (generated_name, sizeof (generated_name), "%s__lf%zu",
                    head->name, *generated_count);
          if (glr_rewrite_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL,
                                      generated_name)
              != GLR_REWRITE_STATUS_OK)
            {
              return GLR_REWRITE_STATUS_MEMORY_ERROR;
            }
          factored = glr_rewrite_find_symbol (grammar, generated_name);
          (*generated_count)++;

          new_body[0] = first->body[0];
          new_body[1] = factored;

          tail_len = first->body_length - 1;
          for (size_t k = 1; k < first->body_length; k++)
            {
              first_tail[k - 1] = first->body[k];
            }
          for (size_t k = 1; k < second->body_length; k++)
            {
              second_tail[k - 1] = second->body[k];
            }

          glr_rewrite_remove_production_at (grammar, j);
          glr_rewrite_remove_production_at (grammar, i);
          if (glr_grammar_add_production (grammar, head->id, new_body, 2) < 0)
            {
              return GLR_REWRITE_STATUS_MEMORY_ERROR;
            }

          tail_len = first->body_length - 1;
          for (size_t k = 0; k < tail_len; k++)
            {
              tail_body[k] = first_tail[k];
            }
          if (glr_grammar_add_production (grammar, factored->id, tail_body,
                                          tail_len)
              < 0)
            {
              return GLR_REWRITE_STATUS_MEMORY_ERROR;
            }

          tail_len = second->body_length - 1;
          for (size_t k = 0; k < tail_len; k++)
            {
              tail_body[k] = second_tail[k];
            }
          if (glr_grammar_add_production (grammar, factored->id, tail_body,
                                          tail_len)
              < 0)
            {
              return GLR_REWRITE_STATUS_MEMORY_ERROR;
            }

          return GLR_REWRITE_STATUS_OK;
        }
    }

  return GLR_REWRITE_STATUS_NOT_FOUND;
}

static bool
glr_rewrite_is_atom (const sexp_t *node, const char *value)
{
  return node != NULL && node->ty == SEXP_VALUE && node->val != NULL
         && strcmp (node->val, value) == 0;
}

static const char *
glr_rewrite_atom_text (const sexp_t *node)
{
  if (node == NULL || node->ty != SEXP_VALUE)
    {
      return NULL;
    }

  return node->val;
}

static glr_rewrite_status_t
glr_rewrite_parse_rule (glr_rewrite_program_t *program, const sexp_t *node,
                        char *error_buffer, size_t error_size)
{
  glr_rewrite_rule_t rule;
  const sexp_t *args;
  const char *op;

  if (program == NULL || node == NULL || node->ty != SEXP_LIST
      || node->list == NULL)
    {
      glr_rewrite_set_error (error_buffer, error_size,
                             "each rule must be a non-empty list");
      return GLR_REWRITE_STATUS_PARSE_ERROR;
    }

  memset (&rule, 0, sizeof (rule));
  op = glr_rewrite_atom_text (node->list);
  args = node->list->next;
  if (op == NULL)
    {
      glr_rewrite_set_error (error_buffer, error_size,
                             "rule name must be an atom");
      return GLR_REWRITE_STATUS_PARSE_ERROR;
    }

  if (strcmp (op, "add-symbol") == 0)
    {
      const char *kind = glr_rewrite_atom_text (args);
      const char *name = glr_rewrite_atom_text (args != NULL ? args->next : NULL);
      if (kind == NULL || name == NULL)
        {
          glr_rewrite_set_error (error_buffer, error_size,
                                 "add-symbol expects type and name");
          return GLR_REWRITE_STATUS_PARSE_ERROR;
        }
      rule.kind = GLR_REWRITE_RULE_ADD_SYMBOL;
      rule.data.symbol.type = strcmp (kind, "terminal") == 0
                                  ? GLR_SYMBOL_TERMINAL
                                  : GLR_SYMBOL_NONTERMINAL;
      rule.data.symbol.name = (char *)name;
    }
  else if (strcmp (op, "drop-symbol") == 0)
    {
      const char *name = glr_rewrite_atom_text (args);
      if (name == NULL)
        {
          glr_rewrite_set_error (error_buffer, error_size,
                                 "drop-symbol expects a name");
          return GLR_REWRITE_STATUS_PARSE_ERROR;
        }
      rule.kind = GLR_REWRITE_RULE_DROP_SYMBOL;
      rule.data.symbol.name = (char *)name;
    }
  else if (strcmp (op, "rename-symbol") == 0)
    {
      const char *old_name = glr_rewrite_atom_text (args);
      const char *new_name
          = glr_rewrite_atom_text (args != NULL ? args->next : NULL);
      if (old_name == NULL || new_name == NULL)
        {
          glr_rewrite_set_error (error_buffer, error_size,
                                 "rename-symbol expects old and new names");
          return GLR_REWRITE_STATUS_PARSE_ERROR;
        }
      rule.kind = GLR_REWRITE_RULE_RENAME_SYMBOL;
      rule.data.rename_symbol.old_name = (char *)old_name;
      rule.data.rename_symbol.new_name = (char *)new_name;
    }
  else if (strcmp (op, "set-start") == 0)
    {
      const char *name = glr_rewrite_atom_text (args);
      if (name == NULL)
        {
          glr_rewrite_set_error (error_buffer, error_size,
                                 "set-start expects a symbol name");
          return GLR_REWRITE_STATUS_PARSE_ERROR;
        }
      rule.kind = GLR_REWRITE_RULE_SET_START;
      rule.data.start_symbol_name = (char *)name;
    }
  else if (strcmp (op, "add-production") == 0
           || strcmp (op, "drop-production") == 0)
    {
      const sexp_t *body_list;
      const sexp_t *body_item;
      size_t body_length = 0;

      rule.kind = strcmp (op, "add-production") == 0
                      ? GLR_REWRITE_RULE_ADD_PRODUCTION
                      : GLR_REWRITE_RULE_DROP_PRODUCTION;
      rule.data.production.head_name = (char *)glr_rewrite_atom_text (args);
      body_list = args != NULL ? args->next : NULL;
      if (rule.data.production.head_name == NULL || body_list == NULL
          || body_list->ty != SEXP_LIST)
        {
          glr_rewrite_set_error (error_buffer, error_size,
                                 "production rule expects head and body list");
          return GLR_REWRITE_STATUS_PARSE_ERROR;
        }

      for (body_item = body_list->list; body_item != NULL;
           body_item = body_item->next)
        {
          body_length++;
        }

      rule.data.production.body = calloc (body_length == 0 ? 1 : body_length,
                                          sizeof (*rule.data.production.body));
      if (rule.data.production.body == NULL)
        {
          return GLR_REWRITE_STATUS_MEMORY_ERROR;
        }
      rule.data.production.body_length = body_length;

      body_item = body_list->list;
      for (size_t i = 0; i < body_length; i++, body_item = body_item->next)
        {
          const char *name = glr_rewrite_atom_text (body_item);
          if (name == NULL)
            {
              free (rule.data.production.body);
              glr_rewrite_set_error (error_buffer, error_size,
                                     "production body items must be atoms");
              return GLR_REWRITE_STATUS_PARSE_ERROR;
            }
          rule.data.production.body[i].type = GLR_SYMBOL_NONTERMINAL;
          rule.data.production.body[i].name = (char *)name;
        }
    }
  else if (strcmp (op, "remove-epsilon-productions") == 0)
    {
      rule.kind = GLR_REWRITE_RULE_REMOVE_EPSILON_PRODUCTIONS;
    }
  else if (strcmp (op, "remove-unit-productions") == 0)
    {
      rule.kind = GLR_REWRITE_RULE_REMOVE_UNIT_PRODUCTIONS;
    }
  else if (strcmp (op, "eliminate-useless-symbols") == 0)
    {
      rule.kind = GLR_REWRITE_RULE_REMOVE_USELESS_SYMBOLS;
    }
  else if (strcmp (op, "remove-left-recursion") == 0)
    {
      rule.kind = GLR_REWRITE_RULE_REMOVE_LEFT_RECURSION;
    }
  else if (strcmp (op, "left-factor") == 0)
    {
      rule.kind = GLR_REWRITE_RULE_LEFT_FACTOR;
    }
  else if (strcmp (op, "make-lr-compatible") == 0)
    {
      rule.kind = GLR_REWRITE_RULE_MAKE_LR_COMPATIBLE;
    }
  else if (strcmp (op, "eliminate-ambiguity") == 0)
    {
      rule.kind = GLR_REWRITE_RULE_ELIMINATE_AMBIGUITY;
    }
  else
    {
      glr_rewrite_set_error (error_buffer, error_size,
                             "unknown rewrite rule");
      return GLR_REWRITE_STATUS_PARSE_ERROR;
    }

  if (rule.kind == GLR_REWRITE_RULE_ADD_PRODUCTION
      || rule.kind == GLR_REWRITE_RULE_DROP_PRODUCTION)
    {
      glr_rewrite_status_t status
          = glr_rewrite_append_rule_copy (program, &rule);
      free (rule.data.production.body);
      return status;
    }

  return glr_rewrite_append_rule_copy (program, &rule);
}

glr_rewrite_program_t *
glr_rewrite_program_create (const char *name)
{
  glr_rewrite_program_t *program = calloc (1, sizeof (*program));

  if (program == NULL)
    {
      return NULL;
    }

  program->name = glr_rewrite_strdup (name != NULL ? name : "anonymous");
  if (program->name == NULL)
    {
      free (program);
      return NULL;
    }

  return program;
}

void
glr_rewrite_program_destroy (glr_rewrite_program_t *program)
{
  size_t i;

  if (program == NULL)
    {
      return;
    }

  for (i = 0; i < program->rule_count; i++)
    {
      glr_rewrite_free_rule (&program->rules[i]);
    }
  free (program->rules);
  free (program->name);
  free (program);
}

glr_rewrite_status_t
glr_rewrite_program_add_rule (glr_rewrite_program_t *program,
                              const glr_rewrite_rule_t *rule)
{
  return glr_rewrite_append_rule_copy (program, rule);
}

glr_rewrite_program_t *
glr_rewrite_program_parse (const char *source, size_t length,
                           char *error_buffer, size_t error_buffer_size)
{
  sexp_t *root;
  glr_rewrite_program_t *program;
  const sexp_t *node;

  if (source == NULL)
    {
      glr_rewrite_set_error (error_buffer, error_buffer_size,
                             "source buffer is NULL");
      return NULL;
    }

  root = parse_sexp ((char *)source, length);
  if (root == NULL)
    {
      glr_rewrite_set_error (error_buffer, error_buffer_size,
                             "failed to parse GRL S-expression");
      return NULL;
    }

  if (root->ty != SEXP_LIST || root->list == NULL
      || !glr_rewrite_is_atom (root->list, "rewrite"))
    {
      destroy_sexp (root);
      glr_rewrite_set_error (error_buffer, error_buffer_size,
                             "program must start with (rewrite ...) ");
      return NULL;
    }

  program = glr_rewrite_program_create ("rewrite");
  if (program == NULL)
    {
      destroy_sexp (root);
      glr_rewrite_set_error (error_buffer, error_buffer_size,
                             "out of memory creating program");
      return NULL;
    }

  for (node = root->list->next; node != NULL; node = node->next)
    {
      if (node->ty == SEXP_LIST && node->list != NULL
          && glr_rewrite_is_atom (node->list, "name"))
        {
          const char *name = glr_rewrite_atom_text (node->list->next);
          if (name != NULL)
            {
              free (program->name);
              program->name = glr_rewrite_strdup (name);
            }
          continue;
        }

      if (node->ty == SEXP_LIST && node->list != NULL
          && glr_rewrite_is_atom (node->list, "rules"))
        {
          const sexp_t *rule_node;
          for (rule_node = node->list->next; rule_node != NULL;
               rule_node = rule_node->next)
            {
              if (glr_rewrite_parse_rule (program, rule_node, error_buffer,
                                          error_buffer_size)
                  != GLR_REWRITE_STATUS_OK)
                {
                  destroy_sexp (root);
                  glr_rewrite_program_destroy (program);
                  return NULL;
                }
            }
          continue;
        }

      if (glr_rewrite_parse_rule (program, node, error_buffer,
                                  error_buffer_size)
          != GLR_REWRITE_STATUS_OK)
        {
          destroy_sexp (root);
          glr_rewrite_program_destroy (program);
          return NULL;
        }
    }

  destroy_sexp (root);
  return program;
}

glr_rewrite_program_t *
glr_rewrite_program_load_file (const char *path, char *error_buffer,
                               size_t error_buffer_size)
{
  FILE *input;
  long size;
  char *buffer;
  size_t read_count;
  glr_rewrite_program_t *program;

  if (path == NULL)
    {
      glr_rewrite_set_error (error_buffer, error_buffer_size,
                             "path is NULL");
      return NULL;
    }

  input = fopen (path, "rb");
  if (input == NULL)
    {
      glr_rewrite_set_error (error_buffer, error_buffer_size,
                             "failed to open rewrite file");
      return NULL;
    }

  if (fseek (input, 0, SEEK_END) != 0)
    {
      fclose (input);
      glr_rewrite_set_error (error_buffer, error_buffer_size,
                             "failed to size rewrite file");
      return NULL;
    }

  size = ftell (input);
  if (size < 0 || fseek (input, 0, SEEK_SET) != 0)
    {
      fclose (input);
      glr_rewrite_set_error (error_buffer, error_buffer_size,
                             "failed to read rewrite file");
      return NULL;
    }

  buffer = calloc ((size_t)size + 1, 1);
  if (buffer == NULL)
    {
      fclose (input);
      glr_rewrite_set_error (error_buffer, error_buffer_size,
                             "out of memory reading rewrite file");
      return NULL;
    }

  read_count = fread (buffer, 1, (size_t)size, input);
  fclose (input);
  if (read_count != (size_t)size)
    {
      free (buffer);
      glr_rewrite_set_error (error_buffer, error_buffer_size,
                             "short read while loading rewrite file");
      return NULL;
    }

  program = glr_rewrite_program_parse (buffer, (size_t)size, error_buffer,
                                       error_buffer_size);
  free (buffer);
  return program;
}

glr_rewrite_status_t
glr_rewrite_program_apply (glr_grammar_t *grammar,
                           const glr_rewrite_program_t *program,
                           glr_rewrite_report_t *report)
{
  glr_rewrite_status_t status = GLR_REWRITE_STATUS_OK;
  size_t i;

  if (report != NULL)
    {
      memset (report, 0, sizeof (*report));
      report->status = GLR_REWRITE_STATUS_OK;
      snprintf (report->message, sizeof (report->message), "ok");
    }

  if (grammar == NULL || program == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  for (i = 0; i < program->rule_count; i++)
    {
      status = glr_rewrite_apply_rule (grammar, &program->rules[i]);
      if (report != NULL)
        {
          report->rules_attempted++;
        }
      if (status != GLR_REWRITE_STATUS_OK)
        {
          if (report != NULL)
            {
              report->status = status;
              snprintf (report->message, sizeof (report->message),
                        "rule %zu failed", i);
            }
          return status;
        }
      if (report != NULL)
        {
          report->rules_applied++;
        }
    }

  return GLR_REWRITE_STATUS_OK;
}

glr_rewrite_status_t
glr_rewrite_apply_rule (glr_grammar_t *grammar, const glr_rewrite_rule_t *rule)
{
  char const *body_names[128];
  size_t i;

  if (grammar == NULL || rule == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  switch (rule->kind)
    {
    case GLR_REWRITE_RULE_ADD_SYMBOL:
      return glr_rewrite_add_symbol (grammar, rule->data.symbol.type,
                                     rule->data.symbol.name);
    case GLR_REWRITE_RULE_DROP_SYMBOL:
      return glr_rewrite_drop_symbol (grammar, rule->data.symbol.name);
    case GLR_REWRITE_RULE_RENAME_SYMBOL:
      return glr_rewrite_rename_symbol (grammar,
                                        rule->data.rename_symbol.old_name,
                                        rule->data.rename_symbol.new_name);
    case GLR_REWRITE_RULE_SET_START:
      return glr_rewrite_set_start (grammar, rule->data.start_symbol_name);
    case GLR_REWRITE_RULE_ADD_PRODUCTION:
    case GLR_REWRITE_RULE_DROP_PRODUCTION:
      if (rule->data.production.body_length > 128)
        {
          return GLR_REWRITE_STATUS_UNSUPPORTED;
        }
      for (i = 0; i < rule->data.production.body_length; i++)
        {
          body_names[i] = rule->data.production.body[i].name;
        }
      return rule->kind == GLR_REWRITE_RULE_ADD_PRODUCTION
                 ? glr_rewrite_add_production (grammar,
                                               rule->data.production.head_name,
                                               body_names,
                                               rule->data.production.body_length)
                 : glr_rewrite_drop_production (
                       grammar, rule->data.production.head_name, body_names,
                       rule->data.production.body_length);
    case GLR_REWRITE_RULE_REMOVE_EPSILON_PRODUCTIONS:
      return glr_rewrite_remove_epsilon_productions (grammar);
    case GLR_REWRITE_RULE_REMOVE_UNIT_PRODUCTIONS:
      return glr_rewrite_remove_unit_productions (grammar);
    case GLR_REWRITE_RULE_REMOVE_USELESS_SYMBOLS:
      return glr_rewrite_remove_useless_symbols (grammar);
    case GLR_REWRITE_RULE_REMOVE_LEFT_RECURSION:
      return glr_rewrite_remove_left_recursion (grammar);
    case GLR_REWRITE_RULE_LEFT_FACTOR:
      return glr_rewrite_left_factor (grammar);
    case GLR_REWRITE_RULE_MAKE_LR_COMPATIBLE:
      return glr_rewrite_make_lr_compatible (grammar);
    case GLR_REWRITE_RULE_ELIMINATE_AMBIGUITY:
      return glr_rewrite_eliminate_ambiguity (grammar);
    }

  return GLR_REWRITE_STATUS_UNSUPPORTED;
}

glr_rewrite_status_t
glr_rewrite_add_symbol (glr_grammar_t *grammar, glr_symbol_type_t type,
                        const char *name)
{
  glr_symbol_t *existing;

  if (grammar == NULL || name == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  existing = glr_rewrite_find_symbol (grammar, name);
  if (existing != NULL)
    {
      if (existing->type != type)
        {
          return GLR_REWRITE_STATUS_CONFLICT;
        }
      return GLR_REWRITE_STATUS_OK;
    }

  return glr_grammar_add_symbol (grammar, type, name) < 0
             ? GLR_REWRITE_STATUS_MEMORY_ERROR
             : GLR_REWRITE_STATUS_OK;
}

glr_rewrite_status_t
glr_rewrite_drop_symbol (glr_grammar_t *grammar, const char *name)
{
  glr_symbol_t *symbol;
  size_t i;

  if (grammar == NULL || name == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  symbol = glr_rewrite_find_symbol (grammar, name);
  if (symbol == NULL)
    {
      return GLR_REWRITE_STATUS_NOT_FOUND;
    }

  for (i = grammar->production_count; i > 0; i--)
    {
      glr_production_t *production = grammar->productions[i - 1];
      bool references = production->head == symbol;
      for (size_t j = 0; !references && j < production->body_length; j++)
        {
          references = production->body[j] == symbol;
        }
      if (references)
        {
          glr_rewrite_remove_production_at (grammar, i - 1);
        }
    }

  for (i = 0; i < grammar->symbol_count; i++)
    {
      if (grammar->symbols[i] == symbol)
        {
          glr_rewrite_remove_symbol_at (grammar, i);
          return GLR_REWRITE_STATUS_OK;
        }
    }

  return GLR_REWRITE_STATUS_NOT_FOUND;
}

glr_rewrite_status_t
glr_rewrite_rename_symbol (glr_grammar_t *grammar, const char *old_name,
                           const char *new_name)
{
  glr_symbol_t *symbol;
  glr_symbol_t *collision;
  char *replacement;

  if (grammar == NULL || old_name == NULL || new_name == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  symbol = glr_rewrite_find_symbol (grammar, old_name);
  if (symbol == NULL)
    {
      return GLR_REWRITE_STATUS_NOT_FOUND;
    }

  collision = glr_rewrite_find_symbol (grammar, new_name);
  if (collision != NULL && collision != symbol)
    {
      return GLR_REWRITE_STATUS_CONFLICT;
    }

  replacement = (char *) glr_stringpool_intern (grammar->strings, new_name);
  if (replacement == NULL)
    {
      return GLR_REWRITE_STATUS_MEMORY_ERROR;
    }

  symbol->name = replacement;
  return GLR_REWRITE_STATUS_OK;
}

glr_rewrite_status_t
glr_rewrite_set_start (glr_grammar_t *grammar, const char *name)
{
  glr_symbol_t *symbol;

  if (grammar == NULL || name == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  symbol = glr_rewrite_find_symbol (grammar, name);
  if (symbol == NULL)
    {
      return GLR_REWRITE_STATUS_NOT_FOUND;
    }
  if (symbol->type != GLR_SYMBOL_NONTERMINAL)
    {
      return GLR_REWRITE_STATUS_CONFLICT;
    }

  grammar->start_symbol = symbol;
  return GLR_REWRITE_STATUS_OK;
}

glr_rewrite_status_t
glr_rewrite_add_production (glr_grammar_t *grammar, const char *head_name,
                            const char *const *body, size_t body_length)
{
  glr_symbol_t *head;
  glr_symbol_t *resolved_body[128];
  size_t i;

  if (grammar == NULL || head_name == NULL || body_length > 128)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  head = glr_rewrite_find_symbol (grammar, head_name);
  if (head == NULL)
    {
      return GLR_REWRITE_STATUS_NOT_FOUND;
    }
  if (head->type != GLR_SYMBOL_NONTERMINAL)
    {
      return GLR_REWRITE_STATUS_CONFLICT;
    }

  for (i = 0; i < body_length; i++)
    {
      resolved_body[i] = glr_rewrite_find_symbol (grammar, body[i]);
      if (resolved_body[i] == NULL)
        {
          return GLR_REWRITE_STATUS_NOT_FOUND;
        }
    }

  if (glr_rewrite_production_exists (grammar, head, resolved_body, body_length))
    {
      return GLR_REWRITE_STATUS_OK;
    }

  return glr_grammar_add_production (grammar, head->id, resolved_body,
                                     body_length)
             < 0
         ? GLR_REWRITE_STATUS_MEMORY_ERROR
         : GLR_REWRITE_STATUS_OK;
}

glr_rewrite_status_t
glr_rewrite_drop_production (glr_grammar_t *grammar, const char *head_name,
                             const char *const *body, size_t body_length)
{
  size_t i;
  bool removed = false;

  if (grammar == NULL || head_name == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  for (i = grammar->production_count; i > 0; i--)
    {
      if (glr_rewrite_production_equals_names (grammar->productions[i - 1],
                                               head_name, body, body_length))
        {
          glr_rewrite_remove_production_at (grammar, i - 1);
          removed = true;
        }
    }

  return removed ? GLR_REWRITE_STATUS_OK : GLR_REWRITE_STATUS_NOT_FOUND;
}

glr_rewrite_status_t
glr_rewrite_remove_epsilon_productions (glr_grammar_t *grammar)
{
  bool *nullable;
  bool changed = true;
  size_t i;
  glr_rewrite_status_t status;

  if (grammar == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  nullable = calloc (grammar->symbol_count == 0 ? 1 : grammar->symbol_count,
                     sizeof (*nullable));
  if (nullable == NULL)
    {
      return GLR_REWRITE_STATUS_MEMORY_ERROR;
    }

  while (changed)
    {
      changed = false;
      for (i = 0; i < grammar->production_count; i++)
        {
          glr_production_t *production = grammar->productions[i];
          bool all_nullable = true;

          if (production->body_length == 0)
            {
              if (!nullable[production->head->id])
                {
                  nullable[production->head->id] = true;
                  changed = true;
                }
              continue;
            }

          for (size_t j = 0; j < production->body_length; j++)
            {
              glr_symbol_t *symbol = production->body[j];
              if (symbol->type != GLR_SYMBOL_NONTERMINAL
                  || !nullable[symbol->id])
                {
                  all_nullable = false;
                  break;
                }
            }

          if (all_nullable && !nullable[production->head->id])
            {
              nullable[production->head->id] = true;
              changed = true;
            }
        }
    }

  status = glr_rewrite_expand_nullable (grammar, nullable);
  if (status != GLR_REWRITE_STATUS_OK)
    {
      free (nullable);
      return status;
    }

  for (i = grammar->production_count; i > 0; i--)
    {
      glr_production_t *production = grammar->productions[i - 1];
      if (production->body_length == 0 && production->head != grammar->start_symbol)
        {
          glr_rewrite_remove_production_at (grammar, i - 1);
        }
    }

  free (nullable);
  return GLR_REWRITE_STATUS_OK;
}

glr_rewrite_status_t
glr_rewrite_remove_unit_productions (glr_grammar_t *grammar)
{
  bool *unit_pairs;
  size_t n;
  size_t i;
  bool changed = true;

  if (grammar == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  n = grammar->symbol_count;
  unit_pairs = calloc (n == 0 ? 1 : n * n, sizeof (*unit_pairs));
  if (unit_pairs == NULL)
    {
      return GLR_REWRITE_STATUS_MEMORY_ERROR;
    }

  for (i = 0; i < n; i++)
    {
      unit_pairs[i * n + i] = true;
    }

  while (changed)
    {
      changed = false;
      for (i = 0; i < grammar->production_count; i++)
        {
          glr_production_t *production = grammar->productions[i];
          if (production->body_length == 1
              && production->body[0]->type == GLR_SYMBOL_NONTERMINAL)
            {
              size_t a = (size_t)production->head->id;
              size_t b = (size_t)production->body[0]->id;
              if (!unit_pairs[a * n + b])
                {
                  unit_pairs[a * n + b] = true;
                  changed = true;
                }
            }
        }

      for (size_t a = 0; a < n; a++)
        {
          for (size_t b = 0; b < n; b++)
            {
              if (!unit_pairs[a * n + b])
                {
                  continue;
                }
              for (size_t c = 0; c < n; c++)
                {
                  if (unit_pairs[b * n + c] && !unit_pairs[a * n + c])
                    {
                      unit_pairs[a * n + c] = true;
                      changed = true;
                    }
                }
            }
        }
    }

  for (size_t a = 0; a < n; a++)
    {
      glr_symbol_t *head = grammar->symbols[a];
      if (head->type != GLR_SYMBOL_NONTERMINAL)
        {
          continue;
        }

      for (size_t b = 0; b < n; b++)
        {
          glr_symbol_t *source = grammar->symbols[b];
          if (!unit_pairs[a * n + b] || source->type != GLR_SYMBOL_NONTERMINAL)
            {
              continue;
            }

          for (i = 0; i < grammar->production_count; i++)
            {
              glr_production_t *production = grammar->productions[i];
              if (production->head != source)
                {
                  continue;
                }
              if (production->body_length == 1
                  && production->body[0]->type == GLR_SYMBOL_NONTERMINAL)
                {
                  continue;
                }
              if (!glr_rewrite_production_exists (grammar, head, production->body,
                                                  production->body_length))
                {
                  if (glr_grammar_add_production (grammar, head->id,
                                                  production->body,
                                                  production->body_length)
                      < 0)
                    {
                      free (unit_pairs);
                      return GLR_REWRITE_STATUS_MEMORY_ERROR;
                    }
                }
            }
        }
    }

  for (i = grammar->production_count; i > 0; i--)
    {
      glr_production_t *production = grammar->productions[i - 1];
      if (production->body_length == 1
          && production->body[0]->type == GLR_SYMBOL_NONTERMINAL)
        {
          glr_rewrite_remove_production_at (grammar, i - 1);
        }
    }

  free (unit_pairs);
  return GLR_REWRITE_STATUS_OK;
}

glr_rewrite_status_t
glr_rewrite_remove_useless_symbols (glr_grammar_t *grammar)
{
  bool *productive;
  bool *reachable;
  bool *keep_symbols;
  bool *keep_productions;
  bool changed = true;
  size_t i;
  glr_rewrite_status_t status;

  if (grammar == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  productive = calloc (grammar->symbol_count == 0 ? 1 : grammar->symbol_count,
                       sizeof (*productive));
  reachable = calloc (grammar->symbol_count == 0 ? 1 : grammar->symbol_count,
                      sizeof (*reachable));
  keep_symbols = calloc (grammar->symbol_count == 0 ? 1 : grammar->symbol_count,
                         sizeof (*keep_symbols));
  keep_productions = calloc (
      grammar->production_count == 0 ? 1 : grammar->production_count,
      sizeof (*keep_productions));
  if (productive == NULL || reachable == NULL || keep_symbols == NULL
      || keep_productions == NULL)
    {
      free (productive);
      free (reachable);
      free (keep_symbols);
      free (keep_productions);
      return GLR_REWRITE_STATUS_MEMORY_ERROR;
    }

  for (i = 0; i < grammar->symbol_count; i++)
    {
      if (grammar->symbols[i]->type == GLR_SYMBOL_TERMINAL)
        {
          productive[i] = true;
        }
    }

  while (changed)
    {
      changed = false;
      for (i = 0; i < grammar->production_count; i++)
        {
          glr_production_t *production = grammar->productions[i];
          bool all_productive = true;
          for (size_t j = 0; j < production->body_length; j++)
            {
              if (!productive[production->body[j]->id])
                {
                  all_productive = false;
                  break;
                }
            }
          if (all_productive && !productive[production->head->id])
            {
              productive[production->head->id] = true;
              changed = true;
            }
        }
    }

  if (grammar->start_symbol != NULL)
    {
      reachable[grammar->start_symbol->id] = true;
      changed = true;
    }
  while (changed)
    {
      changed = false;
      for (i = 0; i < grammar->production_count; i++)
        {
          glr_production_t *production = grammar->productions[i];
          if (!reachable[production->head->id])
            {
              continue;
            }
          for (size_t j = 0; j < production->body_length; j++)
            {
              glr_symbol_t *symbol = production->body[j];
              if (symbol->type == GLR_SYMBOL_NONTERMINAL
                  && !reachable[symbol->id])
                {
                  reachable[symbol->id] = true;
                  changed = true;
                }
              else if (symbol->type == GLR_SYMBOL_TERMINAL)
                {
                  reachable[symbol->id] = true;
                }
            }
        }
    }

  for (i = 0; i < grammar->symbol_count; i++)
    {
      keep_symbols[i] = productive[i] && reachable[i];
    }

  for (i = 0; i < grammar->production_count; i++)
    {
      glr_production_t *production = grammar->productions[i];
      bool keep = keep_symbols[production->head->id];
      for (size_t j = 0; keep && j < production->body_length; j++)
        {
          keep = keep_symbols[production->body[j]->id];
        }
      keep_productions[i] = keep;
    }

  status = glr_rewrite_rebuild_grammar (grammar, keep_symbols, keep_productions);
  free (productive);
  free (reachable);
  free (keep_symbols);
  free (keep_productions);
  return status;
}

glr_rewrite_status_t
glr_rewrite_remove_left_recursion (glr_grammar_t *grammar)
{
  size_t generated_count = 0;
  size_t n;

  if (grammar == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  n = grammar->symbol_count;
  for (size_t i = 0; i < n; i++)
    {
      glr_symbol_t *ai = grammar->symbols[i];
      if (ai->type != GLR_SYMBOL_NONTERMINAL)
        {
          continue;
        }

      for (size_t j = 0; j < i; j++)
        {
          glr_symbol_t *aj = grammar->symbols[j];
          if (aj->type != GLR_SYMBOL_NONTERMINAL)
            {
              continue;
            }
          if (glr_rewrite_expand_indirect_left_recursion (grammar, ai, aj)
              != GLR_REWRITE_STATUS_OK)
            {
              return GLR_REWRITE_STATUS_MEMORY_ERROR;
            }
        }

      if (glr_rewrite_eliminate_direct_left_recursion (grammar, ai,
                                                       generated_count++)
          != GLR_REWRITE_STATUS_OK)
        {
          return GLR_REWRITE_STATUS_MEMORY_ERROR;
        }
    }

  return GLR_REWRITE_STATUS_OK;
}

glr_rewrite_status_t
glr_rewrite_left_factor (glr_grammar_t *grammar)
{
  size_t generated_count = 0;
  bool changed = true;

  if (grammar == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  while (changed)
    {
      changed = false;
      for (size_t i = 0; i < grammar->symbol_count; i++)
        {
          glr_symbol_t *symbol = grammar->symbols[i];
          if (symbol->type != GLR_SYMBOL_NONTERMINAL)
            {
              continue;
            }
          if (glr_rewrite_factor_head (grammar, symbol, &generated_count)
              == GLR_REWRITE_STATUS_OK)
            {
              changed = true;
              break;
            }
        }
    }

  return GLR_REWRITE_STATUS_OK;
}

glr_rewrite_status_t
glr_rewrite_make_lr_compatible (glr_grammar_t *grammar)
{
  glr_rewrite_status_t status;

  status = glr_rewrite_remove_epsilon_productions (grammar);
  if (status != GLR_REWRITE_STATUS_OK)
    {
      return status;
    }
  status = glr_rewrite_remove_unit_productions (grammar);
  if (status != GLR_REWRITE_STATUS_OK)
    {
      return status;
    }
  status = glr_rewrite_remove_left_recursion (grammar);
  if (status != GLR_REWRITE_STATUS_OK)
    {
      return status;
    }
  status = glr_rewrite_left_factor (grammar);
  if (status != GLR_REWRITE_STATUS_OK)
    {
      return status;
    }
  return glr_rewrite_remove_useless_symbols (grammar);
}

glr_rewrite_status_t
glr_rewrite_eliminate_ambiguity (glr_grammar_t *grammar)
{
  glr_rewrite_status_t status;
  glr_parse_table_t *table;
  char error[128];
  size_t conflicts;

  if (grammar == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  status = glr_rewrite_make_lr_compatible (grammar);
  if (status != GLR_REWRITE_STATUS_OK)
    {
      return status;
    }
  status = glr_rewrite_remove_useless_symbols (grammar);
  if (status != GLR_REWRITE_STATUS_OK)
    {
      return status;
    }

  /* Verify the result instead of assuming it. Normalization removes the
     conflicts it can; a grammar that is ambiguous by construction (the
     classic E -> E + E | E * E | n) survives, and reporting that is far more
     useful than returning success for a pass that changed nothing. */
  memset (error, 0, sizeof (error));
  table = glr_grammar_build_parse_table (grammar, error, sizeof (error));
  if (table == NULL)
    {
      return GLR_REWRITE_STATUS_CONFLICT;
    }
  conflicts = glr_parse_table_conflict_count (table);
  glr_parse_table_destroy (table);

  return conflicts == 0 ? GLR_REWRITE_STATUS_OK
                        : GLR_REWRITE_STATUS_CONFLICT;
}
