/**
 * AFL Fuzzing harness for parser operations (real libglr API).
 */

#include <glr/parser.h>
#include <glr/grammar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifndef __AFL_LOOP
#define __AFL_LOOP(n) (0)
#endif

#define MAX_INPUT_SIZE 4096

static glr_grammar_t *
create_simple_grammar (void)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  glr_symbol_t *body[3];

  if (!grammar)
    {
      return NULL;
    }

  {
    int expr = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL,
                                       "Expr");
    int num = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "n");
    int plus = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
    if (expr < 0 || num < 0 || plus < 0)
      {
        glr_grammar_destroy (grammar);
        return NULL;
      }
    body[0] = glr_grammar_get_symbol (grammar, num);
    glr_grammar_add_production (grammar, expr, body, 1);
    body[0] = glr_grammar_get_symbol (grammar, expr);
    body[1] = glr_grammar_get_symbol (grammar, plus);
    body[2] = glr_grammar_get_symbol (grammar, expr);
    glr_grammar_add_production (grammar, expr, body, 3);
    glr_grammar_set_start_symbol (grammar, expr);
  }

  return grammar;
}

static void
run_once (glr_grammar_t *grammar, const uint8_t *buffer, size_t len)
{
  glr_parser_t *parser;
  glr_parse_result_t result;

  if (len == 0)
    {
      return;
    }

  parser = glr_parser_create (grammar);
  if (!parser)
    {
      return;
    }

  result = glr_parse (parser, (const char *) buffer, len);
  (void) result.error;
  (void) glr_parser_stack_count (parser);
  (void) glr_parser_get_forest (parser);
  (void) glr_parser_get_error (parser);

  /* Incremental path: reparse with a one-byte edit window. */
  if (len > 2)
    {
      glr_forest_t *incremental = NULL;
      glr_parser_parse_incremental (parser, result.forest,
                                    (const char *) buffer, len,
                                    (const char *) buffer, len, 0, 0,
                                    &incremental);
      if (incremental != NULL && incremental != result.forest)
        {
          glr_forest_destroy (incremental);
        }
    }

  glr_parser_reset (parser);
  glr_parser_destroy (parser);
}

int
main (int argc, char **argv)
{
  uint8_t buffer[MAX_INPUT_SIZE];
  size_t len = 0;
  glr_grammar_t *grammar = create_simple_grammar ();

  if (!grammar)
    {
      return 1;
    }

  while (__AFL_LOOP (1000))
    {
      len = fread (buffer, 1, MAX_INPUT_SIZE, stdin);
      if (len == 0)
        {
          continue;
        }
      run_once (grammar, buffer, len);
    }

  if (argc > 1)
    {
      FILE *f = fopen (argv[1], "rb");
      if (!f)
        {
          glr_grammar_destroy (grammar);
          return 1;
        }
      len = fread (buffer, 1, MAX_INPUT_SIZE, f);
      fclose (f);
      run_once (grammar, buffer, len);
      glr_grammar_destroy (grammar);
      return 0;
    }

  {
    const uint8_t seed[] = "n+n";
    run_once (grammar, seed, sizeof (seed) - 1);
  }

  glr_grammar_destroy (grammar);
  return 0;
}
