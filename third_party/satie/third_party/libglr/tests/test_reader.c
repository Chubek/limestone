#include "test_common.h"

#include <glr/lexer-hooks.h>
#include <glr/reader.h>

#include <string.h>

static bool
bang_hook (const glr_lexer_event_t *event, glr_lexer_response_t *response,
           void *hook_user_data)
{
  const char *name = hook_user_data;

  if (event->codepoint != 0x0021 || !glr_test_string_eq (event->unicode_name,
                                                          "EXCLAMATION MARK"))
    {
      return false;
    }

  glr_lexer_response_accept (response, name, 4);
  return true;
}

static bool
decline_hook (const glr_lexer_event_t *event, glr_lexer_response_t *response,
              void *hook_user_data)
{
  (void)event;
  (void)response;
  (void)hook_user_data;
  return false;
}

GLR_TEST_CASE (test_reader_fallback)
{
  static const unsigned char input[] = { 0x21, 0x00, 0x22, 0x00 };
  glr_reader_t *reader = glr_reader_create ();
  glr_reader_token_t token = { 0 };

  glr_test_begin ("reader fallback tokenization");
  GLR_TEST_ASSERT_NOT_NULL (reader, "reader should be created");
  GLR_TEST_ASSERT_EQ (glr_reader_set_input (reader, input, sizeof (input)), 0,
                      "input should be set");
  GLR_TEST_ASSERT_EQ (glr_reader_next (reader, &token), GLR_READER_STATUS_OK,
                      "reader should yield first token");
  GLR_TEST_ASSERT (!token.from_hook, "fallback token should not come from hook");
  GLR_TEST_ASSERT (glr_test_string_eq (token.terminal_name, "EXCLAMATION MARK"),
                   "fallback should use Unicode name");
  GLR_TEST_ASSERT_EQ (token.byte_offset, 0, "first token should start at 0");
  GLR_TEST_ASSERT_EQ (token.bytes_consumed, 2,
                      "fallback token should consume one UTF-16 unit");
  glr_reader_token_clear (&token);
  glr_reader_destroy (reader);
  glr_test_end ();
}

GLR_TEST_CASE (test_reader_hook_override)
{
  static const unsigned char input[] = { 0x21, 0x00, 0x3F, 0x00, 0x41, 0x00 };
  glr_reader_t *reader = glr_reader_create ();
  glr_lexer_hooks_t *hooks = glr_lexer_hooks_create ();
  glr_reader_token_t token = { 0 };

  glr_test_begin ("reader hook override");
  GLR_TEST_ASSERT_NOT_NULL (reader, "reader should be created");
  GLR_TEST_ASSERT_NOT_NULL (hooks, "hooks should be created");
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_add (hooks, "bang", 10, bang_hook,
                                           (void *)"BANG_TOKEN", NULL),
                      0, "hook should register");
  GLR_TEST_ASSERT_EQ (glr_reader_set_lexer_hooks (reader, hooks), 0,
                      "reader should accept hooks");
  GLR_TEST_ASSERT_EQ (glr_reader_set_input (reader, input, sizeof (input)), 0,
                      "input should be set");
  GLR_TEST_ASSERT_EQ (glr_reader_next (reader, &token), GLR_READER_STATUS_OK,
                      "reader should yield custom token");
  GLR_TEST_ASSERT (token.from_hook, "custom token should come from hook");
  GLR_TEST_ASSERT (glr_test_string_eq (token.terminal_name, "BANG_TOKEN"),
                   "hook should override terminal name");
  GLR_TEST_ASSERT_EQ (token.bytes_consumed, 4,
                      "hook should be able to consume multiple bytes");
  glr_reader_token_clear (&token);

  GLR_TEST_ASSERT_EQ (glr_reader_next (reader, &token), GLR_READER_STATUS_OK,
                      "reader should continue after hook token");
  GLR_TEST_ASSERT (glr_test_string_eq (token.terminal_name,
                                       "LATIN CAPITAL LETTER A"),
                   "reader should resume at next unconsumed codepoint");

  glr_reader_token_clear (&token);
  glr_lexer_hooks_destroy (hooks);
  glr_reader_destroy (reader);
  glr_test_end ();
}

GLR_TEST_CASE (test_reader_decline_and_invalid_sequence)
{
  static const unsigned char fallback_input[] = { 0x21, 0x00 };
  static const unsigned char invalid_input[] = { 0x00, 0xD8 };
  glr_reader_t *reader = glr_reader_create ();
  glr_lexer_hooks_t *hooks = glr_lexer_hooks_create ();
  glr_reader_token_t token = { 0 };

  glr_test_begin ("reader decline and invalid UTF-16");
  GLR_TEST_ASSERT_NOT_NULL (reader, "reader should be created");
  GLR_TEST_ASSERT_NOT_NULL (hooks, "hooks should be created");
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_add (hooks, "decline", 5, decline_hook,
                                           NULL, NULL),
                      0, "declining hook should register");
  GLR_TEST_ASSERT_EQ (glr_reader_set_lexer_hooks (reader, hooks), 0,
                      "reader should accept hooks");
  GLR_TEST_ASSERT_EQ (
      glr_reader_set_input (reader, fallback_input, sizeof (fallback_input)), 0,
      "fallback input should be set");
  GLR_TEST_ASSERT_EQ (glr_reader_next (reader, &token), GLR_READER_STATUS_OK,
                      "reader should fall back when hook declines");
  GLR_TEST_ASSERT (!token.from_hook,
                   "declining hook should leave fallback path active");
  GLR_TEST_ASSERT (glr_test_string_eq (token.terminal_name, "EXCLAMATION MARK"),
                   "fallback token should still be produced");
  glr_reader_token_clear (&token);

  GLR_TEST_ASSERT_EQ (glr_reader_set_input (reader, invalid_input,
                                            sizeof (invalid_input)),
                      0, "invalid input should be accepted for later decode");
  GLR_TEST_ASSERT_EQ (glr_reader_next (reader, &token),
                      GLR_READER_STATUS_INVALID_SEQUENCE,
                      "unpaired surrogate should be rejected");

  glr_lexer_hooks_destroy (hooks);
  glr_reader_destroy (reader);
  glr_test_end ();
}

int
main (void)
{
  GLR_TEST_INIT;

  printf ("=== LibGLR Reader Tests ===\n\n");
  test_reader_fallback (&stats);
  test_reader_hook_override (&stats);
  test_reader_decline_and_invalid_sequence (&stats);
  return glr_test_finish ("LibGLR Reader", stats);
}
