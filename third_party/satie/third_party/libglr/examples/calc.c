/** Scannerless calculator: patterns, $LEXEME, aliases, positional selection,
    and deferred semantic actions. Usage: ./calc "12.5 + 2 * (3 - 1)". */
#include <glr/glr.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct value_cell
{
  double value;
  struct value_cell *next;
} value_cell_t;

typedef struct
{
  glr_stringpool_t *strings;
  value_cell_t *values;
} calculator_t;

/* Semantic outputs are application-owned. Keep them in stable cells until the
   whole evaluation finishes, then release them together, including on error. */
static int
store_value (calculator_t *calculator, double number, void **value)
{
  value_cell_t *cell = malloc (sizeof (*cell));
  if (cell == NULL)
    return -1;
  cell->value = number;
  cell->next = calculator->values;
  calculator->values = cell;
  *value = &cell->value;
  return 0;
}

static int
number_action (const glr_semantic_context_t *context, void **value, void *data)
{
  calculator_t *calculator = context->user_data;
  glr_selection_t selection;
  const char *text;
  char *end;
  double number;
  (void) data;
  if (glr_select (&context->selection, "$LEXEME", &selection) != 0)
    return -1;
  text = glr_stringpool_intern_n (calculator->strings, selection.lexeme, selection.length);
  if (text == NULL)
    return -1;
  errno = 0;
  number = strtod (text, &end);
  if (errno == ERANGE || end != text + selection.length)
    return -1;
  return store_value (calculator, number, value);
}

static int
operator_action (const glr_semantic_context_t *context, void **value, void *data)
{
  glr_selection_t lhs, rhs;
  double left, right, answer;
  if (glr_select (&context->selection, "$lhs", &lhs) != 0
      || glr_select (&context->selection, "$rhs", &rhs) != 0)
    return -1;
  left = *(double *) lhs.value;
  right = *(double *) rhs.value;
  switch (*(const char *) data)
    {
    case '+': answer = left + right; break;
    case '-': answer = left - right; break;
    case '*': answer = left * right; break;
    case '/':
      if (right == 0)
        {
          fprintf (stderr, "division by zero\n");
          return -1;
        }
      answer = left / right;
      break;
    default: return -1;
    }
  return store_value (context->user_data, answer, value);
}

static int
parenthesis_action (const glr_semantic_context_t *context, void **value, void *data)
{
  glr_selection_t inner;
  (void) data;
  if (glr_select (&context->selection, "$2", &inner) != 0)
    return -1;
  *value = inner.value;
  return 0;
}

static int
add_operator (glr_grammar_t *grammar, int head, int left, int right, const char *op)
{
  int terminal = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, op);
  glr_symbol_t *body[] = { grammar->symbols[left], glr_grammar_get_symbol (grammar, terminal),
                           grammar->symbols[right] };
  int production = glr_grammar_add_production (grammar, head, body, 3);
  if (production < 0 || glr_production_set_alias (grammar, production, 1, "lhs") != 0
      || glr_production_set_alias (grammar, production, 3, "rhs") != 0)
    return -1;
  return glr_production_set_semantic_action (grammar, production, operator_action, (void *) op, NULL);
}

static glr_grammar_t *
build_grammar (void)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  glr_symbol_t *body[3];
  int expr, term, factor, number, lp, rp, production;
  if (grammar == NULL)
    return NULL;
  expr = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Expr");
  term = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Term");
  factor = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Factor");
  number = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "NUMBER");
  lp = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "(");
  rp = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, ")");
  if (expr < 0 || term < 0 || factor < 0 || number < 0 || lp < 0 || rp < 0)
    goto fail;
  if (glr_scannerless_set_pattern (grammar, number,
        "([0-9]+(\\.[0-9]*)?|\\.[0-9]+)", NULL, 0) != 0)
    goto fail;
  body[0] = grammar->symbols[term];
  if (glr_grammar_add_production (grammar, expr, body, 1) < 0)
    goto fail;
  body[0] = grammar->symbols[factor];
  if (glr_grammar_add_production (grammar, term, body, 1) < 0)
    goto fail;
  body[0] = grammar->symbols[number];
  production = glr_grammar_add_production (grammar, factor, body, 1);
  if (glr_production_set_semantic_action (grammar, production, number_action, NULL, NULL) != 0)
    goto fail;
  if (add_operator (grammar, expr, expr, term, "+") != 0
      || add_operator (grammar, expr, expr, term, "-") != 0
      || add_operator (grammar, term, term, factor, "*") != 0
      || add_operator (grammar, term, term, factor, "/") != 0)
    goto fail;
  body[0] = grammar->symbols[lp];
  body[1] = grammar->symbols[expr];
  body[2] = grammar->symbols[rp];
  production = glr_grammar_add_production (grammar, factor, body, 3);
  if (glr_production_set_semantic_action (grammar, production, parenthesis_action, NULL, NULL) != 0
      || glr_grammar_set_start_symbol (grammar, expr) != 0)
    goto fail;
  return grammar;
fail:
  glr_grammar_destroy (grammar);
  return NULL;
}

int
main (int argc, char **argv)
{
  calculator_t calculator = { 0 };
  glr_grammar_t *grammar;
  glr_parser_t *parser;
  glr_parse_result_t result;
  int status = 1;
  if (argc != 2)
    {
      fprintf (stderr, "Usage: %s <expression>\n", argv[0]);
      return 1;
    }
  grammar = build_grammar ();
  calculator.strings = glr_stringpool_create ();
  parser = glr_parser_create (grammar);
  if (parser == NULL || calculator.strings == NULL)
    goto cleanup;
  glr_parser_set_user_data (parser, &calculator);
  glr_parser_set_trivia (parser, " ");
  result = glr_parse (parser, argv[1], strlen (argv[1]));
  if (result.error != GLR_PARSE_SUCCESS)
    fprintf (stderr, "Parse failed: error %d (semantic %d) at byte %zu\n",
             result.error, result.semantic_error, result.position);
  else
    {
      printf ("Result: %g\n", *(double *) result.semantic_value);
      status = 0;
    }
cleanup:
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_stringpool_destroy (calculator.strings);
  while (calculator.values != NULL)
    {
      value_cell_t *next = calculator.values->next;
      free (calculator.values);
      calculator.values = next;
    }
  return status;
}
