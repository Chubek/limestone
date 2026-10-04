#include "test_common.h"

#include <glr/disambiguate.h>
#include <glr/grammar.h>
#include <glr/parser.h>

static int destroy_calls = 0;

static void
count_destroy (void *user_data)
{
  int *value = user_data;
  if (value != NULL)
    {
      *value += 1;
    }
  destroy_calls++;
}

static int
precedence_resolver (const glr_disambig_context_t *context,
                     const glr_disambig_candidate_t *candidate,
                     void *user_data)
{
  (void)context;
  (void)user_data;
  return candidate->precedence;
}

static glr_disambig_associativity_t
assoc_resolver (const glr_disambig_context_t *context,
                const glr_disambig_candidate_t *candidate, void *user_data)
{
  (void)context;
  (void)user_data;
  return candidate->associativity;
}

static bool
semantic_predicate (const glr_disambig_context_t *context,
                    const glr_disambig_candidate_t *candidate,
                    void *user_data)
{
  int threshold = *(int *)user_data;
  (void)context;
  return candidate->precedence >= threshold;
}

static double
score_node (const glr_disambig_context_t *context, const glr_forest_node_t *node,
            const glr_disambig_candidate_t *candidate, void *user_data)
{
  double bias = *(double *)user_data;
  (void)context;
  (void)candidate;
  return node != NULL ? bias : 0.0;
}

static glr_grammar_t *
make_grammar (void)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int expr;

  if (grammar == NULL)
    {
      return NULL;
    }

  expr = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Expr");
  glr_grammar_set_start_symbol (grammar, expr);
  return grammar;
}

static void
seed_context (glr_disambig_context_t *context, glr_disambig_candidate_t *candidates,
              size_t count)
{
  memset (context, 0, sizeof (*context));
  context->candidates = candidates;
  context->candidate_count = count;
}

GLR_TEST_CASE (test_context_helpers)
{
  glr_disambig_context_t context;
  glr_disambig_candidate_t candidates[3] = { 0 };

  glr_test_begin ("disambiguation helpers");
  seed_context (&context, candidates, 3);
  GLR_TEST_ASSERT_EQ (glr_disambig_context_active_count (&context), 3,
                      "all candidates should start active");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_reject_candidate (&context, 1), 0,
                      "reject should succeed");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_active_count (&context), 2,
                      "one candidate should be rejected");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_select_candidate (&context, 2), 0,
                      "select should succeed");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_active_count (&context), 1,
                      "select should leave one active candidate");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_last_active (&context), 2,
                      "winner should be candidate 2");
  glr_test_end ();
}

GLR_TEST_CASE (test_parser_hook_order)
{
  glr_grammar_t *grammar = make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_disambig_context_t context;
  glr_disambig_candidate_t candidates[2] = { 0 };
  glr_disambig_hook_t *high;
  glr_disambig_hook_t *low;
  size_t winner = SIZE_MAX;
  int state = 0;

  glr_test_begin ("parser hook ordering");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");

  high = glr_disambig_precedence_hook_create ("high", 20, precedence_resolver,
                                              &state, NULL);
  low = glr_disambig_precedence_hook_create ("low", 10, precedence_resolver,
                                             &state, NULL);
  GLR_TEST_ASSERT_NOT_NULL (high, "high-priority hook should be created");
  GLR_TEST_ASSERT_NOT_NULL (low, "low-priority hook should be created");
  GLR_TEST_ASSERT_EQ (glr_parser_add_disambiguator (parser, low), 0,
                      "low hook should register");
  GLR_TEST_ASSERT_EQ (glr_parser_add_disambiguator (parser, high), 0,
                      "high hook should register");
  GLR_TEST_ASSERT (glr_test_string_eq (parser->disambig_hooks->name, "high"),
                   "higher priority hook should run first");

  candidates[0].precedence = 2;
  candidates[1].precedence = 8;
  seed_context (&context, candidates, 2);
  GLR_TEST_ASSERT_EQ (glr_parser_run_disambiguators (parser, &context, &winner),
                      GLR_DISAMBIG_RESOLVED,
                      "precedence hook should resolve ambiguity");
  GLR_TEST_ASSERT_EQ (winner, 1, "higher precedence candidate should win");

  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_standard_hooks)
{
  glr_disambig_context_t context;
  glr_disambig_candidate_t candidates[3] = { 0 };
  glr_forest_node_t node = { 0 };
  glr_disambig_hook_t *hook;
  size_t winner = SIZE_MAX;
  int threshold = 5;
  double bias = 0.5;

  glr_test_begin ("standard disambiguators");

  candidates[0].precedence = 4;
  candidates[1].precedence = 9;
  candidates[2].precedence = 9;
  seed_context (&context, candidates, 3);
  hook = glr_disambig_precedence_hook_create (NULL, 0, precedence_resolver, NULL,
                                              NULL);
  GLR_TEST_ASSERT_NOT_NULL (hook, "precedence hook should be created");
  GLR_TEST_ASSERT_EQ (hook->fn (&context, &winner, hook->user_data),
                      GLR_DISAMBIG_NO_MATCH,
                      "tied precedence should leave ambiguity unresolved");
  GLR_TEST_ASSERT (candidates[0].rejected,
                   "lower precedence candidate should be rejected");
  GLR_TEST_ASSERT (!candidates[1].rejected && !candidates[2].rejected,
                   "top-precedence candidates should remain active");
  glr_disambig_hook_destroy (hook);

  memset (candidates, 0, sizeof (candidates));
  candidates[0].precedence = 7;
  candidates[0].associativity = GLR_DISAMBIG_ASSOC_LEFT;
  candidates[0].split_position = 2;
  candidates[0].has_split_position = true;
  candidates[1].precedence = 7;
  candidates[1].associativity = GLR_DISAMBIG_ASSOC_LEFT;
  candidates[1].split_position = 5;
  candidates[1].has_split_position = true;
  seed_context (&context, candidates, 2);
  hook = glr_disambig_associativity_hook_create (NULL, 0, precedence_resolver,
                                                 assoc_resolver, NULL, NULL);
  GLR_TEST_ASSERT_EQ (hook->fn (&context, &winner, hook->user_data),
                      GLR_DISAMBIG_RESOLVED,
                      "associativity hook should resolve");
  GLR_TEST_ASSERT_EQ (winner, 1,
                      "left associativity should prefer the rightmost split");
  glr_disambig_hook_destroy (hook);

  memset (candidates, 0, sizeof (candidates));
  candidates[0].precedence = 3;
  candidates[1].precedence = 8;
  seed_context (&context, candidates, 2);
  hook = glr_disambig_semantic_hook_create (NULL, 0, semantic_predicate,
                                            &threshold, NULL);
  GLR_TEST_ASSERT_EQ (hook->fn (&context, &winner, hook->user_data),
                      GLR_DISAMBIG_RESOLVED,
                      "semantic hook should reject low-precedence candidate");
  GLR_TEST_ASSERT_EQ (winner, 1, "semantic hook should keep the passing candidate");
  glr_disambig_hook_destroy (hook);

  memset (candidates, 0, sizeof (candidates));
  node.child_count = 0;
  candidates[0].node = &node;
  candidates[0].score = 3.0;
  candidates[1].node = &node;
  candidates[1].score = 1.0;
  seed_context (&context, candidates, 2);
  hook = glr_disambig_dynamic_programming_hook_create (NULL, 0, score_node, &bias,
                                                       NULL);
  GLR_TEST_ASSERT_EQ (hook->fn (&context, &winner, hook->user_data),
                      GLR_DISAMBIG_RESOLVED,
                      "dynamic-programming hook should resolve distinct scores");
  GLR_TEST_ASSERT_EQ (winner, 1, "lower total score should win");
  glr_disambig_hook_destroy (hook);

  memset (candidates, 0, sizeof (candidates));
  candidates[0].node = &node;
  candidates[0].probability = 0.2;
  candidates[1].node = &node;
  candidates[1].probability = 0.8;
  seed_context (&context, candidates, 2);
  hook = glr_disambig_probability_hook_create (NULL, 0, NULL, NULL, NULL);
  GLR_TEST_ASSERT_EQ (hook->fn (&context, &winner, hook->user_data),
                      GLR_DISAMBIG_RESOLVED,
                      "probability hook should resolve distinct probabilities");
  GLR_TEST_ASSERT_EQ (winner, 1, "highest probability should win");
  glr_disambig_hook_destroy (hook);

  glr_test_end ();
}

GLR_TEST_CASE (test_hook_destroy_callbacks)
{
  glr_grammar_t *grammar = make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  int destroy_state = 0;
  glr_disambig_hook_t *hook;

  glr_test_begin ("hook destroy callbacks");
  destroy_calls = 0;
  hook = glr_disambig_hook_create ("cleanup", 1, NULL, &destroy_state,
                                   count_destroy);
  GLR_TEST_ASSERT_NULL (hook, "hook create should reject NULL callback");

  hook = glr_disambig_precedence_hook_create ("cleanup", 1, precedence_resolver,
                                              &destroy_state, count_destroy);
  GLR_TEST_ASSERT_NOT_NULL (hook, "cleanup hook should be created");
  GLR_TEST_ASSERT_EQ (glr_parser_add_disambiguator (parser, hook), 0,
                      "cleanup hook should register");
  glr_parser_clear_disambiguators (parser);
  GLR_TEST_ASSERT_EQ (destroy_state, 1,
                      "custom destroy callback should run exactly once");
  GLR_TEST_ASSERT_EQ (destroy_calls, 1,
                      "destroy helper should be called exactly once");

  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

int
main (void)
{
  GLR_TEST_INIT;

  printf ("=== LibGLR Disambiguation Tests ===\n\n");
  test_context_helpers (&stats);
  test_parser_hook_order (&stats);
  test_standard_hooks (&stats);
  test_hook_destroy_callbacks (&stats);
  return glr_test_finish ("LibGLR Disambiguation", stats);
}
