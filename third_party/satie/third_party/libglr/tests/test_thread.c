#include "test_common.h"
#include <glr/glr.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#define JOB_COUNT 512

typedef struct
{
  glr_stringpool_t *pool;
  const char *strings[JOB_COUNT];
  atomic_uint visits[JOB_COUNT];
  atomic_uint failures;
} parallel_state_t;

static void
intern_parallel (size_t index, size_t worker, void *data)
{
  parallel_state_t *state = data;
  const char text[] = { 's', '\0', 'h', 'a', 'r', 'e', 'd' };
  state->strings[index] = glr_stringpool_intern_n (state->pool, text, sizeof text);
  if (worker >= 4 || state->strings[index] == NULL)
    atomic_fetch_add (&state->failures, 1);
  atomic_fetch_add (&state->visits[index], 1);
}

GLR_TEST_CASE (concurrent_interning_and_growth)
{
  parallel_state_t state = { 0 };
  const char *original;
  char text[32];
  glr_test_begin ("klib parallel loop and thread-safe length-aware string pool");
  state.pool = glr_stringpool_create ();
  original = glr_stringpool_intern (state.pool, "stable");
  GLR_TEST_ASSERT_EQ (glr_parallel_for (4, JOB_COUNT, intern_parallel, &state), 0,
                       "parallel loop should complete");
  GLR_TEST_ASSERT_EQ (atomic_load (&state.failures), 0, "workers should receive valid ids and strings");
  for (size_t i = 0; i < JOB_COUNT; i++)
    {
      GLR_TEST_ASSERT_EQ (atomic_load (&state.visits[i]), 1, "each index runs exactly once");
      GLR_TEST_ASSERT_EQ (state.strings[i], state.strings[0], "binary duplicates have identical pointers");
    }
  GLR_TEST_ASSERT_EQ (glr_stringpool_count (state.pool), 2, "duplicate strings occupy one pool entry");
  GLR_TEST_ASSERT_EQ (glr_stringpool_bytes (state.pool), 15, "pool accounts for payloads and terminators");
  for (size_t i = 0; i < 4096; i++)
    {
      snprintf (text, sizeof text, "name_%zu", i);
      GLR_TEST_ASSERT_NOT_NULL (glr_stringpool_intern (state.pool, text), "pool should grow");
    }
  GLR_TEST_ASSERT (strcmp (original, "stable") == 0, "arena growth leaves old pointers stable");
  GLR_TEST_ASSERT_EQ (glr_stringpool_intern (state.pool, "stable"), original, "interning preserves identity");
  GLR_TEST_ASSERT_NOT_NULL (glr_stringpool_intern_n (state.pool, NULL, 0), "empty binary string is supported");
  GLR_TEST_ASSERT_NULL (glr_stringpool_intern_n (state.pool, NULL, 1), "nonempty NULL input is invalid");
  GLR_TEST_ASSERT_EQ (glr_parallel_for (0, 1, intern_parallel, &state), -1, "zero workers is invalid");
  glr_stringpool_destroy (state.pool);
  glr_test_end ();
}

typedef struct
{
  glr_grammar_t *grammar;
  glr_stringpool_t *pool;
  atomic_uint completed;
  atomic_uint failures;
} queued_state_t;

static void
parse_job (void *data)
{
  queued_state_t *state = data;
  glr_parser_t *parser = glr_parser_create (state->grammar);
  glr_parse_result_t result = glr_parse (parser, "314159", 6);
  if (result.error != GLR_PARSE_SUCCESS || result.position != 6
      || glr_stringpool_intern (state->pool, "job") == NULL)
    atomic_fetch_add (&state->failures, 1);
  glr_parser_destroy (parser);
  atomic_fetch_add (&state->completed, 1);
}

GLR_TEST_CASE (persistent_pool_wait_reuse_and_drain)
{
  queued_state_t state = { 0 };
  glr_thread_pool_t *pool = glr_thread_pool_create (4);
  glr_symbol_t *body[1];
  glr_test_begin ("CTL task queue parses concurrently, waits, reuses, and drains");
  state.grammar = glr_grammar_create ();
  state.pool = glr_stringpool_create ();
  int s = glr_grammar_add_symbol (state.grammar, GLR_SYMBOL_NONTERMINAL, "S");
  int digits = glr_grammar_add_symbol (state.grammar, GLR_SYMBOL_TERMINAL, "DIGITS");
  body[0] = state.grammar->symbols[digits];
  glr_grammar_add_production (state.grammar, s, body, 1);
  glr_grammar_set_start_symbol (state.grammar, s);
  glr_scannerless_set_pattern (state.grammar, digits, "[0-9]+", NULL, 0);
  glr_parse_table_t *table = glr_grammar_build_parse_table (state.grammar, NULL, 0);
  glr_grammar_set_parse_table (state.grammar, table, true);
  GLR_TEST_ASSERT_NOT_NULL (pool, "pool should start workers");
  GLR_TEST_ASSERT_EQ (glr_thread_pool_size (pool), 4, "pool should report worker count");
  for (size_t i = 0; i < JOB_COUNT; i++)
    GLR_TEST_ASSERT_EQ (glr_thread_pool_submit (pool, parse_job, &state), 0, "jobs should enqueue");
  GLR_TEST_ASSERT_EQ (glr_thread_pool_wait (pool), 0, "wait is a completion barrier");
  GLR_TEST_ASSERT_EQ (atomic_load (&state.completed), JOB_COUNT, "every job completes before wait returns");
  for (size_t i = 0; i < JOB_COUNT; i++)
    GLR_TEST_ASSERT_EQ (glr_thread_pool_submit (pool, parse_job, &state), 0, "workers should be reusable");
  glr_thread_pool_destroy (pool);
  GLR_TEST_ASSERT_EQ (atomic_load (&state.completed), JOB_COUNT * 2, "destroy drains pending work");
  GLR_TEST_ASSERT_EQ (atomic_load (&state.failures), 0, "shared immutable grammar is safe across parsers");
  GLR_TEST_ASSERT_EQ (glr_stringpool_count (state.pool), 1, "concurrent jobs share interned strings");
  GLR_TEST_ASSERT_NULL (glr_thread_pool_create (0), "zero-thread pool should be rejected");
  glr_stringpool_destroy (state.pool);
  glr_grammar_destroy (state.grammar);
  glr_test_end ();
}

int
main (void)
{
  GLR_TEST_INIT;
  concurrent_interning_and_growth (&stats);
  persistent_pool_wait_reuse_and_drain (&stats);
  return glr_test_finish ("Threading and string pool", stats);
}
