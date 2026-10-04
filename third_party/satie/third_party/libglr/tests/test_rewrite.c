#include <glr/rewrite.h>

#include <stdio.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg)                                                     \
  do                                                                          \
    {                                                                         \
      if (cond)                                                               \
        {                                                                     \
          tests_passed++;                                                     \
        }                                                                     \
      else                                                                    \
        {                                                                     \
          printf ("FAILED: %s\n", msg);                                      \
          tests_failed++;                                                     \
        }                                                                     \
    }                                                                         \
  while (0)

static glr_grammar_t *
build_sample_grammar (void)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int s;
  int a;
  int b;
  int c;
  int ta;
  int tb;
  glr_symbol_t *body[3];

  if (grammar == NULL)
    {
      return NULL;
    }

  s = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "S");
  a = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "A");
  b = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "B");
  c = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "C");
  ta = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "a");
  tb = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "b");

  glr_grammar_set_start_symbol (grammar, s);

  body[0] = glr_grammar_get_symbol (grammar, a);
  glr_grammar_add_production (grammar, s, body, 1);

  body[0] = glr_grammar_get_symbol (grammar, a);
  body[1] = glr_grammar_get_symbol (grammar, ta);
  glr_grammar_add_production (grammar, a, body, 2);
  body[0] = glr_grammar_get_symbol (grammar, b);
  glr_grammar_add_production (grammar, a, body, 1);
  glr_grammar_add_production (grammar, a, NULL, 0);

  body[0] = glr_grammar_get_symbol (grammar, tb);
  glr_grammar_add_production (grammar, b, body, 1);
  body[0] = glr_grammar_get_symbol (grammar, ta);
  glr_grammar_add_production (grammar, c, body, 1);

  return grammar;
}

static int
count_symbol_named (const glr_grammar_t *grammar, const char *name)
{
  size_t i;

  for (i = 0; i < grammar->symbol_count; i++)
    {
      if (strcmp (grammar->symbols[i]->name, name) == 0)
        {
          return 1;
        }
    }

  return 0;
}

static int
has_left_recursive_production (const glr_grammar_t *grammar)
{
  size_t i;

  for (i = 0; i < grammar->production_count; i++)
    {
      glr_production_t *production = grammar->productions[i];
      if (production->body_length > 0 && production->body[0] == production->head)
        {
          return 1;
        }
    }

  return 0;
}

static int
has_unit_production (const glr_grammar_t *grammar)
{
  size_t i;

  for (i = 0; i < grammar->production_count; i++)
    {
      glr_production_t *production = grammar->productions[i];
      if (production->body_length == 1
          && production->body[0]->type == GLR_SYMBOL_NONTERMINAL)
        {
          return 1;
        }
    }

  return 0;
}

static int
has_named_epsilon (const glr_grammar_t *grammar, const char *name)
{
  size_t i;

  for (i = 0; i < grammar->production_count; i++)
    {
      glr_production_t *production = grammar->productions[i];
      if (production->body_length == 0
          && strcmp (production->head->name, name) == 0)
        {
          return 1;
        }
    }

  return 0;
}

static void
test_procedural_pipeline (void)
{
  glr_grammar_t *grammar = build_sample_grammar ();

  printf ("Testing: procedural rewrite pipeline... ");
  ASSERT (grammar != NULL, "grammar should be created");
  ASSERT (glr_rewrite_make_lr_compatible (grammar) == GLR_REWRITE_STATUS_OK,
          "pipeline should succeed");
  ASSERT (!has_left_recursive_production (grammar),
          "left recursion should be removed");
  ASSERT (!has_unit_production (grammar), "unit productions should be removed");
  ASSERT (!has_named_epsilon (grammar, "A"),
          "original nullable production should be removed");
  ASSERT (!count_symbol_named (grammar, "C"),
          "unreachable symbol should be removed");
  glr_grammar_destroy (grammar);
  printf ("PASSED\n");
}

static void
test_grl_parser (void)
{
  static const char source[]
      = "(rewrite (name rewrite-test) (rules (rename-symbol B Bee)"
        " (set-start S)))";
  char error[256];
  glr_rewrite_program_t *program;
  glr_grammar_t *grammar = build_sample_grammar ();

  printf ("Testing: GRL parser and executor... ");
  program = glr_rewrite_program_parse (source, sizeof (source) - 1, error,
                                       sizeof (error));
  ASSERT (program != NULL, error);
  ASSERT (grammar != NULL, "grammar should be created");
  ASSERT (glr_rewrite_program_apply (grammar, program, NULL)
              == GLR_REWRITE_STATUS_OK,
          "program should execute");
  ASSERT (count_symbol_named (grammar, "Bee"), "rename-symbol should apply");
  ASSERT (!count_symbol_named (grammar, "B"), "old symbol name should be gone");
  glr_rewrite_program_destroy (program);
  glr_grammar_destroy (grammar);
  printf ("PASSED\n");
}

int
main (void)
{
  printf ("=== LibGLR Rewrite Tests ===\n\n");
  test_procedural_pipeline ();
  test_grl_parser ();
  printf ("\n=== Results ===\nPassed: %d\nFailed: %d\n", tests_passed,
          tests_failed);
  return tests_failed > 0 ? 1 : 0;
}
