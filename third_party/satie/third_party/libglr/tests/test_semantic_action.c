#include "test_common.h"
#include <glr/glr.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
  long values[512];
  size_t count;
  size_t selections;
} evaluator_t;

static int epsilon_action (const glr_semantic_context_t *, void **, void *);

static int
number_action (const glr_semantic_context_t *context, void **value, void *data)
{
  evaluator_t *evaluation = context->user_data;
  glr_selection_t lexeme;
  long number = 0;
  (void) data;
  if (glr_select (&context->selection, "$LEXEME", &lexeme) != 0)
    return -1;
  for (size_t i = 0; i < lexeme.length; i++)
    number = number * 10 + lexeme.lexeme[i] - '0';
  evaluation->values[evaluation->count] = number;
  *value = &evaluation->values[evaluation->count++];
  return 0;
}

static int
sum_action (const glr_semantic_context_t *context, void **value, void *data)
{
  evaluator_t *evaluation = context->user_data;
  glr_selection_t left, right, position;
  (void) data;
  if (glr_select (&context->selection, "$lhs", &left) != 0
      || glr_select (&context->selection, "$rhs", &right) != 0
      || glr_select (&context->selection, "$3", &position) != 0
      || right.node != position.node || right.value != position.value)
    return -1;
  evaluation->selections++;
  evaluation->values[evaluation->count] = *(long *) left.value + *(long *) right.value;
  *value = &evaluation->values[evaluation->count++];
  return 0;
}

static glr_grammar_t *
sum_grammar (bool patterns)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int expr = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Expr");
  int number = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Number");
  int digits = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, patterns ? "DIGITS" : "1");
  int plus = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
  glr_symbol_t *body[] = { grammar->symbols[digits], NULL, NULL };
  int production = glr_grammar_add_production (grammar, number, body, 1);
  glr_production_set_semantic_action (grammar, production, number_action, NULL, NULL);
  body[0] = grammar->symbols[number];
  glr_grammar_add_production (grammar, expr, body, 1);
  body[0] = grammar->symbols[expr];
  body[1] = grammar->symbols[plus];
  body[2] = grammar->symbols[number];
  production = glr_grammar_add_production (grammar, expr, body, 3);
  glr_production_set_alias (grammar, production, 1, "lhs");
  glr_production_set_alias (grammar, production, 3, "rhs");
  glr_production_set_semantic_action (grammar, production, sum_action, NULL, NULL);
  if (patterns)
    glr_scannerless_set_pattern (grammar, digits, "[0-9]+", NULL, 0);
  glr_grammar_set_start_symbol (grammar, expr);
  return grammar;
}

GLR_TEST_CASE (deferred_evaluation_and_selectors)
{
  glr_grammar_t *grammar = sum_grammar (true);
  glr_parser_t *parser = glr_parser_create (grammar);
  evaluator_t evaluation = { { 0 }, 0, 0 };
  glr_parse_result_t result;
  glr_selection_t selection;
  glr_test_begin ("deferred bottom-up actions select aliases, positions, and LEXEME");
  glr_parser_set_user_data (parser, &evaluation);
  glr_parser_set_trivia (parser, " ");
  result = glr_parse (parser, "12 + 34 + 5", 11);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "sum should parse and evaluate");
  GLR_TEST_ASSERT_EQ (*(long *) result.semantic_value, 51, "semantic values should propagate");
  GLR_TEST_ASSERT_EQ (evaluation.count, 5, "three numbers and two sums evaluate once each");
  GLR_TEST_ASSERT_EQ (evaluation.selections, 2, "aliases and positions should select the same RHS");
  glr_forest_node_t *constructor = result.forest->root->children[0];
  glr_select_context_t context = {
    grammar->productions[2], constructor->children, constructor->child_count,
    NULL, "12 + 34 + 5", 11
  };
  GLR_TEST_ASSERT_EQ (glr_select (&context, "$rhs", &selection), 0, "alias should select a nonterminal");
  GLR_TEST_ASSERT_EQ (selection.length, 1, "right number should cover its input slice");
  GLR_TEST_ASSERT_EQ (selection.lexeme[0], '5', "source slice should contain the final number");
  GLR_TEST_ASSERT_EQ (glr_select (&context, "$LEXEME", &selection), -1,
                       "LEXEME is reserved for a single terminal");
  GLR_TEST_ASSERT_EQ (glr_select (&context, "$0", &selection), -1, "position zero is invalid");
  GLR_TEST_ASSERT_EQ (glr_select (&context, "$4", &selection), -1, "out-of-range position is invalid");
  GLR_TEST_ASSERT_EQ (glr_select (&context, "$999999999999999999999999", &selection), -1,
                       "position overflow should be rejected");
  GLR_TEST_ASSERT_EQ (glr_select (&context, "$missing", &selection), -1, "missing alias should fail");
  GLR_TEST_ASSERT_NULL (selection.node, "failed selection should be cleared");
  GLR_TEST_ASSERT_EQ (glr_production_set_alias (grammar, 2, 2, "lhs"), -1,
                       "duplicate aliases should fail atomically");
  GLR_TEST_ASSERT_EQ (glr_production_set_alias (grammar, 2, 2, "LEXEME"), -1,
                       "reserved alias should fail");
  evaluation.count = 0;
  GLR_TEST_ASSERT_EQ (glr_parse (parser, "12+?", 4).error, GLR_PARSE_ERROR_SYNTAX,
                       "invalid input should fail");
  GLR_TEST_ASSERT_EQ (evaluation.count, 0, "rejected parses must not invoke actions");
  char chain[399];
  for (size_t i = 0; i < sizeof chain; i++)
    chain[i] = i % 2 == 0 ? '1' : '+';
  result = glr_parse (parser, chain, sizeof chain);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "long left-recursive expression should evaluate");
  GLR_TEST_ASSERT_EQ (*(long *) result.semantic_value, 200, "iterative evaluation covers the full chain");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

static void
count_destruction (void *data)
{
  (*(int *) data)++;
}

GLR_TEST_CASE (incremental_actions_evaluate_complete_inputs)
{
  glr_test_begin ("incremental actions evaluate one complete accepted derivation");
  for (int patterns = 0; patterns <= 1; patterns++)
    {
      glr_grammar_t *grammar = sum_grammar (patterns != 0);
      glr_parser_t *parser = glr_parser_create (grammar);
      evaluator_t evaluation = { { 0 }, 0, 0 };
      glr_forest_t *updated = NULL;
      glr_parser_set_user_data (parser, &evaluation);
      glr_parse_result_t result = glr_parse (parser, "1+1+1", 5);
      GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "the initial expression should evaluate");
      glr_forest_t *old = glr_forest_clone (result.forest);
      GLR_TEST_ASSERT_NOT_NULL (old, "the old derivation should be independently owned");
      evaluation.count = evaluation.selections = 0;
      GLR_TEST_ASSERT_EQ (glr_parser_parse_incremental (parser, old, "1+1+1", 5,
                          "1+1+1+1", 7, 5, 5, &updated), 0,
                           "appending another term should evaluate the complete input");
      GLR_TEST_ASSERT_EQ (evaluation.count, 7,
                           "four numbers and three sums run once without fragment callbacks");
      GLR_TEST_ASSERT_EQ (evaluation.selections, 3, "each complete sum sees both operands");
      if (evaluation.count != 0)
        GLR_TEST_ASSERT_EQ (evaluation.values[evaluation.count - 1], 4,
                             "the final value includes the unchanged prefix");
      GLR_TEST_ASSERT_NOT_NULL (updated, "the incremental result should be a forest");
      if (updated != NULL)
        GLR_TEST_ASSERT_EQ (updated->root->end_position, 7, "the result spans the whole input");
      glr_forest_destroy (updated);
      updated = NULL;
      evaluation.count = evaluation.selections = 0;
      GLR_TEST_ASSERT_EQ (glr_parser_parse_incremental (parser, old, "1+1+1", 5,
                          "1+1+?", 5, 4, 5, &updated), -1,
                           "an invalid edit should fail before semantic callbacks");
      GLR_TEST_ASSERT_EQ (evaluation.count, 0, "rejected incremental input invokes no actions");
      GLR_TEST_ASSERT_NULL (updated, "a failed update exposes no forest");
      if (old != NULL)
        GLR_TEST_ASSERT_EQ (old->root->end_position, 5, "the previous forest remains valid");
      glr_forest_destroy (old);
      glr_parser_destroy (parser);
      glr_grammar_destroy (grammar);
    }
  glr_test_end ();
}

static int
failed_action (const glr_semantic_context_t *context, void **value, void *data)
{
  (void) context;
  (void) value;
  (void) data;
  return -1;
}

GLR_TEST_CASE (cycles_action_errors_and_lifecycle)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int a = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "A");
  glr_symbol_t *body[] = { grammar->symbols[a] };
  int p = glr_grammar_add_production (grammar, a, body, 1);
  int old_destroyed = 0, new_destroyed = 0;
  glr_forest_t *forest = glr_forest_create ();
  glr_forest_node_t *symbol = glr_forest_get_symbol (forest, a, 0, 0);
  glr_forest_node_t *constructor = glr_forest_get_constructor (forest, p, 0, 0);
  evaluator_t evaluation = { { 0 }, 0, 0 };
  void *value = NULL;
  glr_test_begin ("cycles are rejected before actions, and action data has single ownership");
  glr_forest_add_child (symbol, constructor);
  glr_forest_add_child (constructor, symbol);
  forest->root = symbol;
  glr_production_set_semantic_action (grammar, p, epsilon_action, &old_destroyed, count_destruction);
  GLR_TEST_ASSERT_EQ (glr_semantic_evaluate (grammar, forest, "", 0, NULL, &evaluation, &value),
                       GLR_SEMANTIC_ERROR_CYCLE, "cyclic derivation should terminate with an error");
  GLR_TEST_ASSERT_EQ (evaluation.count, 0, "cycle validation precedes every action");
  glr_production_set_semantic_action (grammar, p, epsilon_action, &new_destroyed, count_destruction);
  GLR_TEST_ASSERT_EQ (old_destroyed, 1, "replacement releases the old action data once");
  glr_production_set_semantic_action (grammar, p, NULL, NULL, NULL);
  GLR_TEST_ASSERT_EQ (new_destroyed, 1, "clearing releases the replacement data once");
  glr_forest_t *clone = glr_forest_clone (forest);
  GLR_TEST_ASSERT_NOT_NULL (clone, "cyclic forest can be cloned without recursion");
  GLR_TEST_ASSERT (clone->root != symbol, "clone has an independent root");
  glr_forest_destroy (clone);
  glr_forest_destroy (forest);
  glr_grammar_destroy (grammar);

  grammar = glr_grammar_create ();
  a = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "A");
  p = glr_grammar_add_production (grammar, a, NULL, 0);
  glr_grammar_set_start_symbol (grammar, a);
  glr_production_set_semantic_action (grammar, p, failed_action, NULL, NULL);
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_result_t result = glr_parse (parser, "", 0);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SEMANTIC, "callback failure should fail the parse result");
  GLR_TEST_ASSERT_EQ (result.semantic_error, GLR_SEMANTIC_ERROR_ACTION, "callback error is distinguishable");
  GLR_TEST_ASSERT_NULL (result.semantic_value, "failed action must not expose a result value");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

static int
length_action (const glr_semantic_context_t *context, void **value, void *data)
{
  evaluator_t *evaluation = context->user_data;
  glr_selection_t first;
  (void) data;
  if (glr_select (&context->selection, "$1", &first) != 0)
    return -1;
  evaluation->values[evaluation->count] = (long) first.length;
  *value = &evaluation->values[evaluation->count++];
  return 0;
}

static size_t
choose_long_first (const glr_forest_node_t *symbol, void *data)
{
  (void) data;
  for (size_t i = 0; i < symbol->child_count; i++)
    if (symbol->children[i]->children[0]->end_position == 2)
      return i;
  return SIZE_MAX;
}

GLR_TEST_CASE (ambiguity_requires_resolution)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int s = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "S");
  int word = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "WORD");
  glr_symbol_t *body[] = { grammar->symbols[word], grammar->symbols[word] };
  evaluator_t evaluation = { { 0 }, 0, 0 };
  glr_parser_t *parser;
  glr_parse_result_t result;
  glr_test_begin ("ambiguity is resolved before any semantic callbacks");
  int production = glr_grammar_add_production (grammar, s, body, 2);
  glr_production_set_semantic_action (grammar, production, length_action, NULL, NULL);
  glr_grammar_set_start_symbol (grammar, s);
  glr_scannerless_set_pattern (grammar, word, "[a-z]+", NULL, 0);
  parser = glr_parser_create (grammar);
  glr_parser_set_user_data (parser, &evaluation);
  result = glr_parse (parser, "abc", 3);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SEMANTIC, "ambiguous actions require a resolver");
  GLR_TEST_ASSERT_EQ (result.semantic_error, GLR_SEMANTIC_ERROR_AMBIGUOUS, "ambiguity status should be explicit");
  GLR_TEST_ASSERT_EQ (evaluation.count, 0, "no speculative semantic actions should run");
  glr_parser_set_semantic_resolver (parser, choose_long_first);
  result = glr_parse (parser, "abc", 3);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "resolved derivation should evaluate");
  GLR_TEST_ASSERT_EQ (*(long *) result.semantic_value, 2, "resolver should choose ab/c");
  GLR_TEST_ASSERT_EQ (evaluation.count, 1, "only the chosen constructor should evaluate");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

static int
epsilon_action (const glr_semantic_context_t *context, void **value, void *data)
{
  evaluator_t *evaluation = context->user_data;
  (void) data;
  evaluation->values[evaluation->count] = (long) context->selection.child_count;
  *value = &evaluation->values[evaluation->count++];
  return 0;
}

GLR_TEST_CASE (epsilon_and_shared_positions)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int s = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "S");
  int a = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "A");
  glr_symbol_t *body[] = { grammar->symbols[a], grammar->symbols[a] };
  evaluator_t evaluation = { { 0 }, 0, 0 };
  glr_parser_t *parser;
  glr_parse_result_t result;
  glr_selection_t first, second;
  glr_test_begin ("epsilon constructors preserve repeated positions and evaluate once");
  glr_grammar_add_production (grammar, s, body, 2);
  int epsilon = glr_grammar_add_production (grammar, a, NULL, 0);
  glr_production_set_semantic_action (grammar, epsilon, epsilon_action, NULL, NULL);
  glr_grammar_set_start_symbol (grammar, s);
  parser = glr_parser_create (grammar);
  glr_parser_set_user_data (parser, &evaluation);
  result = glr_parse (parser, "", 0);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "empty input should derive S");
  GLR_TEST_ASSERT_EQ (evaluation.count, 1, "shared epsilon occurrence should evaluate once");
  glr_forest_node_t *constructor = result.forest->root->children[0];
  glr_select_context_t context = {
    grammar->productions[0], constructor->children, constructor->child_count,
    NULL, "", 0
  };
  GLR_TEST_ASSERT_EQ (constructor->child_count, 2, "both RHS positions survive even when pointers match");
  GLR_TEST_ASSERT_EQ (glr_select (&context, "$1", &first), 0, "first epsilon can be selected");
  GLR_TEST_ASSERT_EQ (glr_select (&context, "$2", &second), 0, "second epsilon can be selected");
  GLR_TEST_ASSERT_EQ (first.node, second.node, "shared occurrence is packed once");
  GLR_TEST_ASSERT_EQ (first.length, 0, "empty selection has an empty slice");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

int
main (void)
{
  GLR_TEST_INIT;
  deferred_evaluation_and_selectors (&stats);
  incremental_actions_evaluate_complete_inputs (&stats);
  ambiguity_requires_resolution (&stats);
  epsilon_and_shared_positions (&stats);
  cycles_action_errors_and_lifecycle (&stats);
  return glr_test_finish ("Semantic actions", stats);
}
