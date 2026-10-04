#include "test_common.h"
#include <glr/glr.h>
#include <stdlib.h>
#include <string.h>

static glr_grammar_t *
single_terminal (const char *expression)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int s = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "S");
  int word = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "WORD");
  glr_symbol_t *body[] = { glr_grammar_get_symbol (grammar, word) };
  glr_grammar_add_production (grammar, s, body, 1);
  glr_grammar_set_start_symbol (grammar, s);
  if (expression != NULL)
    glr_scannerless_set_pattern (grammar, word, expression, NULL, 0);
  return grammar;
}

GLR_TEST_CASE (patterns_and_bounded_input)
{
  glr_grammar_t *grammar = single_terminal ("[A-Za-z_][A-Za-z0-9_]*");
  glr_parser_t *parser = glr_parser_create (grammar);
  const char input[] = { 'f', 'o', 'o', '9', '!' }; /* Not NUL-terminated. */
  glr_parse_result_t result;
  glr_terminal_match_t *matches;
  size_t count;
  size_t longest;
  char error[128];
  glr_test_begin ("patterns match bounded input and validate atomically");
  result = glr_parse (parser, input, 4);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "identifier should parse");
  GLR_TEST_ASSERT_EQ (result.forest->root->end_position, 4, "span uses matched bytes");
  GLR_TEST_ASSERT_EQ (glr_scannerless_scan (grammar, input, 4, 0, &matches, &count),
                       0, "scan should succeed");
  GLR_TEST_ASSERT_EQ (count, 4, "all four identifier prefixes should be retained");
  for (size_t i = 0; i < count; i++)
    GLR_TEST_ASSERT_EQ (matches[i].length, i + 1, "prefix order should be stable");
  free (matches);
  GLR_TEST_ASSERT_EQ (glr_scannerless_match (grammar->symbols[1], input, 5, &longest),
                       1, "longest-match helper should succeed");
  GLR_TEST_ASSERT_EQ (longest, 4, "helper stops before punctuation");
  GLR_TEST_ASSERT_EQ (glr_scannerless_set_pattern (grammar, 1, "[", error, sizeof error),
                       -1, "invalid regex should be rejected");
  GLR_TEST_ASSERT (error[0] != '\0', "invalid regex should have a diagnostic");
  GLR_TEST_ASSERT_EQ (glr_scannerless_set_pattern (grammar, 1, "a*", error, sizeof error),
                       -1, "nullable terminal should be rejected");
  GLR_TEST_ASSERT_EQ (glr_parse (parser, input, 4).error, GLR_PARSE_SUCCESS,
                       "failed replacements must preserve the identifier pattern");
  GLR_TEST_ASSERT_EQ (glr_parse (parser, input, 5).error, GLR_PARSE_ERROR_SYNTAX,
                       "unmatched trailing input should fail");
  GLR_TEST_ASSERT_EQ (glr_parse (parser, "", 0).error, GLR_PARSE_ERROR_SYNTAX,
                       "terminals cannot consume zero bytes");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (context_and_variable_width)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int s = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "S");
  int a = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "A");
  int b = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "b");
  glr_symbol_t *body[] = { grammar->symbols[a], grammar->symbols[b] };
  glr_parser_t *parser;
  glr_parse_result_t result;
  glr_test_begin ("grammar determines viable token widths");
  glr_grammar_add_production (grammar, s, body, 2);
  glr_grammar_set_start_symbol (grammar, s);
  glr_scannerless_set_pattern (grammar, a, "ab|a", NULL, 0);
  parser = glr_parser_create (grammar);
  result = glr_parse (parser, "ab", 2);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS,
                       "shorter a match should survive the dead ab branch");
  GLR_TEST_ASSERT (!glr_forest_is_ambiguous (result.forest->root),
                    "only the grammatically viable split should survive");
  result = glr_parse (parser, "abb", 3);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "longer ab match should also work");
  glr_forest_t *old = glr_forest_clone (result.forest);
  glr_forest_t *updated = NULL;
  GLR_TEST_ASSERT_EQ (glr_parser_parse_incremental (parser, old, "abb", 3,
                      "ab", 2, 2, 3, &updated), 0,
                       "incremental pattern parsing must reconsider lexical boundaries");
  GLR_TEST_ASSERT_EQ (updated->root->end_position, 2,
                       "incremental root spans the complete updated input");
  glr_forest_destroy (updated);
  glr_forest_destroy (old);
  int c = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "c");
  body[0] = grammar->symbols[c];
  glr_grammar_add_production (grammar, s, body, 1);
  GLR_TEST_ASSERT_EQ (glr_parse (parser, "c", 1).error, GLR_PARSE_SUCCESS,
                       "grammar growth should refresh a generated table");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (packed_splits_and_lexical_overlap)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int s = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "S");
  int word = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "WORD");
  glr_symbol_t *body[] = { grammar->symbols[word], grammar->symbols[word] };
  glr_parser_t *parser;
  glr_parse_result_t result;
  glr_test_begin ("same-production splits remain distinct packed alternatives");
  glr_grammar_add_production (grammar, s, body, 2);
  glr_grammar_set_start_symbol (grammar, s);
  glr_scannerless_set_pattern (grammar, word, "[a-z]+", NULL, 0);
  parser = glr_parser_create (grammar);
  result = glr_parse (parser, "abc", 3);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "two words should parse");
  GLR_TEST_ASSERT_EQ (result.forest->root->child_count, 2,
                       "a/bc and ab/c are distinct derivations of the same production");
  GLR_TEST_ASSERT (glr_forest_is_ambiguous (result.forest->root), "split is ambiguous");
  for (size_t i = 0; i < 2; i++)
    {
      glr_forest_node_t *node = result.forest->root->children[i];
      GLR_TEST_ASSERT_EQ (node->child_count, 2, "each constructor retains ordered positions");
      GLR_TEST_ASSERT_EQ (node->children[0]->end_position, node->children[1]->position,
                           "terminal spans meet at each split");
    }
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);

  grammar = single_terminal ("[a-z]+");
  int keyword = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "if");
  body[0] = grammar->symbols[keyword];
  glr_grammar_add_production (grammar, 0, body, 1);
  parser = glr_parser_create (grammar);
  result = glr_parse (parser, "if", 2);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "overlapping terminals should parse");
  GLR_TEST_ASSERT_EQ (result.forest->root->child_count, 2,
                       "keyword and identifier alternatives both survive");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (literal_bytes_trivia_and_rewrite)
{
  const char bytes[] = { 'x', '\0', 'y' };
  glr_grammar_t *grammar = single_terminal (NULL);
  glr_parser_t *parser;
  glr_parse_result_t result;
  glr_test_begin ("binary literals, trivia, and pattern ownership across renames");
  GLR_TEST_ASSERT_EQ (glr_scannerless_set_literal (grammar, 1, bytes, sizeof bytes),
                       0, "binary literal should configure");
  parser = glr_parser_create (grammar);
  result = glr_parse (parser, bytes, sizeof bytes);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "embedded NUL should match literally");
  GLR_TEST_ASSERT_EQ (result.position, sizeof bytes, "literal consumes its exact byte width");
  glr_scannerless_set_pattern (grammar, 1, "[0-9]{2,4}", NULL, 0);
  glr_parser_set_trivia (parser, " ");
  GLR_TEST_ASSERT_EQ (glr_parse (parser, " 1234 ", 6).error, GLR_PARSE_SUCCESS,
                       "patterns should cooperate with trivia");
  GLR_TEST_ASSERT_EQ (glr_rewrite_rename_symbol (grammar, "WORD", "DIGITS"),
                       GLR_REWRITE_STATUS_OK, "terminal should rename");
  GLR_TEST_ASSERT_EQ (glr_parse (parser, "123", 3).error, GLR_PARSE_SUCCESS,
                       "renaming a pattern terminal retains its pattern");
  char error[128];
  glr_live_parser_t *live = glr_live_parser_create (grammar, "12", 2, error, sizeof error);
  glr_live_edit_t edit = { 1, 3, 1, 3, "3", 1 };
  GLR_TEST_ASSERT_NOT_NULL (live, "pattern grammar should support live parsing");
  GLR_TEST_ASSERT_EQ (glr_live_parser_edit (live, &edit, error, sizeof error), 0,
                       "append inside the lexical extent should edit");
  GLR_TEST_ASSERT_EQ (glr_live_parser_update (live, error, sizeof error), 0,
                       "updating must allow the preceding pattern to grow");
  GLR_TEST_ASSERT_EQ (glr_live_parser_forest (live)->root->end_position, 3,
                       "updated pattern spans all three digits");
  glr_live_parser_destroy (live);
  glr_scannerless_clear_pattern (grammar, 1);
  GLR_TEST_ASSERT_EQ (glr_parse (parser, "DIGITS", 6).error, GLR_PARSE_SUCCESS,
                       "clearing restores literal-name matching");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

int
main (void)
{
  GLR_TEST_INIT;
  patterns_and_bounded_input (&stats);
  context_and_variable_width (&stats);
  packed_splits_and_lexical_overlap (&stats);
  literal_bytes_trivia_and_rewrite (&stats);
  return glr_test_finish ("Scannerless", stats);
}
