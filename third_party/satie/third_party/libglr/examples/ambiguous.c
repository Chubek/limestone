/**
 * @file ambiguous.c
 * @brief Ambiguity handling example using LibGLR
 *
 * This example demonstrates how a genuinely ambiguous grammar behaves:
 *
 *   expr -> expr '+' expr
 *   expr -> expr '*' expr
 *   expr -> 'n'
 *
 * The grammar has no conflict-free LR(1) table, so libglr reports conflicts
 * when it builds one. The parser explores the conflicting configurations and
 * merges equivalent paths into a Shared Packed Parse Forest. Accepted stacks
 * sharing the same packed root are retained once; distinct readings remain
 * under the forest's symbol nodes.
 *
 * For "n*n+n" the root packs both possible associations, and
 * glr_forest_is_ambiguous() reports whether more than one derivation is
 * packed under the root.
 *
 * Usage: ./ambiguous <expression>
 * Example: ./ambiguous "n*n+n"
 */

#include <glr/glr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Build the deliberately ambiguous grammar
 */
static glr_grammar_t *
create_ambiguous_grammar (void)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int expr;
  int num;
  int plus;
  int multiply;
  glr_symbol_t *body[3];

  if (grammar == NULL)
    {
      return NULL;
    }

  expr = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "expr");
  num = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "n");
  plus = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
  multiply = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "*");

  /* expr -> expr '+' expr */
  body[0] = glr_grammar_get_symbol (grammar, expr);
  body[1] = glr_grammar_get_symbol (grammar, plus);
  body[2] = glr_grammar_get_symbol (grammar, expr);
  glr_grammar_add_production (grammar, expr, body, 3);

  /* expr -> expr '*' expr */
  body[1] = glr_grammar_get_symbol (grammar, multiply);
  glr_grammar_add_production (grammar, expr, body, 3);

  /* expr -> n */
  body[0] = glr_grammar_get_symbol (grammar, num);
  glr_grammar_add_production (grammar, expr, body, 1);

  glr_grammar_set_start_symbol (grammar, expr);
  return grammar;
}

/**
 * @brief Report how the parse went, including what ambiguity survived
 */
static void
print_visit (glr_forest_node_t *node, size_t depth, void *user_data)
{
  size_t *max_depth = user_data;

  if (max_depth != NULL && depth > *max_depth)
    {
      *max_depth = depth;
    }
  (void) node;
}

int
main (int argc, char *argv[])
{
  glr_grammar_t *grammar;
  glr_parser_t *parser;
  glr_parse_table_t *table;
  glr_parse_result_t result;
  char error[128];
  const char *input;

  if (argc < 2)
    {
      printf ("Usage: %s <expression>\n", argv[0]);
      printf ("Example: %s \"n*n+n\"\n", argv[0]);
      return 1;
    }
  input = argv[1];

  printf ("=== LibGLR Ambiguous Grammar Example ===\n\n");
  printf ("Input: %s\n\n", input);
  printf ("Grammar (ambiguous):\n");
  printf ("  expr -> expr '+' expr\n");
  printf ("  expr -> expr '*' expr\n");
  printf ("  expr -> n\n\n");

  grammar = create_ambiguous_grammar ();
  if (grammar == NULL)
    {
      fprintf (stderr, "failed to create grammar\n");
      return 1;
    }
  printf ("Grammar: %zu symbols, %zu productions\n", grammar->symbol_count,
          grammar->production_count);

  memset (error, 0, sizeof (error));
  table = glr_grammar_build_parse_table (grammar, error, sizeof (error));
  if (table == NULL)
    {
      fprintf (stderr, "failed to build parse table: %s\n", error);
      glr_grammar_destroy (grammar);
      return 1;
    }
  printf ("Parse table: %zu states, %zu conflicts\n", table->state_count,
          glr_parse_table_conflict_count (table));

  parser = glr_parser_create (grammar);
  if (parser == NULL)
    {
      fprintf (stderr, "failed to create parser\n");
      glr_parse_table_destroy (table);
      glr_grammar_destroy (grammar);
      return 1;
    }
  glr_parser_set_parse_table (parser, table, false);

  result = glr_parse (parser, input, strlen (input));

  printf ("\n=== Parse Result ===\n");
  printf ("Error: %d\n", result.error);
  printf ("Position: %zu\n", result.position);
  printf ("Input consumed: %zu/%zu\n", result.position, strlen (input));
  printf ("Accepted roots: %zu\n", glr_parser_stack_count (parser));

  if (result.error != GLR_PARSE_SUCCESS)
    {
      printf ("\nParse failed.\n");
    }
  else
    {
      glr_forest_t *forest = result.forest;
      size_t max_depth = 0;

      printf ("\nParse succeeded.\n");
      if (forest != NULL)
        {
          size_t visited = glr_forest_visit (forest, forest->root, print_visit,
                                             &max_depth);
          printf ("Forest: %zu packed nodes, %zu reachable from the root\n",
                  glr_forest_total_nodes (forest), visited);
          printf ("Packed node depth: %zu\n", max_depth);
          printf ("Root production alternatives: %zu\n", forest->root->child_count);
          printf ("More than one derivation packed: %s\n",
                  glr_forest_is_ambiguous (forest->root) ? "yes" : "no");
        }
      printf ("\nPacked alternatives preserve the input's distinct readings.\n"
              "Register a disambiguator to select among conflicting LR actions.\n");
    }

  glr_parser_destroy (parser);
  glr_parse_table_destroy (table);
  glr_grammar_destroy (grammar);

  return result.error == GLR_PARSE_SUCCESS ? 0 : 1;
}
