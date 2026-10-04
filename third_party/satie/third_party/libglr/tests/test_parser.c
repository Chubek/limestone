#include "test_common.h"

#include <glr/glr.h>

#include <string.h>

static glr_grammar_t *
make_ascii_grammar (void)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int start_id;
  int term_id;
  glr_symbol_t *body[1];

  if (grammar == NULL)
    {
      return NULL;
    }

  start_id = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Start");
  term_id = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "a");
  body[0] = glr_grammar_get_symbol (grammar, term_id);
  glr_grammar_add_production (grammar, start_id, body, 1);
  glr_grammar_set_start_symbol (grammar, start_id);
  return grammar;
}

/* Start -> BANG LATIN_A, so the UTF-16 input "!" "A" is a full sentence. */
static glr_grammar_t *
make_utf16_grammar (void)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int start_id;
  int bang_id;
  int letter_id;
  glr_symbol_t *body[2];

  if (grammar == NULL)
    {
      return NULL;
    }

  start_id = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Start");
  bang_id = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "BANG_TOKEN");
  letter_id = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL,
                                      "LATIN CAPITAL LETTER A");
  body[0] = glr_grammar_get_symbol (grammar, bang_id);
  body[1] = glr_grammar_get_symbol (grammar, letter_id);
  glr_grammar_add_production (grammar, start_id, body, 2);
  glr_grammar_set_start_symbol (grammar, start_id);
  return grammar;
}

static bool
bang_hook (const glr_lexer_event_t *event, glr_lexer_response_t *response,
           void *hook_user_data)
{
  (void)hook_user_data;
  if (event->codepoint != 0x0021)
    {
      return false;
    }

  glr_lexer_response_accept (response, "BANG_TOKEN", 2);
  return true;
}

GLR_TEST_CASE (test_parser_create_destroy)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  glr_parser_t *parser;

  glr_test_begin ("parser create and destroy");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  parser = glr_parser_create (grammar);
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_parser_parse_ascii)
{
  glr_grammar_t *grammar = make_ascii_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_result_t result;

  glr_test_begin ("parser tokenizes byte input against grammar terminals");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  result = glr_parse (parser, "a", 1);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS,
                      "\"a\" is the grammar's only sentence");
  GLR_TEST_ASSERT_EQ (result.position, 1,
                      "byte input should advance past the accepted token");
  GLR_TEST_ASSERT_NULL (glr_parser_get_last_token (parser),
                        "byte-mode parsing should not expose reader tokens");
  result = glr_parse (parser, "ab", 2);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SYNTAX,
                      "'b' is not a terminal of the grammar");
  GLR_TEST_ASSERT_NULL (result.forest,
                        "rejected input should not expose a forest");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_parser_parse_utf16_with_hook)
{
  static const unsigned char input[] = { 0x21, 0x00, 0x41, 0x00 };
  glr_grammar_t *grammar = make_utf16_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_lexer_hooks_t *hooks = glr_lexer_hooks_create ();
  const glr_reader_token_t *token;
  glr_parse_result_t result;

  glr_test_begin ("parser consumes UTF-16 tokens via lexer hooks");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  GLR_TEST_ASSERT_NOT_NULL (hooks, "hooks should be created");
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_add (hooks, "bang", 10, bang_hook, NULL,
                                           NULL),
                      0, "hook should register");
  GLR_TEST_ASSERT_EQ (glr_parser_set_lexer_hooks (parser, hooks), 0,
                      "parser should accept lexer hooks");
  result = glr_parse (parser, (const char *)input, sizeof (input));
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS,
                      "UTF-16 input should parse through the reader");
  GLR_TEST_ASSERT_EQ (result.position, sizeof (input),
                      "UTF-16 parse should consume all bytes");
  token = glr_parser_get_last_token (parser);
  GLR_TEST_ASSERT_NOT_NULL (token, "parser should expose the last reader token");
  GLR_TEST_ASSERT (glr_test_string_eq (token->terminal_name,
                                       "LATIN CAPITAL LETTER A"),
                   "last token should reflect reader fallback tokenization");
  GLR_TEST_ASSERT_EQ (token->byte_offset, 2,
                      "last token should start at the second UTF-16 unit");
  glr_lexer_hooks_destroy (hooks);
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_parser_last_token_survives_reset)
{
  static const unsigned char input[] = { 0xFF, 0xFE, 0x21, 0x00, 0x41, 0x00 };
  glr_grammar_t *grammar = make_utf16_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_lexer_hooks_t *hooks = glr_lexer_hooks_create ();
  const glr_reader_token_t *token;

  glr_test_begin ("parser keeps the last reader token after a parse");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  GLR_TEST_ASSERT_NOT_NULL (hooks, "hooks should be created");
  glr_lexer_hooks_add (hooks, "bang", 10, bang_hook, NULL, NULL);
  glr_parser_set_lexer_hooks (parser, hooks);
  GLR_TEST_ASSERT_EQ (
      glr_parse (parser, (const char *) input, sizeof (input)).error,
      GLR_PARSE_SUCCESS, "BOM-prefixed UTF-16 input should parse");
  token = glr_parser_get_last_token (parser);
  GLR_TEST_ASSERT_NOT_NULL (token, "last token should be available");
  if (token != NULL)
    {
      GLR_TEST_ASSERT (
          glr_test_string_eq (token->terminal_name, "LATIN CAPITAL LETTER A"),
          "last token should be the final reader token");
      GLR_TEST_ASSERT_EQ (token->byte_offset, 4,
                          "last token should skip the two-byte BOM");
    }
  GLR_TEST_ASSERT_EQ (glr_parser_reset (parser), 0, "reset should succeed");
  GLR_TEST_ASSERT_NULL (glr_parser_get_last_token (parser),
                        "reset should clear the recorded token");
  glr_lexer_hooks_destroy (hooks);
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_parser_rejects_unknown_utf16_terminal)
{
  static const unsigned char input[] = { 0x21, 0x00 };
  glr_grammar_t *grammar = make_ascii_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_result_t result;

  glr_test_begin ("parser rejects unknown UTF-16 terminals");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  result = glr_parse (parser, (const char *)input, sizeof (input));
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SYNTAX,
                      "unknown UTF-16 terminal should fail with syntax error");
  GLR_TEST_ASSERT_EQ (result.position, 2,
                      "error position should reflect the failing token span");
  GLR_TEST_ASSERT_NULL (result.forest,
                        "rejected input should not expose a forest");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_parser_misc_accessors)
{
  glr_grammar_t *grammar = make_ascii_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  void *data = (void *)0x1234;

  glr_test_begin ("parser accessors");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  glr_parser_set_user_data (parser, data);
  GLR_TEST_ASSERT_EQ (glr_parser_get_user_data (parser), data,
                      "parser should return stored user data");
  GLR_TEST_ASSERT_EQ (glr_parser_get_error (parser), GLR_PARSE_SUCCESS,
                      "new parser should report success state");
  GLR_TEST_ASSERT_NOT_NULL (glr_parser_get_forest (parser),
                            "parser should expose a forest");
  GLR_TEST_ASSERT_EQ (glr_parser_reset (parser), 0,
                      "parser reset should succeed");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_parser_null_contract)
{
  glr_parse_result_t result;

  glr_test_begin ("parser null contract");
  GLR_TEST_ASSERT_NULL (glr_parser_create (NULL),
                        "parser creation should fail without a grammar");
  result = glr_parse (NULL, "input", 5);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_MEMORY,
                      "null parser should yield memory-style failure");
  GLR_TEST_ASSERT_NULL (result.forest, "null parser should not return a forest");
  glr_test_end ();
}

GLR_TEST_CASE (test_parser_version)
{
  glr_test_begin ("parser version helpers");
  GLR_TEST_ASSERT_NOT_NULL (glr_version (), "version should not be NULL");
  GLR_TEST_ASSERT_NOT_NULL (glr_name (), "library name should not be NULL");
  GLR_TEST_ASSERT (strlen (glr_name ()) > 0, "library name should not be empty");
  GLR_TEST_ASSERT (glr_test_string_eq (glr_name (), "LibGLR"),
                   "library name should remain LibGLR");
  glr_test_end ();
}

int
main (void)
{
  GLR_TEST_INIT;

  printf ("=== LibGLR Parser Tests ===\n\n");
  test_parser_create_destroy (&stats);
  test_parser_parse_ascii (&stats);
  test_parser_parse_utf16_with_hook (&stats);
  test_parser_last_token_survives_reset (&stats);
  test_parser_rejects_unknown_utf16_terminal (&stats);
  test_parser_misc_accessors (&stats);
  test_parser_null_contract (&stats);
  test_parser_version (&stats);
  return glr_test_finish ("LibGLR Parser", stats);
}
