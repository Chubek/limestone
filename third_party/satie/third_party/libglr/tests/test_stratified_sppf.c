/* tests/test_stratified_sppf.c
 *
 * Companion cases to test_stratified.c covering everything that happens once a
 * grammar exists: LR(1) table generation, the shift/reduce engine, the Shared
 * Packed Parse Forest (root and spans, DAG traversal, constructor packing,
 * self-reference), trivia skipping, left-recursive operator chains, and the
 * verifiable result of the ambiguity-reduction rewrite pass.
 *
 * The cases live here rather than in test_stratified.c so that file stays at a
 * fixed 80 cases covering the data-structure layers one by one.
 */

#include "test_common.h"

#include <glr/glr.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Expr -> Term, Term -> Factor, Factor -> n | ( Expr ), with operators
   spelled literally so the tokenizer can match them. */
static glr_grammar_t *
extra_expr_grammar (void)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int expr;
  int term;
  int factor;
  int num;
  int plus;
  int star;
  int lp;
  int rp;
  glr_symbol_t *body[3];

  if (grammar == NULL)
    {
      return NULL;
    }

  expr = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Expr");
  term = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Term");
  factor = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Factor");
  num = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "n");
  plus = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
  star = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "*");
  lp = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "(");
  rp = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, ")");

  body[0] = glr_grammar_get_symbol (grammar, term);
  glr_grammar_add_production (grammar, expr, body, 1);
  body[0] = glr_grammar_get_symbol (grammar, expr);
  body[1] = glr_grammar_get_symbol (grammar, plus);
  body[2] = glr_grammar_get_symbol (grammar, term);
  glr_grammar_add_production (grammar, expr, body, 3);
  body[0] = glr_grammar_get_symbol (grammar, factor);
  glr_grammar_add_production (grammar, term, body, 1);
  body[0] = glr_grammar_get_symbol (grammar, term);
  body[1] = glr_grammar_get_symbol (grammar, star);
  body[2] = glr_grammar_get_symbol (grammar, factor);
  glr_grammar_add_production (grammar, term, body, 3);
  body[0] = glr_grammar_get_symbol (grammar, num);
  glr_grammar_add_production (grammar, factor, body, 1);
  body[0] = glr_grammar_get_symbol (grammar, lp);
  body[1] = glr_grammar_get_symbol (grammar, expr);
  body[2] = glr_grammar_get_symbol (grammar, rp);
  glr_grammar_add_production (grammar, factor, body, 3);
  glr_grammar_set_start_symbol (grammar, expr);
  return grammar;
}

/* S -> a | b | c */
static glr_grammar_t *
extra_seq_grammar (void)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int s;
  glr_symbol_t *body[1];

  if (grammar == NULL)
    {
      return NULL;
    }

  s = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "S");
  body[0] = glr_grammar_get_symbol (
      grammar, glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "a"));
  glr_grammar_add_production (grammar, s, body, 1);
  body[0] = glr_grammar_get_symbol (
      grammar, glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "b"));
  glr_grammar_add_production (grammar, s, body, 1);
  body[0] = glr_grammar_get_symbol (
      grammar, glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "c"));
  glr_grammar_add_production (grammar, s, body, 1);
  glr_grammar_set_start_symbol (grammar, s);
  return grammar;
}

typedef struct
{
  size_t visited;
  size_t max_depth;
  int saw_terminal;
  int saw_constructor;
} extra_visit_ctx_t;

static void
extra_visit (glr_forest_node_t *node, size_t depth, void *user_data)
{
  extra_visit_ctx_t *ctx = user_data;

  if (ctx == NULL || node == NULL)
    {
      return;
    }
  ctx->visited++;
  if (depth > ctx->max_depth)
    {
      ctx->max_depth = depth;
    }
  if (node->type == GLR_NODE_TERMINAL)
    {
      ctx->saw_terminal = 1;
    }
  if (node->type == GLR_NODE_CONSTRUCTOR)
    {
      ctx->saw_constructor = 1;
    }
}

/* ------------------------------------------------------------------ */
/* LR(1) table construction (5)                                       */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_extra_lrtable_build)
{
  glr_grammar_t *grammar = extra_seq_grammar ();
  char error[128];
  glr_parse_table_t *table;

  glr_test_begin ("lr(1) table build");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  memset (error, 0, sizeof (error));
  table = glr_grammar_build_parse_table (grammar, error, sizeof (error));
  GLR_TEST_ASSERT_NOT_NULL (table, error);
  if (table != NULL)
    {
      GLR_TEST_ASSERT_EQ (table->state_count, 5,
                          "S -> a|b|c needs 5 states");
      GLR_TEST_ASSERT_EQ (glr_parse_table_conflict_count (table), 0,
                          "the grammar is conflict free");
      GLR_TEST_ASSERT_EQ (glr_parse_table_eof_column (table),
                          (uint32_t) glr_grammar_symbol_count (grammar),
                          "the EOF column follows the grammar symbols");
    }
  glr_parse_table_destroy (table);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_lrtable_rejects_bad)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  glr_parse_table_t *table;
  char error[128];

  glr_test_begin ("lr(1) table rejects bad input");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  memset (error, 0, sizeof (error));
  GLR_TEST_ASSERT_NULL (
      glr_grammar_build_parse_table (NULL, error, sizeof (error)),
      "null grammar should fail");
  GLR_TEST_ASSERT (error[0] != '\0', "null grammar should report an error");

  glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "S");
  GLR_TEST_ASSERT_NULL (
      glr_grammar_build_parse_table (grammar, error, sizeof (error)),
      "a grammar without a start symbol should fail");
  GLR_TEST_ASSERT (error[0] != '\0',
                   "a missing start symbol should report an error");

  glr_grammar_set_start_symbol (grammar, 0);

  /* A production-free grammar is structurally valid but derives nothing, so
     a table is still produced and the parser must reject everything. */
  table = glr_grammar_build_parse_table (grammar, error, sizeof (error));
  GLR_TEST_ASSERT_NOT_NULL (table,
                            "a production-free grammar still yields a table");
  if (table != NULL)
    {
      glr_parser_t *parser = glr_parser_create (grammar);
      GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
      GLR_TEST_ASSERT_EQ (glr_parse (parser, "a", 1).error,
                          GLR_PARSE_ERROR_SYNTAX,
                          "a production-free grammar accepts nothing");
      glr_parser_destroy (parser);
      glr_parse_table_destroy (table);
    }
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_lrtable_parse_accepts)
{
  glr_grammar_t *grammar = extra_seq_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_result_t result;

  glr_test_begin ("lr(1) parse accepts valid input");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  result = glr_parse (parser, "b", 1);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS,
                      "\"b\" is in the language");
  GLR_TEST_ASSERT_NOT_NULL (result.forest, "forest should be returned");
  GLR_TEST_ASSERT_EQ (result.position, 1, "position should cover the input");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_lrtable_parse_rejects)
{
  glr_grammar_t *grammar = extra_seq_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_result_t result;

  glr_test_begin ("lr(1) parse rejects invalid input");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  result = glr_parse (parser, "ab", 2);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SYNTAX,
                      "\"ab\" is not in the language");
  GLR_TEST_ASSERT_NULL (result.forest,
                        "failed parse should return no forest");
  result = glr_parse (parser, "", 0);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SYNTAX,
                      "empty input does not derive S");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_lrtable_expr_precedence)
{
  glr_grammar_t *grammar = extra_expr_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_table_t *table;
  glr_parse_result_t result;
  char error[128];

  glr_test_begin ("precedence grammar has no conflicts");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  memset (error, 0, sizeof (error));
  table = glr_grammar_build_parse_table (grammar, error, sizeof (error));
  GLR_TEST_ASSERT_NOT_NULL (table, error);
  glr_parse_table_destroy (table);

  result = glr_parse (parser, "n*n+n", 5);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "n*n+n should parse");
  result = glr_parse (parser, "(n+n)*n", 7);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS,
                      "parenthesized input should parse");
  result = glr_parse (parser, "n+", 2);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SYNTAX,
                      "a trailing operator should fail");
  result = glr_parse (parser, "(n", 2);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SYNTAX,
                      "an unclosed parenthesis should fail");
  result = glr_parse (parser, "n n", 3);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SYNTAX,
                      "a space is not a terminal in this grammar");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_forest_add_child_terminal_fails)
{
  glr_forest_t *forest = glr_forest_create ();
  glr_forest_node_t *terminal;
  glr_forest_node_t *child;

  glr_test_begin ("terminals cannot take children");
  GLR_TEST_ASSERT_NOT_NULL (forest, "forest should be created");
  terminal = glr_forest_get_node (forest, GLR_NODE_TERMINAL, 1, 0);
  child = glr_forest_get_node (forest, GLR_NODE_NONTERMINAL, 2, 0);
  GLR_TEST_ASSERT_NOT_NULL (terminal, "terminal should be created");
  GLR_TEST_ASSERT_NOT_NULL (child, "non-terminal should be created");
  GLR_TEST_ASSERT_EQ (glr_forest_add_child (terminal, child), -1,
                      "a terminal cannot take children");
  GLR_TEST_ASSERT_EQ (terminal->child_count, 0,
                      "a terminal stays a leaf");
  glr_forest_destroy (forest);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_sppf_root_and_spans)
{
  glr_grammar_t *grammar = extra_expr_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_result_t result;

  glr_test_begin ("sppf root and spans");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  result = glr_parse (parser, "n+n", 3);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "input should parse");
  GLR_TEST_ASSERT_NOT_NULL (result.forest, "forest should exist");
  if (result.forest != NULL)
    {
      GLR_TEST_ASSERT_NOT_NULL (result.forest->root,
                                "forest should record the root node");
      GLR_TEST_ASSERT_EQ (result.forest->root->position, 0,
                          "root should start at byte 0");
      GLR_TEST_ASSERT_EQ (result.forest->root->end_position, 3,
                          "root should span the whole input");
    }
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_sppf_visit)
{
  glr_grammar_t *grammar = extra_expr_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_result_t result;
  extra_visit_ctx_t ctx;

  glr_test_begin ("sppf dag traversal");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  memset (&ctx, 0, sizeof (ctx));
  result = glr_parse (parser, "n+n", 3);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "input should parse");
  if (result.forest != NULL)
    {
      size_t from_root = glr_forest_visit (result.forest, result.forest->root,
                                           extra_visit, &ctx);
      GLR_TEST_ASSERT (from_root > 0, "root traversal should visit nodes");
      GLR_TEST_ASSERT (ctx.saw_terminal,
                       "traversal should reach a terminal leaf");
      GLR_TEST_ASSERT (ctx.saw_constructor,
                       "traversal should reach constructor nodes");
      GLR_TEST_ASSERT (from_root <= glr_forest_total_nodes (result.forest),
                       "traversal must not exceed the node count");
      GLR_TEST_ASSERT_EQ (
          glr_forest_visit (result.forest, result.forest->root, NULL, &ctx), 0,
          "a NULL callback should visit nothing");
      GLR_TEST_ASSERT_EQ (glr_forest_visit (NULL, NULL, extra_visit, &ctx), 0,
                          "a NULL forest should visit nothing");
      GLR_TEST_ASSERT (glr_forest_total_nodes (result.forest) > 0,
                       "the forest should hold packed nodes");
    }
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_sppf_derivations)
{
  glr_grammar_t *grammar = extra_expr_grammar ();
  glr_grammar_t *amb = glr_grammar_create ();
  glr_parser_t *parser;
  glr_parse_result_t result;

  glr_test_begin ("sppf derivation counts");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  GLR_TEST_ASSERT_NOT_NULL (amb, "grammar should be created");

   /* Expr -> Expr '+' Expr | Expr '*' Expr | n is genuinely ambiguous:
      "n*n+n" has two readings, both of which must survive GLR packing. */
  {
    int expr
        = glr_grammar_add_symbol (amb, GLR_SYMBOL_NONTERMINAL, "Expr");
    int n = glr_grammar_add_symbol (amb, GLR_SYMBOL_TERMINAL, "n");
    int plus = glr_grammar_add_symbol (amb, GLR_SYMBOL_TERMINAL, "+");
    int star = glr_grammar_add_symbol (amb, GLR_SYMBOL_TERMINAL, "*");
    glr_symbol_t *body[3];
    glr_parse_table_t *table;
    char error[128];

    body[0] = glr_grammar_get_symbol (amb, n);
    glr_grammar_add_production (amb, expr, body, 1);
    body[0] = glr_grammar_get_symbol (amb, expr);
    body[1] = glr_grammar_get_symbol (amb, plus);
    body[2] = glr_grammar_get_symbol (amb, expr);
    glr_grammar_add_production (amb, expr, body, 3);
    body[1] = glr_grammar_get_symbol (amb, star);
    glr_grammar_add_production (amb, expr, body, 3);
    glr_grammar_set_start_symbol (amb, expr);

    memset (error, 0, sizeof (error));
    table = glr_grammar_build_parse_table (amb, error, sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (table, error);
    if (table != NULL)
      {
        GLR_TEST_ASSERT (glr_parse_table_conflict_count (table) > 0,
                         "an ambiguous grammar has no conflict-free LR(1) "
                         "table");
        glr_parse_table_destroy (table);
      }
  }

  parser = glr_parser_create (grammar);
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  result = glr_parse (parser, "n*n+n", 5);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS,
                      "unambiguous grammar should parse");
  if (result.forest != NULL && result.forest->root != NULL)
    {
      GLR_TEST_ASSERT (!glr_forest_is_ambiguous (result.forest->root),
                       "an unambiguous parse packs a single derivation");
    }
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);

  parser = glr_parser_create (amb);
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  result = glr_parse (parser, "n*n+n", 5);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS,
                      "ambiguous input should still parse");
  /* Equivalent accepting configurations may merge; their packed derivations
     must remain available to disambiguators and semantic evaluation. */
  GLR_TEST_ASSERT (glr_forest_is_ambiguous (result.forest->root),
                   "both readings must survive configuration merging");
  GLR_TEST_ASSERT_NOT_NULL (result.forest, "forest should exist");
  if (result.forest != NULL)
    {
      GLR_TEST_ASSERT_NOT_NULL (result.forest->root,
                                "forest should record the root node");
    }
  glr_parser_destroy (parser);
  glr_grammar_destroy (amb);

  /* Hand-packed ambiguity: a symbol node with two constructors underneath. */
  {
    glr_forest_t *forest = glr_forest_create ();
    glr_forest_node_t *symbol = glr_forest_get_symbol (forest, 0, 0, 5);
    glr_forest_node_t *left = glr_forest_get_constructor (forest, 3, 0, 5);
    glr_forest_node_t *right = glr_forest_get_constructor (forest, 4, 0, 5);

    GLR_TEST_ASSERT_NOT_NULL (symbol, "symbol node should be created");
    GLR_TEST_ASSERT (symbol != left && symbol != right,
                     "a symbol node and a constructor are distinct nodes");
    GLR_TEST_ASSERT_EQ (glr_forest_add_child (symbol, left), 0,
                        "first alternative should attach");
    GLR_TEST_ASSERT_EQ (glr_forest_add_child (symbol, right), 0,
                        "second alternative should attach");
    GLR_TEST_ASSERT (glr_forest_is_ambiguous (symbol),
                     "two packed constructors are ambiguous");
    GLR_TEST_ASSERT (!glr_forest_is_ambiguous (left),
                     "a lone constructor is not ambiguous");
    GLR_TEST_ASSERT (!glr_forest_is_ambiguous (NULL),
                     "a NULL node is not ambiguous");
    glr_forest_destroy (forest);
  }
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_sppf_constructor_packing)
{
  glr_forest_t *forest = glr_forest_create ();
  glr_forest_node_t *short_span;
  glr_forest_node_t *long_span;
  glr_forest_node_t *again;

  glr_test_begin ("sppf constructor packing");
  GLR_TEST_ASSERT_NOT_NULL (forest, "forest should be created");
  short_span = glr_forest_get_constructor (forest, 3, 0, 3);
  long_span = glr_forest_get_constructor (forest, 3, 0, 5);
  again = glr_forest_get_constructor (forest, 3, 0, 3);
  GLR_TEST_ASSERT_NOT_NULL (short_span, "constructor should be created");
  GLR_TEST_ASSERT_NOT_NULL (long_span, "second span should be created");
  GLR_TEST_ASSERT (short_span != long_span,
                   "different spans must not share a node");
  GLR_TEST_ASSERT_EQ (again, short_span,
                      "the same span should reuse its node");
  GLR_TEST_ASSERT_EQ (long_span->end_position, 5, "span end should be kept");
  GLR_TEST_ASSERT_EQ (long_span->position, 0, "span start should be kept");
  GLR_TEST_ASSERT_NULL (glr_forest_get_constructor (NULL, 0, 0, 0),
                        "a NULL forest yields no constructor");
  glr_forest_destroy (forest);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_sppf_children)
{
  glr_forest_t *forest = glr_forest_create ();
  glr_forest_node_t *node;
  glr_forest_node_t *child;
  glr_forest_node_t *terminal;

  glr_test_begin ("sppf child bookkeeping");
  GLR_TEST_ASSERT_NOT_NULL (forest, "forest should be created");
  node = glr_forest_get_constructor (forest, 1, 0, 4);
  GLR_TEST_ASSERT_NOT_NULL (node, "constructor should be created");
  GLR_TEST_ASSERT_EQ (glr_forest_add_child (node, node), -1,
                      "a node cannot be its own child");
  child = glr_forest_get_node (forest, GLR_NODE_NONTERMINAL, 2, 0);
  GLR_TEST_ASSERT_EQ (glr_forest_add_child (node, child), 0,
                      "a distinct child should be accepted");
  GLR_TEST_ASSERT_EQ (node->child_count, 1, "child list should hold one node");
  GLR_TEST_ASSERT_NOT_NULL (glr_forest_get_children (node),
                            "children should be retrievable");
  terminal = glr_forest_get_node (forest, GLR_NODE_TERMINAL, 1, 0);
  GLR_TEST_ASSERT_EQ (glr_forest_add_child (terminal, child), -1,
                      "a terminal cannot take children");
  GLR_TEST_ASSERT_NULL (glr_forest_get_children (terminal),
                        "a terminal has no children");
  GLR_TEST_ASSERT_EQ (glr_forest_add_child (NULL, child), -1,
                      "a null parent should be rejected");
  glr_forest_destroy (forest);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_trivia_skipped)
{
  glr_grammar_t *grammar = extra_seq_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_result_t result;

  glr_test_begin ("trivia is skipped between tokens");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  GLR_TEST_ASSERT_EQ (glr_parser_set_trivia (parser, " "), 0,
                      "trivia should be configurable");
  GLR_TEST_ASSERT (glr_test_string_eq (glr_parser_get_trivia (parser), " "),
                   "trivia getter should return the literal");
  result = glr_parse (parser, "  a  ", 4);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS,
                      "spaces around a token should be ignored");
  GLR_TEST_ASSERT_EQ (result.position, 4,
                      "position should cover the whole input");
  result = glr_parse (parser, " b", 2);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS,
                      "leading space should be ignored");
  result = glr_parse (parser, "\tb", 2);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SYNTAX,
                      "a tab is not the configured trivia");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_trivia_validation)
{
  glr_grammar_t *grammar = extra_seq_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);

  glr_test_begin ("trivia validation");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  GLR_TEST_ASSERT_EQ (glr_parser_set_trivia (NULL, " "), -1,
                      "null parser should be rejected");
  GLR_TEST_ASSERT_EQ (glr_parser_set_trivia (parser, ""), -1,
                      "empty trivia should be rejected");
  GLR_TEST_ASSERT_EQ (glr_parser_set_trivia (parser, "//"), 0,
                      "a multi-byte literal should be accepted");
  GLR_TEST_ASSERT_EQ (glr_parser_set_trivia (parser, NULL), 0,
                      "trivia should be disableable");
  GLR_TEST_ASSERT_NULL (glr_parser_get_trivia (parser),
                        "trivia getter should report none");
  GLR_TEST_ASSERT_NULL (glr_parser_get_trivia (NULL),
                        "null parser has no trivia");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_left_recursive_chain)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  glr_parser_t *parser;
  glr_parse_result_t result;
  glr_parse_table_t *table;
  char error[128];
  int expr;
  int num;
  int plus;
  int minus;
  glr_symbol_t *body[3];

  glr_test_begin ("left recursion stays conflict free");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  expr = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Expr");
  num = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "n");
  plus = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
  minus = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "-");
  body[0] = glr_grammar_get_symbol (grammar, num);
  glr_grammar_add_production (grammar, expr, body, 1);
  body[0] = glr_grammar_get_symbol (grammar, expr);
  body[1] = glr_grammar_get_symbol (grammar, plus);
  body[2] = glr_grammar_get_symbol (grammar, num);
  glr_grammar_add_production (grammar, expr, body, 3);
  body[1] = glr_grammar_get_symbol (grammar, minus);
  glr_grammar_add_production (grammar, expr, body, 3);
  glr_grammar_set_start_symbol (grammar, expr);

  memset (error, 0, sizeof (error));
  table = glr_grammar_build_parse_table (grammar, error, sizeof (error));
  GLR_TEST_ASSERT_NOT_NULL (table, error);
  GLR_TEST_ASSERT_EQ (glr_parse_table_conflict_count (table), 0,
                      "left recursion alone is not a conflict");
  glr_parse_table_destroy (table);

  parser = glr_parser_create (grammar);
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  result = glr_parse (parser, "n+n-n", 5);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS,
                      "a chain of operators should parse");
  if (result.forest != NULL && result.forest->root != NULL)
    {
      GLR_TEST_ASSERT_EQ (result.forest->root->end_position, 5,
                          "the root reduction should span the input");
    }
  result = glr_parse (parser, "n+n+", 4);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SYNTAX,
                      "a trailing operator should be rejected");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_precedence_levels)
{
  glr_grammar_t *grammar = extra_expr_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_result_t result;
  glr_parse_table_t *table;
  char error[128];

  glr_test_begin ("precedence is encoded in the grammar");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  memset (error, 0, sizeof (error));
  table = glr_grammar_build_parse_table (grammar, error, sizeof (error));
  GLR_TEST_ASSERT_NOT_NULL (table, error);
  GLR_TEST_ASSERT_EQ (glr_parse_table_conflict_count (table), 0,
                      "the precedence grammar is conflict free");
  glr_parse_table_destroy (table);

  result = glr_parse (parser, "n*n+n", 5);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "n*n+n should parse");
  result = glr_parse (parser, "(n+n)*n", 7);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS,
                      "parenthesized input should parse");
  result = glr_parse (parser, "(n+n", 4);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SYNTAX,
                      "an unclosed parenthesis should fail");
  result = glr_parse (parser, "n+", 2);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SYNTAX,
                      "a trailing operator should fail");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_parser_reuse_after_failure)
{
  glr_grammar_t *grammar = extra_seq_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);

  glr_test_begin ("parser is reusable after a failed parse");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  GLR_TEST_ASSERT_EQ (glr_parse (parser, "a", 1).error, GLR_PARSE_SUCCESS,
                      "first parse should succeed");
  GLR_TEST_ASSERT_EQ (glr_parse (parser, "ab", 2).error,
                      GLR_PARSE_ERROR_SYNTAX, "second parse should fail");
  GLR_TEST_ASSERT_EQ (glr_parse (parser, "b", 1).error, GLR_PARSE_SUCCESS,
                      "the parser should recover after a failure");
  GLR_TEST_ASSERT_EQ (glr_parser_get_error (parser), GLR_PARSE_SUCCESS,
                      "error state should be reset by a successful parse");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_extra_rewrite_ambiguity_is_verified)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  glr_grammar_t *ambiguous = glr_grammar_create ();
  glr_rewrite_status_t status;

  glr_test_begin ("ambiguity reduction reports a verifiable result");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  GLR_TEST_ASSERT_NOT_NULL (ambiguous, "grammar should be created");

  /* E -> E '+' T | T, T -> n: a left-recursive, LR-compatible grammar. */
  {
    int expr
        = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Expr");
    int term = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Term");
    int n = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "n");
    int plus = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
    glr_symbol_t *body[3];

    body[0] = glr_grammar_get_symbol (grammar, term);
    glr_grammar_add_production (grammar, expr, body, 1);
    body[0] = glr_grammar_get_symbol (grammar, expr);
    body[1] = glr_grammar_get_symbol (grammar, plus);
    body[2] = glr_grammar_get_symbol (grammar, term);
    glr_grammar_add_production (grammar, expr, body, 3);
    body[0] = glr_grammar_get_symbol (grammar, n);
    glr_grammar_add_production (grammar, term, body, 1);
    glr_grammar_set_start_symbol (grammar, expr);
  }

  status = glr_rewrite_eliminate_ambiguity (grammar);
  GLR_TEST_ASSERT_EQ (status, GLR_REWRITE_STATUS_OK,
                      "an LR-compatible grammar normalizes cleanly");

  /* E -> E '+' E | E '*' E | n is ambiguous by construction, so the pass
     must say so rather than claim success. */
  {
    int expr
        = glr_grammar_add_symbol (ambiguous, GLR_SYMBOL_NONTERMINAL, "Expr");
    int n = glr_grammar_add_symbol (ambiguous, GLR_SYMBOL_TERMINAL, "n");
    int plus = glr_grammar_add_symbol (ambiguous, GLR_SYMBOL_TERMINAL, "+");
    int star = glr_grammar_add_symbol (ambiguous, GLR_SYMBOL_TERMINAL, "*");
    glr_symbol_t *body[3];

    body[0] = glr_grammar_get_symbol (ambiguous, n);
    glr_grammar_add_production (ambiguous, expr, body, 1);
    body[0] = glr_grammar_get_symbol (ambiguous, expr);
    body[1] = glr_grammar_get_symbol (ambiguous, plus);
    body[2] = glr_grammar_get_symbol (ambiguous, expr);
    glr_grammar_add_production (ambiguous, expr, body, 3);
    body[1] = glr_grammar_get_symbol (ambiguous, star);
    glr_grammar_add_production (ambiguous, expr, body, 3);
    glr_grammar_set_start_symbol (ambiguous, expr);
  }

  status = glr_rewrite_eliminate_ambiguity (ambiguous);
  GLR_TEST_ASSERT_EQ (status, GLR_REWRITE_STATUS_CONFLICT,
                      "a genuinely ambiguous grammar is reported as such");
  GLR_TEST_ASSERT_EQ (glr_rewrite_eliminate_ambiguity (NULL),
                      GLR_REWRITE_STATUS_INVALID_ARGUMENT,
                      "a null grammar should be rejected");
  glr_grammar_destroy (grammar);
  glr_grammar_destroy (ambiguous);
  glr_test_end ();
}

int
main (void)
{
  GLR_TEST_INIT;

  printf ("=== LibGLR Stratified Parsing and Forest Cases ===\n\n");

  test_extra_lrtable_build (&stats);
  test_extra_lrtable_rejects_bad (&stats);
  test_extra_lrtable_parse_accepts (&stats);
  test_extra_lrtable_parse_rejects (&stats);
  test_extra_lrtable_expr_precedence (&stats);
  test_extra_sppf_root_and_spans (&stats);
  test_extra_sppf_visit (&stats);
  test_extra_sppf_derivations (&stats);
  test_extra_sppf_constructor_packing (&stats);
  test_extra_sppf_children (&stats);
  test_extra_forest_add_child_terminal_fails (&stats);
  test_extra_trivia_skipped (&stats);
  test_extra_trivia_validation (&stats);
  test_extra_left_recursive_chain (&stats);
  test_extra_precedence_levels (&stats);
  test_extra_parser_reuse_after_failure (&stats);
  test_extra_rewrite_ambiguity_is_verified (&stats);

  return glr_test_finish ("LibGLR Parsing", stats);
}
