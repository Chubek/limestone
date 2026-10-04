#include "test_common.h"

#include <glr/disambiguate.h>
#include <glr/grammar.h>
#include <glr/parser.h>

static bool
prefer_user_flag (const glr_disambig_context_t *context,
                  const glr_disambig_candidate_t *candidate, void *user_data)
{
  (void)context;
  return candidate->user_data == user_data;
}

/* Expr -> Term, Term -> a, with "a" as the only terminal, so the
   grammar's language is exactly {"a"}. */
static glr_grammar_t *
make_grammar (void)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int expr;
  int term;
  int a;
  glr_symbol_t *body[1];

  if (grammar == NULL)
    {
      return NULL;
    }

  expr = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Expr");
  term = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Term");
  a = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "a");
  body[0] = glr_grammar_get_symbol (grammar, term);
  glr_grammar_add_production (grammar, expr, body, 1);
  body[0] = glr_grammar_get_symbol (grammar, a);
  glr_grammar_add_production (grammar, term, body, 1);
  glr_grammar_set_start_symbol (grammar, expr);
  return grammar;
}

GLR_TEST_CASE (test_parser_driven_resolution)
{
  glr_grammar_t *grammar = make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_disambig_candidate_t candidates[2] = { 0 };
  glr_disambig_context_t context = { 0 };
  glr_disambig_hook_t *predicate_hook;
  size_t winner = SIZE_MAX;
  int preferred_tag = 7;

  glr_test_begin ("ambiguity pipeline with parser hooks");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");

  predicate_hook = glr_disambig_predicate_hook_create (
      "prefer-tag", 50, prefer_user_flag, &preferred_tag, NULL);
  GLR_TEST_ASSERT_NOT_NULL (predicate_hook, "predicate hook should be created");
  GLR_TEST_ASSERT_EQ (glr_parser_add_disambiguator (parser, predicate_hook), 0,
                      "predicate hook should register");

  candidates[0].precedence = 1;
  candidates[0].user_data = &preferred_tag;
  candidates[1].precedence = 99;
  candidates[1].user_data = NULL;

  context.parser = parser;
  context.grammar = grammar;
  context.forest = glr_parser_get_forest (parser);
  context.candidates = candidates;
  context.candidate_count = 2;

  GLR_TEST_ASSERT_EQ (glr_parser_run_disambiguators (parser, &context, &winner),
                      GLR_DISAMBIG_RESOLVED,
                      "predicate hook should resolve the ambiguity");
  GLR_TEST_ASSERT_EQ (winner, 0, "preferred tagged candidate should win");
  GLR_TEST_ASSERT (candidates[1].rejected,
                   "non-preferred candidate should be rejected");

  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_parse_result_contract)
{
  glr_grammar_t *grammar = make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_result_t result;

  glr_test_begin ("parser result contract on minimal grammar");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");

  result = glr_parse (parser, "a", 1);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS,
                      "the grammar's only sentence should parse");
  GLR_TEST_ASSERT_NOT_NULL (result.forest, "parse result should expose a forest");
  GLR_TEST_ASSERT_EQ (result.position, 1, "parse should consume the full input");
  GLR_TEST_ASSERT_EQ (glr_parser_stack_count (parser), 1,
                      "parser should leave one active stack");

  {
    glr_parse_result_t bad = glr_parse (parser, "b", 1);
    GLR_TEST_ASSERT_EQ (bad.error, GLR_PARSE_ERROR_SYNTAX,
                        "input outside the language should be rejected");
    GLR_TEST_ASSERT_NULL (bad.forest,
                          "rejected input should not expose a forest");
  }

  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

int
main (void)
{
  GLR_TEST_INIT;

  printf ("=== LibGLR Ambiguity Tests ===\n\n");
  test_parser_driven_resolution (&stats);
  test_parse_result_contract (&stats);
  return glr_test_finish ("LibGLR Ambiguity", stats);
}
