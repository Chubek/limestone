#include <assert.h>
#include <glr/grammar.h>
#include <stdio.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name)                                                            \
  void test_##name (void);                                                    \
  void test_##name (void)

#define ASSERT(cond, msg)                                                     \
  if (cond)                                                                   \
    {                                                                         \
      tests_passed++;                                                         \
    }                                                                         \
  else                                                                        \
    {                                                                         \
      printf ("FAILED: %s\n", msg);                                           \
      tests_failed++;                                                         \
    }

#define ASSERT_EQ(a, b, msg) ASSERT ((a) == (b), msg)

#define ASSERT_NE(a, b, msg) ASSERT ((a) != (b), msg)

#define ASSERT_NULL(ptr, msg) ASSERT ((ptr) == NULL, msg)

#define ASSERT_NOT_NULL(ptr, msg) ASSERT ((ptr) != NULL, msg)

static void
print_test_name (const char *name)
{
  printf ("Testing: %s... ", name);
}

// Test 1: Create and destroy grammar
TEST (grammar_create_destroy)
{
  print_test_name ("create_destroy");

  glr_grammar_t *grammar = glr_grammar_create ();
  ASSERT_NOT_NULL (grammar, "Failed to create grammar");

  glr_grammar_destroy (grammar);
  ASSERT_NOT_NULL (grammar, "Grammar pointer should be valid");

  printf ("PASSED\n");
}

// Test 2: Add symbols
TEST (grammar_add_symbols)
{
  print_test_name ("add_symbols");

  glr_grammar_t *grammar = glr_grammar_create ();
  ASSERT_NOT_NULL (grammar, "Failed to create grammar");

  int term1 = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
  ASSERT_EQ (term1, 0, "First terminal should have ID 0");

  int term2 = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "*");
  ASSERT_EQ (term2, 1, "Second terminal should have ID 1");

  int nonterm
      = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "expression");
  ASSERT_EQ (nonterm, 2, "First non-terminal should have ID 2");

  ASSERT_EQ (grammar->symbol_count, 3, "Should have 3 symbols");

  glr_grammar_destroy (grammar);
  printf ("PASSED\n");
}

// Test 3: Get symbol by ID
TEST (grammar_get_symbol)
{
  print_test_name ("get_symbol");

  glr_grammar_t *grammar = glr_grammar_create ();
  ASSERT_NOT_NULL (grammar, "Failed to create grammar");

  glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
  glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "expr");

  glr_symbol_t *sym = glr_grammar_get_symbol (grammar, 0);
  ASSERT_NOT_NULL (sym, "Should find symbol with ID 0");
  ASSERT_EQ (sym->id, 0, "Symbol ID should be 0");
  ASSERT_EQ (sym->type, GLR_SYMBOL_TERMINAL, "Symbol should be terminal");
  ASSERT_EQ (strcmp (sym->name, "+"), 0, "Symbol name should be '+'");

  sym = glr_grammar_get_symbol (grammar, 99);
  ASSERT_NULL (sym, "Should not find symbol with ID 99");

  glr_grammar_destroy (grammar);
  printf ("PASSED\n");
}

// Test 4: Add production
TEST (grammar_add_production)
{
  print_test_name ("add_production");

  glr_grammar_t *grammar = glr_grammar_create ();
  ASSERT_NOT_NULL (grammar, "Failed to create grammar");

  // Add symbols
  int expr_id
      = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "expression");
  int add_id = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
  int term_id
      = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "term");

  // Create production: expression -> expression + term
  glr_symbol_t *body[3] = { glr_grammar_get_symbol (grammar, expr_id),
                            glr_grammar_get_symbol (grammar, add_id),
                            glr_grammar_get_symbol (grammar, term_id) };

  int prod_id = glr_grammar_add_production (grammar, expr_id, body, 3);
  ASSERT_EQ (prod_id, 0, "First production should have ID 0");
  ASSERT_EQ (grammar->production_count, 1, "Should have 1 production");

  glr_production_t *prod = glr_grammar_get_production (grammar, 0);
  ASSERT_NOT_NULL (prod, "Should find production with ID 0");
  ASSERT_EQ (prod->body_length, 3, "Production body should have 3 symbols");

  glr_grammar_destroy (grammar);
  printf ("PASSED\n");
}

// Test 5: Set start symbol
TEST (grammar_set_start)
{
  print_test_name ("set_start");

  glr_grammar_t *grammar = glr_grammar_create ();
  ASSERT_NOT_NULL (grammar, "Failed to create grammar");

  glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "start");
  glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "x");

  int ret = glr_grammar_set_start_symbol (grammar, 0);
  ASSERT_EQ (ret, 0, "Should successfully set start symbol");
  ASSERT_EQ (grammar->start_symbol->id, 0, "Start symbol should have ID 0");

  // Try invalid start symbol (terminal)
  glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "bad");
  ret = glr_grammar_set_start_symbol (grammar, 3);
  ASSERT_EQ (ret, -1, "Should not allow terminal as start symbol");

  glr_grammar_destroy (grammar);
  printf ("PASSED\n");
}

// Test 6: Inline helpers
TEST (grammar_helpers)
{
  print_test_name ("helpers");

  glr_grammar_t *grammar = glr_grammar_create ();
  ASSERT_NOT_NULL (grammar, "Failed to create grammar");

  glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
  glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "expr");

  glr_symbol_t *term = glr_grammar_get_symbol (grammar, 0);
  glr_symbol_t *nonterm = glr_grammar_get_symbol (grammar, 1);

  ASSERT (glr_symbol_is_terminal (term), "Symbol should be terminal");
  ASSERT (!glr_symbol_is_terminal (nonterm), "Symbol should not be terminal");
  ASSERT (glr_symbol_is_nonterminal (nonterm),
          "Symbol should be non-terminal");
  ASSERT (!glr_symbol_is_nonterminal (term),
          "Symbol should not be non-terminal");

  glr_grammar_destroy (grammar);
  printf ("PASSED\n");
}

int
main (void)
{
  printf ("=== LibGLR Grammar Tests ===\n\n");

  test_grammar_create_destroy ();
  test_grammar_add_symbols ();
  test_grammar_get_symbol ();
  test_grammar_add_production ();
  test_grammar_set_start ();
  test_grammar_helpers ();

  printf ("\n=== Results ===\n");
  printf ("Passed: %d\n", tests_passed);
  printf ("Failed: %d\n", tests_failed);

  return tests_failed > 0 ? 1 : 0;
}
