/* tests/test_stratified.c
 *
 * Stratified unit tests for libglr: 80 focused cases covering every core
 * module (grammar, stack, fork, forest, reduction, graph, parsetbl,
 * parser, reader, lexer-hooks, disambiguation, serialization, diff,
 * dependency, forest-merge, incremental parsing, rewrite, cache).
 *
 * Cache-backed cases are guarded by HAVE_LMDB so the suite also builds
 * with ENABLE_CACHE=OFF (those cases degrade to null-contract checks).
 */

#include "test_common.h"

#include <glr/glr.h>

#include <glr/dependency.h>
#include <glr/diff.h>
#include <glr/forest-merge.h>
#include <glr/serialization.h>

#ifdef HAVE_LMDB
#include <glr/cache.h>
#endif

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Grammar with a single literal terminal "a" as its language. */
static glr_grammar_t *
strat_make_grammar (void)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int expr;
  int term;
  glr_symbol_t *body[1];

  if (grammar == NULL)
    {
      return NULL;
    }

  expr = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Expr");
  term = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "a");
  if (expr < 0 || term < 0)
    {
      glr_grammar_destroy (grammar);
      return NULL;
    }
  body[0] = glr_grammar_get_symbol (grammar, term);
  glr_grammar_add_production (grammar, expr, body, 1);
  glr_grammar_set_start_symbol (grammar, expr);
  return grammar;
}

static glr_disambig_result_t
strat_noop_hook (glr_disambig_context_t *context, size_t *winner,
                 void *user_data)
{
  (void) context;
  (void) user_data;
  if (winner != NULL)
    {
      *winner = 0;
    }
  return GLR_DISAMBIG_NO_MATCH;
}

/* Create a unique scratch directory for cache-backed cases. The path is
   written into `out` (size at least 64 bytes) and stays valid until the
   caller removes it. Returns false when a directory cannot be created.
   Only the cache-backed cases need it, so it is compiled out with them. */
#ifdef HAVE_LMDB
static bool
strat_make_temp_dir (char *out, size_t out_size)
{
  static unsigned long counter = 0;
  char templ[64];

  if (snprintf (templ, sizeof (templ), "/tmp/libglr_strat_%lu_%lu",
                (unsigned long) getpid (), counter++) >= (int) sizeof (templ))
    {
      return false;
    }
  if (mkdir (templ, 0755) != 0)
    {
      return false;
    }
  if (snprintf (out, out_size, "%s", templ) >= (int) out_size)
    {
      return false;
    }
  return true;
}

/* Remove a scratch directory and everything the cache put in it. libmdbx opens
   the path as a directory and leaves its lock file inside, so the directory
   cannot be removed until that file is; leaving it behind would litter /tmp
   with one directory per test run. */
static void
strat_remove_temp_dir (const char *path)
{
  char file[192];

  if (path == NULL || path[0] == '\0')
    {
      return;
    }
  snprintf (file, sizeof (file), "%s/mdbx.lck", path);
  (void) remove (file);
  snprintf (file, sizeof (file), "%s/mdbx.dat", path);
  (void) remove (file);
  (void) rmdir (path);
}
#endif /* HAVE_LMDB */

static bool
strat_lexer_hook (const glr_lexer_event_t *event,
                  glr_lexer_response_t *response, void *user_data)
{
  (void) user_data;
  if (event->codepoint != 0x0021)
    {
      return false;
    }
  glr_lexer_response_accept (response, "BANG", 2);
  return true;
}

/* ------------------------------------------------------------------ */
/* SLR(1) table construction (5)                                       */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Grammar (8)                                                        */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_grammar_create_destroy)
{
  glr_grammar_t *grammar;

  glr_test_begin ("strat grammar create/destroy");
  grammar = glr_grammar_create ();
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  glr_grammar_destroy (grammar);
  glr_grammar_destroy (NULL);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_grammar_add_symbols)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int term;
  int nonterm;

  glr_test_begin ("strat grammar add symbols");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  term = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
  GLR_TEST_ASSERT_EQ (term, 0, "first symbol id should be 0");
  nonterm = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "E");
  GLR_TEST_ASSERT_EQ (nonterm, 1, "second symbol id should be 1");
  GLR_TEST_ASSERT_EQ (glr_grammar_symbol_count (grammar), 2,
                      "symbol count accessor should report 2");
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_grammar_reject_empty)
{
  glr_grammar_t *grammar = glr_grammar_create ();

  glr_test_begin ("strat grammar rejects empty names");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  GLR_TEST_ASSERT_EQ (
      glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, ""), -1,
      "empty symbol name should be rejected");
  GLR_TEST_ASSERT_EQ (glr_grammar_symbol_count (grammar), 0,
                      "no symbol should be stored");
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_grammar_reject_null)
{
  glr_grammar_t *grammar = glr_grammar_create ();

  glr_test_begin ("strat grammar null contract");
  GLR_TEST_ASSERT_EQ (glr_grammar_add_symbol (NULL, GLR_SYMBOL_TERMINAL, "x"),
                      -1, "null grammar should fail");
  GLR_TEST_ASSERT_EQ (
      glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, NULL), -1,
      "null name should fail");
  GLR_TEST_ASSERT_NULL (glr_grammar_get_symbol (NULL, 0),
                        "null grammar lookup should be NULL");
  GLR_TEST_ASSERT_NULL (glr_grammar_get_symbol (grammar, 99),
                        "out-of-range lookup should be NULL");
  GLR_TEST_ASSERT_EQ (glr_grammar_symbol_count (NULL), 0,
                      "null symbol count should be 0");
  GLR_TEST_ASSERT_EQ (glr_grammar_production_count (NULL), 0,
                      "null production count should be 0");
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_grammar_find_by_name)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int id;

  glr_test_begin ("strat grammar find by name");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
  glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Expr");
  id = glr_grammar_find_symbol (grammar, "+", GLR_SYMBOL_TERMINAL);
  GLR_TEST_ASSERT_EQ (id, 0, "terminal lookup should find id 0");
  id = glr_grammar_find_symbol (grammar, "+", GLR_SYMBOL_NONTERMINAL);
  GLR_TEST_ASSERT_EQ (id, -1, "wrong-kind lookup should miss");
  id = glr_grammar_find_symbol (grammar, "Expr", GLR_SYMBOL_NONTERMINAL);
  GLR_TEST_ASSERT_EQ (id, 1, "nonterminal lookup should find id 1");
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_grammar_find_any)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int id;

  glr_test_begin ("strat grammar find any");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "tok");
  id = glr_grammar_find_symbol_any (grammar, "tok");
  GLR_TEST_ASSERT_EQ (id, 0, "typeless lookup should find the symbol");
  GLR_TEST_ASSERT_EQ (glr_grammar_find_symbol_any (grammar, "missing"), -1,
                      "missing name should miss");
  GLR_TEST_ASSERT_EQ (glr_grammar_find_symbol_any (NULL, "tok"), -1,
                      "null grammar should miss");
  GLR_TEST_ASSERT_EQ (glr_grammar_find_symbol (grammar, NULL,
                                               GLR_SYMBOL_TERMINAL),
                      -1, "null name should miss");
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_grammar_validate_ok)
{
  glr_grammar_t *grammar = strat_make_grammar ();
  char error[128];

  glr_test_begin ("strat grammar validate ok");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  memset (error, 0, sizeof (error));
  GLR_TEST_ASSERT (glr_grammar_validate (grammar, error, sizeof (error)),
                   "well-formed grammar should validate");
  GLR_TEST_ASSERT_EQ (glr_grammar_production_count (grammar), 1,
                      "production count accessor should report 1");
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_grammar_validate_no_start)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  char error[128];

  glr_test_begin ("strat grammar validate without start");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "S");
  GLR_TEST_ASSERT (!glr_grammar_validate (grammar, error, sizeof (error)),
                   "grammar without start symbol should not validate");
  GLR_TEST_ASSERT (!glr_grammar_validate (NULL, error, sizeof (error)),
                   "null grammar should not validate");
  GLR_TEST_ASSERT (!glr_grammar_validate (grammar, NULL, 0),
                   "validation without buffer should still fail");
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Stack (7) + GSS nodes (2)                                          */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_stack_create_destroy)
{
  glr_stack_t *stack;

  glr_test_begin ("strat stack create/destroy");
  stack = glr_stack_create ();
  GLR_TEST_ASSERT_NOT_NULL (stack, "stack should be created");
  GLR_TEST_ASSERT_EQ (glr_stack_height (stack), 0,
                      "new stack height should be 0");
  GLR_TEST_ASSERT (glr_stack_empty (stack), "new stack should be empty");
  GLR_TEST_ASSERT (glr_stack_is_empty (stack), "is_empty alias should agree");
  GLR_TEST_ASSERT (glr_stack_is_empty (NULL), "null stack counts as empty");
  glr_stack_destroy (stack);
  glr_stack_destroy (NULL);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_stack_push_peek_pop)
{
  glr_stack_t *stack = glr_stack_create ();

  glr_test_begin ("strat stack push/peek/pop");
  GLR_TEST_ASSERT_NOT_NULL (stack, "stack should be created");
  GLR_TEST_ASSERT_EQ (glr_stack_push (stack, (void *) 0x11), 0,
                      "push should succeed");
  GLR_TEST_ASSERT_EQ (glr_stack_push (stack, (void *) 0x22), 0,
                      "second push should succeed");
  GLR_TEST_ASSERT_EQ (glr_stack_peek (stack), (void *) 0x22,
                      "peek should show the top");
  GLR_TEST_ASSERT_EQ (glr_stack_get (stack, 0), (void *) 0x11,
                      "get(0) should show the bottom");
  GLR_TEST_ASSERT_EQ (glr_stack_pop (stack), (void *) 0x22,
                      "pop should return the top");
  GLR_TEST_ASSERT_EQ (glr_stack_height (stack), 1,
                      "height should shrink after pop");
  GLR_TEST_ASSERT_EQ (glr_stack_get_node_count (stack), 1,
                      "node count should equal height");
  glr_stack_destroy (stack);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_stack_push_null_fails)
{
  glr_test_begin ("strat stack push null safety");
  GLR_TEST_ASSERT_EQ (glr_stack_push (NULL, (void *) 0x1), -1,
                      "push to null stack should fail without crashing");
  GLR_TEST_ASSERT_EQ (glr_stack_height (NULL), 0,
                      "null height should be 0");
  GLR_TEST_ASSERT_NULL (glr_stack_peek (NULL), "null peek should be NULL");
  GLR_TEST_ASSERT_NULL (glr_stack_pop (NULL), "null pop should be NULL");
  GLR_TEST_ASSERT_NULL (glr_stack_get (NULL, 0), "null get should be NULL");
  GLR_TEST_ASSERT_EQ (glr_stack_reset (NULL), -1,
                      "null reset should fail");
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_stack_pop_empty)
{
  glr_stack_t *stack = glr_stack_create ();

  glr_test_begin ("strat stack empty pop");
  GLR_TEST_ASSERT_NOT_NULL (stack, "stack should be created");
  GLR_TEST_ASSERT_NULL (glr_stack_pop (stack), "pop on empty should be NULL");
  GLR_TEST_ASSERT_NULL (glr_stack_peek (stack),
                        "peek on empty should be NULL");
  GLR_TEST_ASSERT_NULL (glr_stack_get (stack, 3),
                        "out-of-range get should be NULL");
  glr_stack_destroy (stack);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_stack_fork)
{
  glr_stack_t *stack = glr_stack_create ();
  glr_stack_t *fork;

  glr_test_begin ("strat stack fork");
  GLR_TEST_ASSERT_NOT_NULL (stack, "stack should be created");
  glr_stack_push (stack, (void *) 0x1);
  glr_stack_push (stack, (void *) 0x2);
  glr_stack_push (stack, (void *) 0x3);
  fork = glr_stack_fork (stack, 2);
  GLR_TEST_ASSERT_NOT_NULL (fork, "fork should be created");
  GLR_TEST_ASSERT_EQ (glr_stack_height (fork), 2, "fork height should be 2");
  GLR_TEST_ASSERT_EQ (glr_stack_get (fork, 1), (void *) 0x2,
                      "fork should copy prefix states");
  GLR_TEST_ASSERT_NULL (glr_stack_fork (NULL, 0),
                        "fork of null should be NULL");
  glr_stack_destroy (fork);
  glr_stack_destroy (stack);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_stack_fork_too_tall)
{
  glr_stack_t *stack = glr_stack_create ();

  glr_test_begin ("strat stack fork beyond height");
  GLR_TEST_ASSERT_NOT_NULL (stack, "stack should be created");
  glr_stack_push (stack, (void *) 0x1);
  GLR_TEST_ASSERT_NULL (glr_stack_fork (stack, 99),
                        "over-height fork should be NULL");
  glr_stack_destroy (stack);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_stack_reset)
{
  glr_stack_t *stack = glr_stack_create ();

  glr_test_begin ("strat stack reset");
  GLR_TEST_ASSERT_NOT_NULL (stack, "stack should be created");
  glr_stack_push (stack, (void *) 0x5);
  glr_stack_push (stack, (void *) 0x6);
  GLR_TEST_ASSERT_EQ (glr_stack_reset (stack), 0, "reset should succeed");
  GLR_TEST_ASSERT_EQ (glr_stack_height (stack), 0,
                      "height should be 0 after reset");
  GLR_TEST_ASSERT (glr_stack_empty (stack), "stack should be empty");
  glr_stack_clear (stack);
  glr_stack_clear (NULL);
  GLR_TEST_ASSERT_EQ (glr_stack_get_node_count (NULL), 0,
                      "null node count should be 0");
  glr_stack_destroy (stack);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_stack_node_create)
{
  glr_stack_node_t *node;

  glr_test_begin ("strat gss node create");
  node = glr_stack_node_create (7, 42);
  GLR_TEST_ASSERT_NOT_NULL (node, "node should be created");
  GLR_TEST_ASSERT_EQ (glr_stack_node_get_state (node), 7,
                      "state getter should match");
  GLR_TEST_ASSERT_EQ (glr_stack_node_get_position (node), 42,
                      "position getter should match");
  GLR_TEST_ASSERT_EQ (glr_stack_node_get_parent_count (node), 0,
                      "new node should have no parents");
  GLR_TEST_ASSERT_EQ (glr_stack_node_get_state (NULL), 0,
                      "null state should be 0");
  GLR_TEST_ASSERT_EQ (glr_stack_node_get_position (NULL), 0,
                      "null position should be 0");
  GLR_TEST_ASSERT_NULL (glr_stack_node_get_parent (node, 0),
                        "missing parent should be NULL");
  glr_stack_node_destroy (node);
  glr_stack_node_free (NULL);
  glr_stack_node_destroy_tree (NULL);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_stack_node_parents)
{
  glr_stack_node_t *child = glr_stack_node_create (1, 10);
  glr_stack_node_t *parent = glr_stack_node_create (2, 20);

  glr_test_begin ("strat gss node parents");
  GLR_TEST_ASSERT_NOT_NULL (child, "child should be created");
  GLR_TEST_ASSERT_NOT_NULL (parent, "parent should be created");
  GLR_TEST_ASSERT_EQ (glr_stack_node_add_parent (child, parent), 0,
                      "attaching a parent should succeed");
  GLR_TEST_ASSERT_EQ (glr_stack_node_get_parent_count (child), 1,
                      "parent count should be 1");
  GLR_TEST_ASSERT_EQ (glr_stack_node_get_parent (child, 0), parent,
                      "parent getter should return the link");
  GLR_TEST_ASSERT_EQ (glr_stack_node_add_parent (NULL, parent), -1,
                      "null child should fail");
  GLR_TEST_ASSERT_EQ (glr_stack_node_add_parent (child, NULL), -1,
                      "null parent should fail");
  glr_stack_node_detach_last (child);
  GLR_TEST_ASSERT_EQ (glr_stack_node_get_parent_count (child), 0,
                      "detach should drop the count");
  glr_stack_node_detach_last (NULL);
  glr_stack_node_destroy (child);
  glr_stack_node_destroy (parent);
  {
    glr_stack_node_t *root = glr_stack_node_create (9, 90);
    glr_stack_node_t *leaf = glr_stack_node_create (8, 80);
    glr_stack_node_add_parent (root, leaf);
    glr_stack_node_destroy_tree (root);
  }
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Fork (3)                                                           */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_fork_create_destroy)
{
  glr_fork_t *fork;

  glr_test_begin ("strat fork create/destroy");
  fork = glr_fork_create (3);
  GLR_TEST_ASSERT_NOT_NULL (fork, "fork should be created");
  GLR_TEST_ASSERT_EQ (fork->height, 3, "fork height should match");
  GLR_TEST_ASSERT (!glr_fork_has_context (fork),
                   "new fork should have no context");
  glr_fork_destroy (fork);
  glr_fork_destroy (NULL);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_fork_next_null)
{
  glr_test_begin ("strat fork next null safety");
  GLR_TEST_ASSERT_NULL (glr_fork_next (NULL), "next of null should be NULL");
  GLR_TEST_ASSERT_NULL (glr_fork_get_context (NULL),
                        "context of null should be NULL");
  glr_fork_set_context (NULL, (void *) 0x1);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_fork_context)
{
  glr_fork_t *fork = glr_fork_create (1);
  int marker = 1234;

  glr_test_begin ("strat fork context");
  GLR_TEST_ASSERT_NOT_NULL (fork, "fork should be created");
  glr_fork_set_context (fork, &marker);
  GLR_TEST_ASSERT (glr_fork_has_context (fork), "fork should have context");
  GLR_TEST_ASSERT_EQ (glr_fork_get_context (fork), &marker,
                      "context getter should match");
  GLR_TEST_ASSERT_NULL (glr_fork_next (fork), "single fork has no next");
  glr_fork_destroy (fork);
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Forest (8)                                                         */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_forest_create_destroy)
{
  glr_forest_t *forest;

  glr_test_begin ("strat forest create/destroy");
  forest = glr_forest_create ();
  GLR_TEST_ASSERT_NOT_NULL (forest, "forest should be created");
  GLR_TEST_ASSERT_EQ (glr_forest_total_nodes (forest), 0,
                      "new forest should be empty");
  GLR_TEST_ASSERT_EQ (glr_forest_total_nodes (NULL), 0,
                      "null forest count should be 0");
  glr_forest_destroy (forest);
  glr_forest_destroy (NULL);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_forest_get_reuse)
{
  glr_forest_t *forest = glr_forest_create ();
  glr_forest_node_t *first;
  glr_forest_node_t *second;

  glr_test_begin ("strat forest node reuse");
  GLR_TEST_ASSERT_NOT_NULL (forest, "forest should be created");
  first = glr_forest_get_node (forest, GLR_NODE_TERMINAL, 4, 0);
  second = glr_forest_get_node (forest, GLR_NODE_TERMINAL, 4, 0);
  GLR_TEST_ASSERT_NOT_NULL (first, "node should be created");
  GLR_TEST_ASSERT_EQ (first, second, "same key should reuse the node");
  GLR_TEST_ASSERT (glr_forest_node_is_terminal (first),
                   "node should test as terminal");
  GLR_TEST_ASSERT (!glr_forest_node_is_nonterminal (first),
                   "terminal should not test as nonterminal");
  GLR_TEST_ASSERT_NULL (glr_forest_get_node (NULL, GLR_NODE_TERMINAL, 0, 0),
                        "null forest should yield NULL");
  glr_forest_destroy (forest);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_forest_add_child)
{
  glr_forest_t *forest = glr_forest_create ();
  glr_forest_node_t *parent;
  glr_forest_node_t *child;

  glr_test_begin ("strat forest add child");
  GLR_TEST_ASSERT_NOT_NULL (forest, "forest should be created");
  parent = glr_forest_get_node (forest, GLR_NODE_NONTERMINAL, 1, 0);
  child = glr_forest_get_node (forest, GLR_NODE_TERMINAL, 2, 0);
  GLR_TEST_ASSERT_NOT_NULL (parent, "parent should be created");
  GLR_TEST_ASSERT_NOT_NULL (child, "child should be created");
  GLR_TEST_ASSERT_EQ (glr_forest_add_child (parent, child), 0,
                      "adding a child should succeed");
  GLR_TEST_ASSERT_NOT_NULL (glr_forest_get_children (parent),
                            "children array should exist");
  GLR_TEST_ASSERT (glr_forest_node_is_nonterminal (parent),
                   "parent should test as nonterminal");
  glr_forest_destroy (forest);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_forest_edge_roundtrip)
{
  glr_forest_t *forest = glr_forest_create ();
  glr_forest_edge_t edge;

  glr_test_begin ("strat forest edge add/get");
  GLR_TEST_ASSERT_NOT_NULL (forest, "forest should be created");
  memset (&edge, 0, sizeof (edge));
  edge.nonterminal_id = 3;
  edge.start_position = 0;
  edge.end_position = 2;
  GLR_TEST_ASSERT_EQ (glr_forest_add_edge (forest, &edge), 0,
                      "edge insert should succeed");
  GLR_TEST_ASSERT_NOT_NULL (glr_forest_get_edges (forest, 2),
                            "edge should be retrievable at end position");
  GLR_TEST_ASSERT_NULL (glr_forest_get_edges (forest, 99),
                        "out-of-range edges should be NULL");
  GLR_TEST_ASSERT_NULL (glr_forest_get_edges (NULL, 0),
                        "null forest edges should be NULL");
  GLR_TEST_ASSERT_EQ (glr_forest_add_edge (NULL, &edge), -1,
                      "null forest insert should fail");
  GLR_TEST_ASSERT_EQ (glr_forest_add_edge (forest, NULL), -1,
                      "null edge insert should fail");
  glr_forest_destroy (forest);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_forest_clone_independent)
{
  glr_forest_t *forest = glr_forest_create ();
  glr_forest_node_t *parent;
  glr_forest_node_t *child;
  glr_forest_t *clone;

  glr_test_begin ("strat forest clone independence");
  GLR_TEST_ASSERT_NOT_NULL (forest, "forest should be created");
  parent = glr_forest_get_node (forest, GLR_NODE_NONTERMINAL, 1, 0);
  child = glr_forest_get_node (forest, GLR_NODE_TERMINAL, 2, 0);
  glr_forest_add_child (parent, child);
  clone = glr_forest_clone (forest);
  GLR_TEST_ASSERT_NOT_NULL (clone, "clone should be created");
  GLR_TEST_ASSERT_EQ (glr_forest_total_nodes (clone),
                      glr_forest_total_nodes (forest),
                      "clone should hold the same node count");
  GLR_TEST_ASSERT (clone->nodes[0] != forest->nodes[0],
                   "clone nodes should be fresh allocations");
  glr_forest_destroy (forest);
  GLR_TEST_ASSERT_EQ (glr_forest_total_nodes (clone), 2,
                      "clone should survive source destruction");
  GLR_TEST_ASSERT_NULL (glr_forest_clone (NULL), "null clone should be NULL");
  glr_forest_destroy (clone);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_forest_counts)
{
  glr_forest_t *forest = glr_forest_create ();

  glr_test_begin ("strat forest counts");
  GLR_TEST_ASSERT_NOT_NULL (forest, "forest should be created");
  glr_forest_get_node (forest, GLR_NODE_TERMINAL, 1, 0);
  glr_forest_get_node (forest, GLR_NODE_TERMINAL, 2, 0);
  glr_forest_get_node (forest, GLR_NODE_TERMINAL, 3, 5);
  GLR_TEST_ASSERT_EQ (glr_forest_node_count_at (forest, 0), 2,
                      "position 0 should hold two nodes");
  GLR_TEST_ASSERT_EQ (glr_forest_node_count_at (forest, 5), 1,
                      "position 5 should hold one node");
  GLR_TEST_ASSERT_EQ (glr_forest_node_count_at (forest, 99), 0,
                      "missing position should count 0");
  GLR_TEST_ASSERT_EQ (glr_forest_node_count_at (NULL, 0), 0,
                      "null forest should count 0");
  glr_forest_destroy (forest);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_forest_clear)
{
  glr_forest_t *forest = glr_forest_create ();

  glr_test_begin ("strat forest clear");
  GLR_TEST_ASSERT_NOT_NULL (forest, "forest should be created");
  glr_forest_get_node (forest, GLR_NODE_TERMINAL, 1, 0);
  glr_forest_clear (forest);
  GLR_TEST_ASSERT_EQ (glr_forest_total_nodes (forest), 0,
                      "cleared forest should be empty");
  glr_forest_clear (NULL);
  glr_forest_destroy (forest);
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Reduction (4)                                                      */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_reduction_item_add)
{
  glr_item_set_t *set = glr_item_set_create ();
  glr_item_t item = { .production_id = 2, .dot = 1, .start_position = 0 };

  glr_test_begin ("strat item set add/contains");
  GLR_TEST_ASSERT_NOT_NULL (set, "set should be created");
  GLR_TEST_ASSERT_EQ (glr_item_set_add (set, item), 0, "add should succeed");
  GLR_TEST_ASSERT (glr_item_set_contains (set, item),
                   "set should contain the item");
  {
    glr_item_t other = { .production_id = 9, .dot = 0, .start_position = 0 };
    GLR_TEST_ASSERT (!glr_item_set_contains (set, other),
                     "set should miss unknown items");
    GLR_TEST_ASSERT (!glr_item_set_contains (NULL, other),
                     "null set should miss");
  }
  GLR_TEST_ASSERT_EQ (glr_item_set_add (NULL, item), -1,
                      "null set add should fail");
  GLR_TEST_ASSERT_EQ (glr_item_get_production (&item), 2,
                      "production getter should match");
  GLR_TEST_ASSERT_EQ (glr_item_get_production (NULL), -1,
                      "null item production should be -1");
  glr_item_set_destroy (set);
  glr_item_set_destroy (NULL);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_reduction_item_duplicate)
{
  glr_item_set_t *set = glr_item_set_create ();
  glr_item_t item = { .production_id = 1, .dot = 1, .start_position = 4 };

  glr_test_begin ("strat item duplicate add");
  GLR_TEST_ASSERT_NOT_NULL (set, "set should be created");
  GLR_TEST_ASSERT_EQ (glr_item_set_add (set, item), 0,
                      "first add should succeed");
  GLR_TEST_ASSERT_EQ (glr_item_set_add (set, item), 0,
                      "duplicate add should be idempotent");
  GLR_TEST_ASSERT_EQ (glr_item_set_size (set), 1,
                      "duplicate should not grow the set");
  glr_item_set_destroy (set);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_reduction_clear_size_get)
{
  glr_item_set_t *set = glr_item_set_create ();
  glr_item_t item = { .production_id = 3, .dot = 2, .start_position = 1 };

  glr_test_begin ("strat item clear/size/get");
  GLR_TEST_ASSERT_NOT_NULL (set, "set should be created");
  glr_item_set_add (set, item);
  GLR_TEST_ASSERT_EQ (glr_item_set_size (set), 1, "size should be 1");
  GLR_TEST_ASSERT_NOT_NULL (glr_item_set_get (set, 0),
                            "get(0) should return the item");
  GLR_TEST_ASSERT_NULL (glr_item_set_get (set, 7),
                        "out-of-range get should be NULL");
  GLR_TEST_ASSERT_NULL (glr_item_set_get (NULL, 0),
                        "null set get should be NULL");
  GLR_TEST_ASSERT_EQ (glr_item_set_size (NULL), 0, "null size should be 0");
  glr_item_set_clear (set);
  GLR_TEST_ASSERT_EQ (glr_item_set_size (set), 0,
                      "size should be 0 after clear");
  glr_item_set_clear (NULL);
  glr_item_set_destroy (set);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_reduction_create)
{
  glr_reduction_t *reduction;
  glr_item_t incomplete = { .production_id = 1, .dot = 0,
                            .start_position = 0 };
  glr_item_t advanced = { .production_id = 1, .dot = 2,
                          .start_position = 0 };

  glr_test_begin ("strat reduction create/helpers");
  reduction = glr_reduction_create (5, 11);
  GLR_TEST_ASSERT_NOT_NULL (reduction, "reduction should be created");
  GLR_TEST_ASSERT_EQ (glr_reduction_get_production (reduction), 5,
                      "production getter should match");
  GLR_TEST_ASSERT_EQ (glr_reduction_get_production (NULL), -1,
                      "null reduction production should be -1");
  GLR_TEST_ASSERT (!glr_item_is_complete (&incomplete),
                   "dot at 0 should not count as complete");
  GLR_TEST_ASSERT (glr_item_is_complete (&advanced),
                   "advanced dot should count as complete");
  GLR_TEST_ASSERT (!glr_item_is_complete (NULL), "null item is incomplete");
  GLR_TEST_ASSERT (glr_item_is_complete_for_production (&advanced, 2),
                   "dot >= length should be complete");
  GLR_TEST_ASSERT (!glr_item_is_complete_for_production (&advanced, 5),
                   "dot < length should be incomplete");
  GLR_TEST_ASSERT (!glr_item_is_complete_for_production (NULL, 2),
                   "null item is never complete");
  glr_reduction_destroy (reduction);
  glr_reduction_destroy (NULL);
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Graph (6)                                                          */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_graph_create_destroy)
{
  glr_graph_t *graph;

  glr_test_begin ("strat graph create/destroy");
  graph = glr_graph_create ();
  GLR_TEST_ASSERT_NOT_NULL (graph, "graph should be created");
  GLR_TEST_ASSERT_EQ (glr_graph_node_count (graph), 0,
                      "new graph should have no nodes");
  GLR_TEST_ASSERT_EQ (glr_graph_edge_count (graph), 0,
                      "new graph should have no edges");
  glr_graph_destroy (graph);
  glr_graph_destroy (NULL);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_graph_add_get_node)
{
  glr_graph_t *graph = glr_graph_create ();
  int first;
  int second;

  glr_test_begin ("strat graph add/get node");
  GLR_TEST_ASSERT_NOT_NULL (graph, "graph should be created");
  first = glr_graph_add_node (graph, NULL);
  second = glr_graph_add_node (graph, NULL);
  GLR_TEST_ASSERT_EQ (first, 0, "first node id should be 0");
  GLR_TEST_ASSERT_EQ (second, 1, "second node id should be 1");
  GLR_TEST_ASSERT_NOT_NULL (glr_graph_get_node (graph, 0),
                            "node 0 should be retrievable");
  GLR_TEST_ASSERT_NULL (glr_graph_get_node (graph, 99),
                        "missing node should be NULL");
  GLR_TEST_ASSERT_NULL (glr_graph_get_node (NULL, 0),
                        "null graph node should be NULL");
  GLR_TEST_ASSERT_EQ (glr_graph_add_node (NULL, NULL), -1,
                      "null graph add should fail");
  glr_graph_destroy (graph);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_graph_add_has_edge)
{
  glr_graph_t *graph = glr_graph_create ();
  glr_graph_edge_t *edge;

  glr_test_begin ("strat graph add/has edge");
  GLR_TEST_ASSERT_NOT_NULL (graph, "graph should be created");
  glr_graph_add_node (graph, NULL);
  glr_graph_add_node (graph, NULL);
  edge = glr_graph_add_edge (graph, 0, 1, 4);
  GLR_TEST_ASSERT_NOT_NULL (edge, "edge should be created");
  GLR_TEST_ASSERT (glr_graph_has_edge (graph, 0, 1),
                   "edge should be reported present");
  GLR_TEST_ASSERT (!glr_graph_has_edge (graph, 1, 0),
                   "reverse edge should be absent");
  GLR_TEST_ASSERT (!glr_graph_has_edge (NULL, 0, 1),
                   "null graph should report no edge");
  GLR_TEST_ASSERT_NULL (glr_graph_add_edge (graph, 0, 99, 1),
                        "edge to missing node should be NULL");
  GLR_TEST_ASSERT_NULL (glr_graph_add_edge (NULL, 0, 1, 1),
                        "null graph edge should be NULL");
  GLR_TEST_ASSERT_EQ (glr_graph_edge_count (graph), 1,
                      "edge count should be 1");
  glr_graph_destroy (graph);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_graph_remove_edge)
{
  glr_graph_t *graph = glr_graph_create ();

  glr_test_begin ("strat graph remove edge");
  GLR_TEST_ASSERT_NOT_NULL (graph, "graph should be created");
  glr_graph_add_node (graph, NULL);
  glr_graph_add_node (graph, NULL);
  glr_graph_add_edge (graph, 0, 1, 2);
  GLR_TEST_ASSERT_EQ (glr_graph_remove_edge (graph, 0, 1), 1,
                      "one edge should be removed");
  GLR_TEST_ASSERT (!glr_graph_has_edge (graph, 0, 1),
                   "removed edge should be gone");
  GLR_TEST_ASSERT_EQ (glr_graph_remove_edge (graph, 0, 1), 0,
                      "second removal should remove nothing");
  GLR_TEST_ASSERT_EQ (glr_graph_remove_edge (NULL, 0, 1), -1,
                      "null graph removal should fail");
  glr_graph_destroy (graph);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_graph_clear)
{
  glr_graph_t *graph = glr_graph_create ();

  glr_test_begin ("strat graph clear");
  GLR_TEST_ASSERT_NOT_NULL (graph, "graph should be created");
  glr_graph_add_node (graph, NULL);
  glr_graph_add_node (graph, NULL);
  glr_graph_add_edge (graph, 0, 1, 1);
  glr_graph_clear (graph);
  GLR_TEST_ASSERT_EQ (glr_graph_node_count (graph), 0,
                      "cleared graph should have no nodes");
  GLR_TEST_ASSERT_EQ (glr_graph_edge_count (graph), 0,
                      "cleared graph should have no edges");
  GLR_TEST_ASSERT_EQ (glr_graph_add_node (graph, NULL), 0,
                      "graph should be reusable after clear");
  glr_graph_clear (NULL);
  glr_graph_destroy (graph);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_graph_counts_null)
{
  glr_test_begin ("strat graph null counts and inlines");
  GLR_TEST_ASSERT_EQ (glr_graph_node_count (NULL), 0,
                      "null node count should be 0");
  GLR_TEST_ASSERT_EQ (glr_graph_edge_count (NULL), 0,
                      "null edge count should be 0");
  GLR_TEST_ASSERT_NULL (glr_graph_node_outgoing (NULL),
                        "null outgoing should be NULL");
  GLR_TEST_ASSERT_NULL (glr_graph_node_incoming (NULL),
                        "null incoming should be NULL");
  GLR_TEST_ASSERT_EQ (glr_graph_node_outgoing_count (NULL), 0,
                      "null outgoing count should be 0");
  GLR_TEST_ASSERT_EQ (glr_graph_node_incoming_count (NULL), 0,
                      "null incoming count should be 0");
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Parse table (5)                                                    */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_parsetbl_create_destroy)
{
  glr_parse_table_t *table;
  glr_parse_table_t *empty;

  glr_test_begin ("strat parsetbl create/destroy");
  table = glr_parse_table_create (4, 3, 2);
  GLR_TEST_ASSERT_NOT_NULL (table, "table should be created");
  GLR_TEST_ASSERT_EQ (table->state_count, 4, "state count should match");
  GLR_TEST_ASSERT_EQ (table->terminal_count, 3, "terminal count should match");
  GLR_TEST_ASSERT_EQ (table->nonterminal_count, 2,
                      "nonterminal count should match");
  empty = glr_parse_table_create (0, 0, 0);
  GLR_TEST_ASSERT_NOT_NULL (empty, "zero-size table should still allocate");
  GLR_TEST_ASSERT_EQ (empty->state_count, 0, "zero-size table has no states");
  glr_parse_table_destroy (empty);
  glr_parse_table_destroy (table);
  glr_parse_table_destroy (NULL);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_parsetbl_action_add_get)
{
  glr_parse_table_t *table = glr_parse_table_create (2, 2, 1);
  glr_action_t action;
  const glr_action_set_t *set;

  glr_test_begin ("strat parsetbl action add/get");
  GLR_TEST_ASSERT_NOT_NULL (table, "table should be created");
  memset (&action, 0, sizeof (action));
  action.type = GLR_ACTION_SHIFT;
  action.shift.next_state = 1;
  GLR_TEST_ASSERT_EQ (glr_parse_table_add_action (table, 0, 1, action), 0,
                      "action insert should succeed");
  set = glr_parse_table_get_actions (table, 0, 1);
  GLR_TEST_ASSERT_NOT_NULL (set, "action set should be retrievable");
  GLR_TEST_ASSERT_EQ (set->action_count, 1, "one action should be stored");
  GLR_TEST_ASSERT_NULL (glr_parse_table_get_actions (table, 9, 1),
                        "out-of-range state should be NULL");
  GLR_TEST_ASSERT_NULL (glr_parse_table_get_actions (NULL, 0, 0),
                        "null table should be NULL");
  glr_parse_table_destroy (table);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_parsetbl_oob_fails)
{
  glr_parse_table_t *table = glr_parse_table_create (1, 1, 1);
  glr_action_t action;

  glr_test_begin ("strat parsetbl out-of-bounds");
  GLR_TEST_ASSERT_NOT_NULL (table, "table should be created");
  memset (&action, 0, sizeof (action));
  GLR_TEST_ASSERT_EQ (glr_parse_table_add_action (table, 7, 0, action), -1,
                      "bad state should fail");
  GLR_TEST_ASSERT_EQ (glr_parse_table_add_action (table, 0, 7, action), -1,
                      "bad terminal should fail");
  GLR_TEST_ASSERT_EQ (glr_parse_table_add_action (NULL, 0, 0, action), -1,
                      "null table should fail");
  GLR_TEST_ASSERT_EQ (glr_parse_table_set_goto (table, 7, 0, 0), -1,
                      "bad goto state should fail");
  GLR_TEST_ASSERT_EQ (glr_parse_table_set_goto (NULL, 0, 0, 0), -1,
                      "null goto table should fail");
  glr_parse_table_destroy (table);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_parsetbl_conflict)
{
  glr_parse_table_t *table = glr_parse_table_create (1, 1, 1);
  glr_action_t shift;
  glr_action_t reduce;

  glr_test_begin ("strat parsetbl conflict detection");
  GLR_TEST_ASSERT_NOT_NULL (table, "table should be created");
  memset (&shift, 0, sizeof (shift));
  memset (&reduce, 0, sizeof (reduce));
  shift.type = GLR_ACTION_SHIFT;
  shift.shift.next_state = 0;
  reduce.type = GLR_ACTION_REDUCE;
  reduce.reduce.production_id = 0;
  glr_parse_table_add_action (table, 0, 0, shift);
  GLR_TEST_ASSERT (!glr_parse_table_has_conflict (table, 0, 0),
                   "single action should not conflict");
  GLR_TEST_ASSERT_EQ (glr_parse_table_action_count (table, 0, 0), 1,
                      "action count should be 1");
  glr_parse_table_add_action (table, 0, 0, reduce);
  GLR_TEST_ASSERT (glr_parse_table_has_conflict (table, 0, 0),
                   "two actions should conflict");
  GLR_TEST_ASSERT_EQ (glr_parse_table_conflict_count (table), 1,
                      "conflict cell count should be 1");
  GLR_TEST_ASSERT_EQ (glr_parse_table_action_count (NULL, 0, 0), 0,
                      "null table count should be 0");
  GLR_TEST_ASSERT_EQ (glr_parse_table_conflict_count (NULL), 0,
                      "null conflict count should be 0");
  glr_parse_table_destroy (table);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_parsetbl_goto)
{
  glr_parse_table_t *table = glr_parse_table_create (2, 1, 2);
  uint32_t next = 99;

  glr_test_begin ("strat parsetbl goto");
  GLR_TEST_ASSERT_NOT_NULL (table, "table should be created");
  GLR_TEST_ASSERT_EQ (glr_parse_table_set_goto (table, 0, 1, 1), 0,
                      "goto insert should succeed");
  GLR_TEST_ASSERT_EQ (glr_parse_table_get_goto (table, 0, 1, &next), 0,
                      "goto lookup should succeed");
  GLR_TEST_ASSERT_EQ (next, 1, "goto target should match");
  GLR_TEST_ASSERT_EQ (glr_parse_table_set_goto (table, 0, 1, 0), 0,
                      "goto update should succeed");
  GLR_TEST_ASSERT_EQ (glr_parse_table_get_goto (table, 0, 0, &next), -1,
                      "missing goto should fail");
  GLR_TEST_ASSERT_EQ (glr_parse_table_get_goto (table, 0, 1, NULL), -1,
                      "null out pointer should fail");
  GLR_TEST_ASSERT_EQ (glr_parse_table_get_goto (NULL, 0, 1, &next), -1,
                      "null table goto should fail");
  glr_parse_table_destroy (table);
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Parser (7)                                                         */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_parser_create_destroy)
{
  glr_grammar_t *grammar = strat_make_grammar ();
  glr_parser_t *parser;

  glr_test_begin ("strat parser create/destroy");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  parser = glr_parser_create (grammar);
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  GLR_TEST_ASSERT_NULL (glr_parser_create (NULL),
                        "null grammar should yield NULL");
  glr_parser_destroy (parser);
  glr_parser_destroy (NULL);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_parser_null_contract)
{
  glr_parse_result_t result;

  glr_test_begin ("strat parser null contract");
  result = glr_parse (NULL, "abc", 3);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_MEMORY,
                      "null parser should report memory-style error");
  GLR_TEST_ASSERT_NULL (result.forest, "null parser yields no forest");
  GLR_TEST_ASSERT_EQ (glr_parser_reset (NULL), -1,
                      "null reset should fail");
  GLR_TEST_ASSERT_NULL (glr_parser_get_lexer_hooks (NULL),
                        "null hooks getter should be NULL");
  GLR_TEST_ASSERT_NULL (glr_parser_get_parse_table (NULL),
                        "null table getter should be NULL");
  GLR_TEST_ASSERT_NULL (glr_parser_get_last_token (NULL),
                        "null last token should be NULL");
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_parser_parse_ascii)
{
  glr_grammar_t *grammar = strat_make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_result_t result;

  glr_test_begin ("strat parser parses ascii bytes");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  result = glr_parse (parser, "a", 1);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS,
                      "byte input should parse");
  GLR_TEST_ASSERT_EQ (result.position, 1, "position should advance by bytes");
  GLR_TEST_ASSERT_NOT_NULL (result.forest, "forest should be returned");
  GLR_TEST_ASSERT_EQ (glr_parser_stack_count (parser), 1,
                      "one stack should remain");
  GLR_TEST_ASSERT_EQ (glr_parser_stack_count (NULL), 0,
                      "null stack count should be 0");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_parser_rejects_unknown_token)
{
  glr_grammar_t *grammar = strat_make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_result_t result;

  glr_test_begin ("strat parser rejects unknown tokens");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  result = glr_parse (parser, "ab", 2);
  GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_ERROR_SYNTAX,
                      "'b' is not a terminal of the grammar");
  GLR_TEST_ASSERT_EQ (result.position, 1,
                      "error position should point past the accepted 'a'");
  GLR_TEST_ASSERT_NULL (result.forest, "failed parse should return no forest");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_parser_reset_userdata)
{
  glr_grammar_t *grammar = strat_make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  int marker = 42;

  glr_test_begin ("strat parser reset and user data");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  glr_parser_set_user_data (parser, &marker);
  GLR_TEST_ASSERT_EQ (glr_parser_get_user_data (parser), &marker,
                      "user data should round-trip");
  GLR_TEST_ASSERT_NULL (glr_parser_get_user_data (NULL),
                        "null user data should be NULL");
  GLR_TEST_ASSERT_EQ (glr_parser_get_error (parser), GLR_PARSE_SUCCESS,
                      "fresh parser should report success");
  GLR_TEST_ASSERT_NOT_NULL (glr_parser_get_forest (parser),
                            "forest accessor should be non-null");
  GLR_TEST_ASSERT_NULL (glr_parser_get_forest (NULL),
                        "null forest accessor should be NULL");
  GLR_TEST_ASSERT_EQ (glr_parser_reset (parser), 0,
                      "reset should succeed");
  glr_parser_set_user_data (NULL, &marker);
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_parser_table_override)
{
  glr_grammar_t *grammar = strat_make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_table_t *table = glr_parse_table_create (1, 1, 1);

  glr_test_begin ("strat parser parse-table override");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  GLR_TEST_ASSERT_NOT_NULL (table, "table should be created");
  GLR_TEST_ASSERT_EQ (glr_parser_set_parse_table (parser, table, true), 0,
                      "override install should succeed");
  GLR_TEST_ASSERT_EQ (glr_parser_get_parse_table (parser), table,
                      "override getter should match");
  GLR_TEST_ASSERT_EQ (glr_parser_set_parse_table (NULL, table, false), -1,
                      "null parser override should fail");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_parser_lexer_attach)
{
  glr_grammar_t *grammar = strat_make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_lexer_hooks_t *hooks = glr_lexer_hooks_create ();

  glr_test_begin ("strat parser lexer hooks attach");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  GLR_TEST_ASSERT_NOT_NULL (hooks, "hooks should be created");
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_add (hooks, "bang", 10,
                                           strat_lexer_hook, NULL, NULL),
                      0, "hook registration should succeed");
  GLR_TEST_ASSERT_EQ (glr_parser_set_lexer_hooks (parser, hooks), 0,
                      "parser should accept hooks");
  GLR_TEST_ASSERT_EQ (glr_parser_get_lexer_hooks (parser), hooks,
                      "hooks getter should match");
  GLR_TEST_ASSERT_EQ (glr_parser_set_lexer_hooks (NULL, hooks), -1,
                      "null parser hook install should fail");
  glr_lexer_hooks_destroy (hooks);
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_parser_disambig_lifecycle)
{
  glr_grammar_t *grammar = strat_make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_disambig_hook_t *hook;

  glr_test_begin ("strat parser disambiguator lifecycle");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  hook = glr_disambig_hook_create ("strat", 10, strat_noop_hook, NULL, NULL);
  GLR_TEST_ASSERT_NOT_NULL (hook, "hook should be created");
  GLR_TEST_ASSERT_EQ (glr_parser_add_disambiguator (parser, hook), 0,
                      "hook install should succeed");
  GLR_TEST_ASSERT_EQ (glr_parser_disambiguator_count (parser), 1,
                      "one hook should be registered");
  GLR_TEST_ASSERT_EQ (glr_parser_disambiguator_count (NULL), 0,
                      "null parser has no hooks");
  GLR_TEST_ASSERT_EQ (glr_parser_add_disambiguator (NULL, hook), -1,
                      "null parser install should fail");
  GLR_TEST_ASSERT_EQ (glr_parser_remove_disambiguator (parser, "missing"),
                      -1, "removing an unknown hook should fail");
  GLR_TEST_ASSERT_EQ (glr_parser_remove_disambiguator (NULL, "strat"), -1,
                      "null parser removal should fail");
  GLR_TEST_ASSERT_EQ (glr_parser_remove_disambiguator (parser, "strat"), 0,
                      "named removal should succeed");
  GLR_TEST_ASSERT_EQ (glr_parser_disambiguator_count (parser), 0,
                      "no hooks should remain");
  glr_parser_clear_disambiguators (parser);
  glr_parser_clear_disambiguators (NULL);
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Reader (4)                                                         */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_reader_create_input)
{
  glr_reader_t *reader = glr_reader_create ();
  static const unsigned char input[] = { 0x41, 0x00 };

  glr_test_begin ("strat reader create/input");
  GLR_TEST_ASSERT_NOT_NULL (reader, "reader should be created");
  GLR_TEST_ASSERT_EQ (glr_reader_set_input (reader, input, sizeof (input)),
                      0, "input install should succeed");
  GLR_TEST_ASSERT_EQ (glr_reader_set_input (NULL, input, sizeof (input)), -1,
                      "null reader input should fail");
  GLR_TEST_ASSERT_EQ (glr_reader_set_lexer_hooks (reader, NULL), 0,
                      "clearing hooks should succeed");
  GLR_TEST_ASSERT_EQ (glr_reader_set_lexer_hooks (NULL, NULL), -1,
                      "null reader hooks should fail");
  GLR_TEST_ASSERT_NULL (glr_reader_get_lexer_hooks (reader),
                        "hooks should be null");
  glr_reader_reset (reader);
  glr_reader_reset (NULL);
  glr_reader_destroy (reader);
  glr_reader_destroy (NULL);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_reader_offset_eof)
{
  glr_reader_t *reader = glr_reader_create ();
  static const unsigned char input[] = { 0x41, 0x00, 0x42, 0x00 };
  glr_reader_token_t token;

  glr_test_begin ("strat reader offset/eof");
  GLR_TEST_ASSERT_NOT_NULL (reader, "reader should be created");
  memset (&token, 0, sizeof (token));
  glr_reader_set_input (reader, input, sizeof (input));
  GLR_TEST_ASSERT_EQ (glr_reader_get_offset (reader), 0,
                      "offset should start at 0");
  GLR_TEST_ASSERT_EQ (glr_reader_get_offset (NULL), 0,
                      "null offset should be 0");
  GLR_TEST_ASSERT (!glr_reader_at_eof (reader), "reader should not be at eof");
  GLR_TEST_ASSERT (glr_reader_at_eof (NULL), "null counts as eof");
  GLR_TEST_ASSERT_EQ (glr_reader_remaining (reader), sizeof (input),
                      "all bytes should remain");
  GLR_TEST_ASSERT_EQ (glr_reader_remaining (NULL), 0,
                      "null remaining should be 0");
  GLR_TEST_ASSERT_EQ (glr_reader_next (reader, &token),
                      GLR_READER_STATUS_OK, "first token should decode");
  GLR_TEST_ASSERT (glr_reader_get_offset (reader) > 0,
                   "offset should advance");
  glr_reader_token_clear (&token);
  glr_reader_destroy (reader);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_reader_status_strings)
{
  glr_test_begin ("strat reader status strings");
  GLR_TEST_ASSERT_NOT_NULL (glr_reader_status_string (GLR_READER_STATUS_OK),
                            "ok string should exist");
  GLR_TEST_ASSERT_NOT_NULL (glr_reader_status_string (GLR_READER_STATUS_EOF),
                            "eof string should exist");
  GLR_TEST_ASSERT_NOT_NULL (
      glr_reader_status_string (GLR_READER_STATUS_INVALID_ARGUMENT),
      "invalid-argument string should exist");
  GLR_TEST_ASSERT_NOT_NULL (
      glr_reader_status_string (GLR_READER_STATUS_INVALID_ENCODING),
      "encoding string should exist");
  GLR_TEST_ASSERT_NOT_NULL (
      glr_reader_status_string (GLR_READER_STATUS_INVALID_SEQUENCE),
      "sequence string should exist");
  GLR_TEST_ASSERT_NOT_NULL (
      glr_reader_status_string (GLR_READER_STATUS_NO_MEMORY),
      "no-memory string should exist");
  GLR_TEST_ASSERT_NOT_NULL (glr_reader_status_string (
                                (glr_reader_status_t) 9999),
                            "unknown status should still stringify");
  GLR_TEST_ASSERT_EQ (glr_reader_next (NULL, NULL),
                      GLR_READER_STATUS_INVALID_ARGUMENT,
                      "null next should be invalid-argument");
  glr_reader_token_clear (NULL);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_reader_utf16_token)
{
  glr_reader_t *reader = glr_reader_create ();
  static const unsigned char input[] = { 0xFF, 0xFE, 0x41, 0x00 };
  glr_reader_token_t token;

  glr_test_begin ("strat reader utf16 bom token");
  GLR_TEST_ASSERT_NOT_NULL (reader, "reader should be created");
  memset (&token, 0, sizeof (token));
  glr_reader_set_encoding (reader, GLR_READER_ENCODING_UTF16_AUTO);
  glr_reader_set_input (reader, input, sizeof (input));
  GLR_TEST_ASSERT_EQ (glr_reader_next (reader, &token),
                      GLR_READER_STATUS_OK, "bom input should decode");
  GLR_TEST_ASSERT_NOT_NULL (token.terminal_name,
                            "decoded token should be named");
  GLR_TEST_ASSERT_EQ (glr_reader_get_encoding (reader),
                      GLR_READER_ENCODING_UTF16_LE,
                      "bom should resolve to little-endian");
  GLR_TEST_ASSERT_EQ (
      glr_reader_get_encoding (NULL), GLR_READER_ENCODING_UTF16_LE,
      "null reader encoding should fall back to little-endian");
  glr_reader_token_clear (&token);
  glr_reader_destroy (reader);
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Lexer hooks (3)                                                    */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_lexer_add_count)
{
  glr_lexer_hooks_t *hooks = glr_lexer_hooks_create ();

  glr_test_begin ("strat lexer add/count");
  GLR_TEST_ASSERT_NOT_NULL (hooks, "hooks should be created");
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_count (hooks), 0,
                      "new registry should be empty");
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_count (NULL), 0,
                      "null registry count should be 0");
  GLR_TEST_ASSERT_EQ (
      glr_lexer_hooks_add (hooks, "a", 1, strat_lexer_hook, NULL, NULL), 0,
      "first hook should register");
  GLR_TEST_ASSERT_EQ (
      glr_lexer_hooks_add (hooks, "b", 5, strat_lexer_hook, NULL, NULL), 0,
      "second hook should register");
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_count (hooks), 2,
                      "two hooks should be counted");
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_add (NULL, "x", 1, strat_lexer_hook,
                                           NULL, NULL),
                      -1, "null registry add should fail");
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_add (hooks, "x", 1, NULL, NULL, NULL),
                      -1, "null function add should fail");
  glr_lexer_hooks_set_user_data (hooks, (void *) 0x7);
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_get_user_data (hooks), (void *) 0x7,
                      "user data should round-trip");
  GLR_TEST_ASSERT_NULL (glr_lexer_hooks_get_user_data (NULL),
                        "null user data should be NULL");
  glr_lexer_hooks_set_user_data (NULL, NULL);
  glr_lexer_hooks_clear (hooks);
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_count (hooks), 0,
                      "clear should empty the registry");
  glr_lexer_hooks_clear (NULL);
  glr_lexer_hooks_destroy (hooks);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_lexer_dispatch_empty)
{
  glr_lexer_hooks_t *hooks = glr_lexer_hooks_create ();
  glr_lexer_event_t event;
  glr_lexer_response_t response;

  glr_test_begin ("strat lexer dispatch without hooks");
  GLR_TEST_ASSERT_NOT_NULL (hooks, "hooks should be created");
  memset (&event, 0, sizeof (event));
  memset (&response, 0, sizeof (response));
  event.codepoint = 0x41;
  event.default_bytes_consumed = 2;
  GLR_TEST_ASSERT (!glr_lexer_hooks_dispatch (hooks, &event, &response),
                   "empty registry should decline");
  GLR_TEST_ASSERT (!glr_lexer_hooks_dispatch (NULL, &event, &response),
                   "null registry should decline");
  GLR_TEST_ASSERT (!glr_lexer_hooks_dispatch (hooks, NULL, &response),
                   "null event should decline");
  GLR_TEST_ASSERT (!glr_lexer_hooks_dispatch (hooks, &event, NULL),
                   "null response should decline");
  glr_lexer_response_reset (&response);
  glr_lexer_response_reset (NULL);
  glr_lexer_response_accept (&response, "T", 2);
  GLR_TEST_ASSERT (response.accepted, "accept should flag the response");
  glr_lexer_response_accept (NULL, "T", 2);
  glr_lexer_hooks_destroy (hooks);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_lexer_remove)
{
  glr_lexer_hooks_t *hooks = glr_lexer_hooks_create ();

  glr_test_begin ("strat lexer remove");
  GLR_TEST_ASSERT_NOT_NULL (hooks, "hooks should be created");
  glr_lexer_hooks_add (hooks, "gone", 1, strat_lexer_hook, NULL, NULL);
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_remove (hooks, "gone"), 0,
                      "named removal should succeed");
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_count (hooks), 0,
                      "registry should be empty again");
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_remove (hooks, "gone"), -1,
                      "second removal should fail");
  GLR_TEST_ASSERT_EQ (glr_lexer_hooks_remove (NULL, "gone"), -1,
                      "null registry removal should fail");
  glr_lexer_hooks_destroy (hooks);
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Disambiguation (3)                                                 */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_disambig_hook_null)
{
  glr_grammar_t *grammar = strat_make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);

  glr_test_begin ("strat disambiguation hook validation");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  GLR_TEST_ASSERT_NULL (glr_disambig_hook_create ("x", 1, NULL, NULL, NULL),
                        "null hook function should fail");
  GLR_TEST_ASSERT_EQ (glr_parser_add_disambiguator (parser, NULL), -1,
                      "null hook install should fail");
  GLR_TEST_ASSERT_EQ (
      glr_parser_run_disambiguators (NULL, NULL, NULL),
      GLR_DISAMBIG_ERROR, "null run should report an error");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_active_count (NULL), 0,
                      "null context has no active candidates");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_reject_candidate (NULL, 0), -1,
                      "null reject should fail");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_select_candidate (NULL, 0), -1,
                      "null select should fail");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_disambig_reject_select)
{
  glr_disambig_candidate_t candidates[3];
  glr_disambig_context_t context;

  glr_test_begin ("strat disambiguation reject/select");
  memset (candidates, 0, sizeof (candidates));
  memset (&context, 0, sizeof (context));
  context.candidates = candidates;
  context.candidate_count = 3;
  GLR_TEST_ASSERT_EQ (glr_disambig_context_active_count (&context), 3,
                      "all candidates start active");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_reject_candidate (&context, 1),
                      0, "reject should succeed");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_active_count (&context), 2,
                      "one rejection should leave two active");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_reject_candidate (&context, 9),
                      -1, "out-of-range reject should fail");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_select_candidate (&context, 2),
                      0, "select should succeed");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_active_count (&context), 1,
                      "select should leave one active");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_select_candidate (&context, 9),
                      -1, "out-of-range select should fail");
  GLR_TEST_ASSERT (glr_disambig_candidate_is_active (&candidates[2]),
                   "selected candidate should test active");
  GLR_TEST_ASSERT (!glr_disambig_candidate_is_active (&candidates[0]),
                   "rejected candidate should test inactive");
  GLR_TEST_ASSERT (!glr_disambig_candidate_is_active (NULL),
                   "null candidate is inactive");
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_disambig_run_single)
{
  glr_grammar_t *grammar = strat_make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_disambig_candidate_t candidates[1];
  glr_disambig_context_t context;
  size_t winner = 99;

  glr_test_begin ("strat disambiguation single candidate run");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  memset (candidates, 0, sizeof (candidates));
  memset (&context, 0, sizeof (context));
  context.candidates = candidates;
  context.candidate_count = 1;
  GLR_TEST_ASSERT_EQ (
      glr_parser_run_disambiguators (parser, &context, &winner),
      GLR_DISAMBIG_RESOLVED, "single active candidate should resolve");
  GLR_TEST_ASSERT_EQ (winner, 0, "winner should be index 0");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_first_active (&context), 0,
                      "first active should be 0");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_last_active (&context), 0,
                      "last active should be 0");
  memset (candidates, 0, sizeof (candidates));
  candidates[0].rejected = true;
  GLR_TEST_ASSERT_EQ (
      glr_parser_run_disambiguators (parser, &context, NULL),
      GLR_DISAMBIG_NO_MATCH, "fully rejected context should not match");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_first_active (&context),
                      (size_t) -1, "no first active when all rejected");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_last_active (&context),
                      (size_t) -1, "no last active when all rejected");
  GLR_TEST_ASSERT_EQ (glr_disambig_context_last_active (NULL),
                      (size_t) -1, "null context last should be SIZE_MAX");
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Serialization (4)                                                  */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_serial_forest_roundtrip)
{
  glr_forest_t *forest = glr_forest_create ();
  glr_forest_node_t *parent;
  glr_forest_node_t *child;
  uint8_t *buffer = NULL;
  size_t size = 0;
  glr_forest_t *back = NULL;

  glr_test_begin ("strat serialization forest roundtrip");
  GLR_TEST_ASSERT_NOT_NULL (forest, "forest should be created");
  parent = glr_forest_get_node (forest, GLR_NODE_NONTERMINAL, 1, 0);
  child = glr_forest_get_node (forest, GLR_NODE_TERMINAL, 2, 0);
  glr_forest_add_child (parent, child);
  GLR_TEST_ASSERT_EQ (glr_serialize_forest (forest, &buffer, &size), 0,
                      "forest serialization should succeed");
  GLR_TEST_ASSERT_NOT_NULL (buffer, "buffer should be allocated");
  GLR_TEST_ASSERT_EQ (glr_deserialize_forest (buffer, size, &back), 0,
                      "forest deserialization should succeed");
  GLR_TEST_ASSERT_NOT_NULL (back, "restored forest should exist");
  GLR_TEST_ASSERT_EQ (glr_forest_total_nodes (back),
                      glr_forest_total_nodes (forest),
                      "node counts should survive the roundtrip");
  GLR_TEST_ASSERT_EQ (glr_serialize_forest (NULL, &buffer, &size), -1,
                      "null forest serialization should fail");
  GLR_TEST_ASSERT_EQ (glr_deserialize_forest (NULL, 0, &back), -1,
                      "null buffer deserialization should fail");
  free (buffer);
  glr_forest_destroy (back);
  glr_forest_destroy (forest);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_serial_node_roundtrip)
{
  glr_forest_t *forest = glr_forest_create ();
  glr_forest_node_t *node;
  uint8_t *buffer = NULL;
  size_t size = 0;
  glr_forest_node_t *back = NULL;

  glr_test_begin ("strat serialization node roundtrip");
  GLR_TEST_ASSERT_NOT_NULL (forest, "forest should be created");
  node = glr_forest_get_node (forest, GLR_NODE_TERMINAL, 6, 3);
  GLR_TEST_ASSERT_NOT_NULL (node, "node should be created");
  GLR_TEST_ASSERT_EQ (glr_serialize_forest_node (node, &buffer, &size), 0,
                      "node serialization should succeed");
  GLR_TEST_ASSERT_EQ (glr_deserialize_forest_node (buffer, size, &back), 0,
                      "node deserialization should succeed");
  GLR_TEST_ASSERT_EQ (back->symbol_id, 6, "symbol id should survive");
  GLR_TEST_ASSERT_EQ (back->position, 3, "position should survive");
  GLR_TEST_ASSERT_EQ (glr_serialize_forest_node (NULL, &buffer, &size), -1,
                      "null node serialization should fail");
  free (buffer);
  free (back->children);
  free (back);
  glr_forest_destroy (forest);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_serial_stack_node_roundtrip)
{
  glr_stack_node_t *node = glr_stack_node_create (11, 22);
  glr_stack_node_t *parent = glr_stack_node_create (33, 44);
  uint8_t *buffer = NULL;
  size_t size = 0;
  glr_stack_node_t *back = NULL;

  glr_test_begin ("strat serialization gss roundtrip");
  GLR_TEST_ASSERT_NOT_NULL (node, "node should be created");
  GLR_TEST_ASSERT_NOT_NULL (parent, "parent should be created");
  glr_stack_node_add_parent (node, parent);
  GLR_TEST_ASSERT_EQ (glr_serialize_stack_node (node, &buffer, &size), 0,
                      "gss serialization should succeed");
  GLR_TEST_ASSERT_EQ (glr_deserialize_stack_node (buffer, size, &back), 0,
                      "gss deserialization should succeed");
  GLR_TEST_ASSERT_EQ (glr_stack_node_get_state (back), 11,
                      "state should survive");
  GLR_TEST_ASSERT_EQ (glr_stack_node_get_position (back), 22,
                      "position should survive");
  GLR_TEST_ASSERT_EQ (glr_stack_node_get_parent_count (back), 1,
                      "parent count should survive");
  GLR_TEST_ASSERT_EQ (
      glr_stack_node_get_state (glr_stack_node_get_parent (back, 0)), 33,
      "parent state should survive");
  GLR_TEST_ASSERT_EQ (glr_serialize_stack_node (NULL, &buffer, &size), -1,
                      "null gss serialization should fail");
  free (buffer);
  glr_stack_node_destroy_tree (back);
  glr_stack_node_destroy (node);
  glr_stack_node_destroy (parent);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_serial_malformed_fails)
{
  static const uint8_t junk[] = { 0xDE, 0xAD, 0xBE, 0xEF,
                                  0x00, 0x11, 0x22, 0x33 };
  glr_forest_t *forest = NULL;
  glr_forest_node_t *node = NULL;
  glr_stack_node_t *gnode = NULL;

  glr_test_begin ("strat serialization malformed input");
  GLR_TEST_ASSERT_EQ (glr_deserialize_forest (junk, sizeof (junk), &forest),
                      -1, "junk forest payload should fail");
  GLR_TEST_ASSERT_EQ (
      glr_deserialize_forest_node (junk, sizeof (junk), &node), -1,
      "junk node payload should fail");
  GLR_TEST_ASSERT_EQ (
      glr_deserialize_stack_node (junk, sizeof (junk), &gnode), -1,
      "junk gss payload should fail");
  GLR_TEST_ASSERT_EQ (glr_deserialize_stack_node (junk, 2, &gnode), -1,
                      "truncated gss payload should fail");
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Diff (3)                                                           */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_diff_prefix_suffix)
{
  glr_test_begin ("strat diff prefix/suffix");
  GLR_TEST_ASSERT_EQ (glr_find_common_prefix ("abcdef", "abcxyz", 6, 6), 3,
                      "common prefix should be 3");
  GLR_TEST_ASSERT_EQ (glr_find_common_prefix ("abc", "xyz", 3, 3), 0,
                      "disjoint strings share no prefix");
  GLR_TEST_ASSERT_EQ (
      glr_find_common_suffix ("abcdef", "xyzdef", 6, 6, 0), 3,
      "common suffix should be 3");
  GLR_TEST_ASSERT_EQ (glr_find_common_suffix ("abc", "abc", 3, 3, 3), 0,
                      "fully consumed prefix leaves no suffix");
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_diff_insertion_deletion)
{
  glr_edit_t edit;

  glr_test_begin ("strat diff insertion/deletion");
  memset (&edit, 0, sizeof (edit));
  GLR_TEST_ASSERT_EQ (glr_compute_edit ("ac", 2, "abc", 3, &edit), 0,
                      "insertion diff should compute");
  GLR_TEST_ASSERT (edit.is_insertion, "middle byte should read as insertion");
  GLR_TEST_ASSERT_EQ (glr_compute_edit ("abc", 3, "ac", 2, &edit), 0,
                      "deletion diff should compute");
  GLR_TEST_ASSERT (edit.is_deletion, "removed byte should read as deletion");
  GLR_TEST_ASSERT_EQ (glr_compute_edit ("abc", 3, "axc", 3, &edit), 0,
                      "replacement diff should compute");
  GLR_TEST_ASSERT (edit.is_replacement,
                   "changed byte should read as replacement");
  GLR_TEST_ASSERT_EQ (glr_compute_edit (NULL, 0, "x", 1, &edit), -1,
                      "null old content should fail");
  GLR_TEST_ASSERT_EQ (glr_compute_edit ("x", 1, "x", 1, NULL), -1,
                      "null edit output should fail");
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_diff_empty_lengths)
{
  glr_edit_t edit;

  glr_test_begin ("strat diff empty edit");
  memset (&edit, 0, sizeof (edit));
  GLR_TEST_ASSERT_EQ (glr_compute_edit ("same", 4, "same", 4, &edit), 0,
                      "identical buffers should compute");
  GLR_TEST_ASSERT (glr_edit_is_empty (&edit), "identical edit should be empty");
  GLR_TEST_ASSERT_EQ (glr_edit_old_length (&edit), 0,
                      "empty old length should be 0");
  GLR_TEST_ASSERT_EQ (glr_edit_new_length (&edit), 0,
                      "empty new length should be 0");
  GLR_TEST_ASSERT (glr_edit_is_empty (NULL), "null edit counts as empty");
  GLR_TEST_ASSERT_EQ (glr_edit_old_length (NULL), 0,
                      "null old length should be 0");
  GLR_TEST_ASSERT_EQ (glr_edit_new_length (NULL), 0,
                      "null new length should be 0");
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Dependency (3)                                                     */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_dependency_add_get)
{
#ifdef HAVE_LMDB
  char path[128];
  glr_cache_config_t config;
  glr_cache_t *cache;
  glr_dependency_t *deps = NULL;
  size_t count = 0;

  glr_test_begin ("strat dependency add/get");
  GLR_TEST_ASSERT (strat_make_temp_dir (path, sizeof (path)),
                   "temp dir should be created");
  memset (&config, 0, sizeof (config));
  config.mdbx_path = path;
  config.map_size = 64 * 1024 * 1024;
  config.max_readers = 8;
  cache = glr_cache_open (&config);
  GLR_TEST_ASSERT_NOT_NULL (cache, "cache should open");
  GLR_TEST_ASSERT_EQ (
      glr_dependency_add (cache, 1001, GLR_CACHE_ENTRY_FOREST, 0, 10), 0,
      "dependency add should succeed");
  GLR_TEST_ASSERT_EQ (
      glr_dependency_add (cache, 1001, GLR_CACHE_ENTRY_FOREST, 0, 10), 0,
      "duplicate add should be idempotent");
  GLR_TEST_ASSERT_EQ (glr_dependency_count (cache), 1,
                      "one record should be tracked");
  GLR_TEST_ASSERT_EQ (
      glr_dependency_get_affected (cache, 5, 15, &deps, &count), 0,
      "overlapping query should succeed");
  GLR_TEST_ASSERT_EQ (count, 1, "one record should overlap");
  GLR_TEST_ASSERT_NOT_NULL (deps, "dependency list should be returned");
  glr_dependency_free_list (deps);
  deps = NULL;
  count = 0;
  GLR_TEST_ASSERT_EQ (
      glr_dependency_get_affected (cache, 50, 60, &deps, &count), 0,
      "disjoint query should succeed");
  GLR_TEST_ASSERT_EQ (count, 0, "no records should overlap");
  GLR_TEST_ASSERT_NULL (deps, "no list should be returned");
  glr_cache_close (cache);
  strat_remove_temp_dir (path);
#else
  glr_test_begin ("strat dependency add/get (no cache build)");
  GLR_TEST_ASSERT_EQ (glr_dependency_count (NULL), 0,
                      "null cache tracks nothing");
  GLR_TEST_ASSERT_EQ (glr_dependency_add (NULL, 1, 1, 0, 5), -1,
                      "null cache add should fail");
  glr_dependency_free_list (NULL);
#endif
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_dependency_invalidate)
{
#ifdef HAVE_LMDB
  char path[128];
  glr_cache_config_t config;
  glr_cache_t *cache;
  glr_dependency_t *deps = NULL;
  size_t count = 0;

  glr_test_begin ("strat dependency invalidate");
  GLR_TEST_ASSERT (strat_make_temp_dir (path, sizeof (path)),
                   "temp dir should be created");
  memset (&config, 0, sizeof (config));
  config.mdbx_path = path;
  config.map_size = 64 * 1024 * 1024;
  config.max_readers = 8;
  cache = glr_cache_open (&config);
  GLR_TEST_ASSERT_NOT_NULL (cache, "cache should open");
  glr_dependency_add (cache, 2001, GLR_CACHE_ENTRY_FOREST, 0, 10);
  glr_dependency_add (cache, 2002, GLR_CACHE_ENTRY_FOREST, 20, 30);
  GLR_TEST_ASSERT_EQ (glr_dependency_invalidate_range (cache, 0, 10), 0,
                      "invalidation should succeed");
  GLR_TEST_ASSERT_EQ (glr_dependency_count (cache), 1,
                      "one record should survive");
  GLR_TEST_ASSERT_EQ (
      glr_dependency_get_affected (cache, 0, 30, &deps, &count), 0,
      "survivor query should succeed");
  GLR_TEST_ASSERT_EQ (count, 1, "only the second record should remain");
  glr_dependency_free_list (deps);
  glr_cache_close (cache);
  strat_remove_temp_dir (path);
#else
  glr_test_begin ("strat dependency invalidate (no cache build)");
  GLR_TEST_ASSERT_EQ (glr_dependency_invalidate_range (NULL, 0, 5), -1,
                      "null cache invalidation should fail");
  GLR_TEST_ASSERT_EQ (
      glr_dependency_get_affected (NULL, 0, 5, NULL, NULL), -1,
      "null affected query should fail");
#endif
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_dependency_invalid_args)
{
#ifdef HAVE_LMDB
  char path[128];
  glr_cache_config_t config;
  glr_cache_t *cache;

  glr_test_begin ("strat dependency invalid args");
  GLR_TEST_ASSERT (strat_make_temp_dir (path, sizeof (path)),
                   "temp dir should be created");
  memset (&config, 0, sizeof (config));
  config.mdbx_path = path;
  config.map_size = 64 * 1024 * 1024;
  config.max_readers = 8;
  cache = glr_cache_open (&config);
  GLR_TEST_ASSERT_NOT_NULL (cache, "cache should open");
  GLR_TEST_ASSERT_EQ (glr_dependency_add (cache, 1, 1, 9, 9), -1,
                      "empty range add should fail");
  GLR_TEST_ASSERT_EQ (glr_dependency_add (cache, 1, 1, 10, 5), -1,
                      "inverted range add should fail");
  GLR_TEST_ASSERT_EQ (glr_dependency_add (cache, 1, 999, 0, 5), -1,
                      "bogus entry type should fail");
  GLR_TEST_ASSERT_EQ (glr_dependency_invalidate_range (cache, 5, 5), -1,
                      "empty invalidation should fail");
  glr_cache_close (cache);
  strat_remove_temp_dir (path);
#else
  glr_test_begin ("strat dependency invalid args (no cache build)");
  GLR_TEST_ASSERT_EQ (glr_dependency_count (NULL), 0,
                      "null cache count stays 0");
#endif
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Forest merge (2)                                                   */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_merge_all_null)
{
  glr_grammar_t *grammar = strat_make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_forest_t *out = NULL;

  glr_test_begin ("strat merge all-null inputs");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  GLR_TEST_ASSERT_EQ (glr_forest_merge (parser, NULL, NULL, NULL, &out), 0,
                      "all-null merge should succeed");
  GLR_TEST_ASSERT_NOT_NULL (out, "merge should produce a forest");
  GLR_TEST_ASSERT_EQ (glr_forest_merge (parser, NULL, NULL, NULL, NULL),
                      -1, "null out pointer should fail");
  glr_forest_destroy (out);
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_merge_adjust_positions)
{
  glr_forest_t *forest = glr_forest_create ();
  glr_forest_node_t *node;
  glr_grammar_t *grammar = strat_make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_forest_t *merged = NULL;

  glr_test_begin ("strat merge and adjust positions");
  GLR_TEST_ASSERT_NOT_NULL (forest, "forest should be created");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  node = glr_forest_get_node (forest, GLR_NODE_TERMINAL, 1, 10);
  GLR_TEST_ASSERT_NOT_NULL (node, "node should be created");
  GLR_TEST_ASSERT_EQ (glr_forest_adjust_positions (forest, 5, 3), 0,
                      "positive shift should succeed");
  GLR_TEST_ASSERT_EQ (node->position, 13, "position should shift by delta");
  GLR_TEST_ASSERT_EQ (glr_forest_adjust_positions (forest, 0, -20), -1,
                      "underflowing shift should fail");
  GLR_TEST_ASSERT_EQ (glr_forest_adjust_positions (NULL, 0, 1), -1,
                      "null forest adjust should fail");
  GLR_TEST_ASSERT_EQ (glr_forest_merge (parser, forest, NULL, NULL,
                                        &merged),
                      0, "single-input merge should succeed");
  GLR_TEST_ASSERT_NOT_NULL (merged, "merged forest should exist");
  GLR_TEST_ASSERT (merged->nodes != forest->nodes,
                   "merge must deep-copy the node table");
  glr_forest_destroy (merged);
  glr_forest_destroy (forest);
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Incremental parsing (2)                                            */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_incremental_fallback)
{
  glr_grammar_t *grammar = strat_make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_forest_t *out = NULL;

  glr_test_begin ("strat incremental falls back to full parse");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  GLR_TEST_ASSERT_EQ (glr_parser_parse_incremental (parser, NULL, NULL, 0,
                                                    "a", 1, 0, 0, &out),
                      0, "null history should trigger a full parse");
  GLR_TEST_ASSERT_NOT_NULL (out, "fallback should produce a forest");
  GLR_TEST_ASSERT_EQ (glr_parser_parse_incremental (NULL, NULL, NULL, 0, "a",
                                                    1, 0, 0, &out),
                      -1, "null parser should fail");
  GLR_TEST_ASSERT_EQ (
      glr_parser_parse_incremental (parser, NULL, NULL, 0, NULL, 0, 0, 0,
                                    &out),
      -1, "null new content should fail");
  glr_forest_destroy (out);
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_incremental_identical)
{
  glr_grammar_t *grammar = strat_make_grammar ();
  glr_parser_t *parser = glr_parser_create (grammar);
  glr_parse_result_t first;
  glr_forest_t *second = NULL;

  glr_test_begin ("strat incremental identical content clones");
  GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
  first = glr_parse (parser, "a", 1);
  GLR_TEST_ASSERT_EQ (first.error, GLR_PARSE_SUCCESS,
                      "initial parse should succeed");
  GLR_TEST_ASSERT_EQ (
      glr_parser_parse_incremental (parser, first.forest, "a", 1, "a", 1, 0,
                                    0, &second),
      0, "identical reparse should succeed");
  GLR_TEST_ASSERT_NOT_NULL (second, "incremental result should exist");
  GLR_TEST_ASSERT (second != first.forest,
                   "identical content must clone, never alias");
  if (second != NULL && second != first.forest)
    {
      glr_forest_destroy (second);
    }
  glr_parser_destroy (parser);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Rewrite (4)                                                        */
/* ------------------------------------------------------------------ */

static glr_grammar_t *
strat_rewrite_grammar (void)
{
  glr_grammar_t *grammar = glr_grammar_create ();
  int s;
  int a;
  int ta;
  glr_symbol_t *body[2];

  if (grammar == NULL)
    {
      return NULL;
    }

  s = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "S");
  a = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "A");
  ta = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "a");
  body[0] = glr_grammar_get_symbol (grammar, a);
  glr_grammar_add_production (grammar, s, body, 1);
  body[0] = glr_grammar_get_symbol (grammar, ta);
  glr_grammar_add_production (grammar, a, body, 1);
  glr_grammar_set_start_symbol (grammar, s);
  return grammar;
}

GLR_TEST_CASE (test_strat_rewrite_program_create)
{
  glr_rewrite_program_t *program;
  glr_rewrite_rule_t rule;

  glr_test_begin ("strat rewrite program create/add");
  program = glr_rewrite_program_create ("strat");
  GLR_TEST_ASSERT_NOT_NULL (program, "program should be created");
  memset (&rule, 0, sizeof (rule));
  rule.kind = GLR_REWRITE_RULE_REMOVE_USELESS_SYMBOLS;
  GLR_TEST_ASSERT_EQ (glr_rewrite_program_add_rule (program, &rule),
                      GLR_REWRITE_STATUS_OK, "rule append should succeed");
  GLR_TEST_ASSERT_EQ (
      glr_rewrite_program_add_rule (NULL, &rule),
      GLR_REWRITE_STATUS_INVALID_ARGUMENT, "null program add should fail");
  GLR_TEST_ASSERT_EQ (glr_rewrite_program_add_rule (program, NULL),
                      GLR_REWRITE_STATUS_INVALID_ARGUMENT,
                      "null rule add should fail");
  glr_rewrite_program_destroy (program);
  glr_rewrite_program_destroy (NULL);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_rewrite_parse_apply)
{
  static const char source[]
      = "(rewrite (name strat-test) (rules (rename-symbol A Bee)))";
  char error[256];
  glr_rewrite_program_t *program;
  glr_grammar_t *grammar = strat_rewrite_grammar ();

  glr_test_begin ("strat rewrite parse and apply");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  program = glr_rewrite_program_parse (source, sizeof (source) - 1, error,
                                       sizeof (error));
  GLR_TEST_ASSERT_NOT_NULL (program, "program should parse");
  GLR_TEST_ASSERT_EQ (glr_rewrite_program_apply (grammar, program, NULL),
                      GLR_REWRITE_STATUS_OK, "program should apply");
  GLR_TEST_ASSERT (glr_grammar_find_symbol_any (grammar, "Bee") >= 0,
                   "renamed symbol should exist");
  GLR_TEST_ASSERT (glr_grammar_find_symbol_any (grammar, "A") < 0,
                   "old symbol name should be gone");
  GLR_TEST_ASSERT_NULL (
      glr_rewrite_program_parse ("(not-a-program", 15, error,
                                 sizeof (error)),
      "malformed source should not parse");
  GLR_TEST_ASSERT_NULL (glr_rewrite_program_load_file (
                            "/nonexistent-strat-path.grl", error,
                            sizeof (error)),
                        "missing file should not load");
  glr_rewrite_program_destroy (program);
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_rewrite_add_rename)
{
  glr_grammar_t *grammar = strat_rewrite_grammar ();

  glr_test_begin ("strat rewrite add/rename helpers");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  GLR_TEST_ASSERT_EQ (
      glr_rewrite_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "b"),
      GLR_REWRITE_STATUS_OK, "helper add should succeed");
  GLR_TEST_ASSERT (glr_grammar_find_symbol_any (grammar, "b") >= 0,
                   "added symbol should be found");
  GLR_TEST_ASSERT_EQ (glr_rewrite_rename_symbol (grammar, "b", "B2"),
                      GLR_REWRITE_STATUS_OK, "helper rename should succeed");
  GLR_TEST_ASSERT_EQ (
      glr_rewrite_set_start (grammar, "S"), GLR_REWRITE_STATUS_OK,
      "helper set-start should succeed");
  GLR_TEST_ASSERT_EQ (
      glr_rewrite_set_start (grammar, "Nope"), GLR_REWRITE_STATUS_NOT_FOUND,
      "unknown start name should report not-found");
  GLR_TEST_ASSERT_EQ (
      glr_rewrite_rename_symbol (grammar, "Nope", "X"),
      GLR_REWRITE_STATUS_NOT_FOUND, "unknown rename should report not-found");
  GLR_TEST_ASSERT_EQ (glr_rewrite_add_symbol (NULL, GLR_SYMBOL_TERMINAL, "z"),
                      GLR_REWRITE_STATUS_INVALID_ARGUMENT,
                      "null grammar add should be invalid");
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_rewrite_lr_compat)
{
  glr_grammar_t *grammar = strat_rewrite_grammar ();

  glr_test_begin ("strat rewrite lr-compatible pipeline");
  GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
  GLR_TEST_ASSERT_EQ (glr_rewrite_make_lr_compatible (grammar),
                      GLR_REWRITE_STATUS_OK, "pipeline should succeed");
  GLR_TEST_ASSERT_EQ (glr_rewrite_remove_useless_symbols (grammar),
                      GLR_REWRITE_STATUS_OK, "cleanup should succeed");
  GLR_TEST_ASSERT_EQ (glr_rewrite_remove_epsilon_productions (grammar),
                      GLR_REWRITE_STATUS_OK, "epsilon pass should succeed");
  GLR_TEST_ASSERT_EQ (glr_rewrite_remove_unit_productions (grammar),
                      GLR_REWRITE_STATUS_OK, "unit pass should succeed");
  GLR_TEST_ASSERT_EQ (glr_rewrite_remove_left_recursion (grammar),
                      GLR_REWRITE_STATUS_OK, "left-recursion pass succeeds");
  GLR_TEST_ASSERT_EQ (glr_rewrite_left_factor (grammar),
                      GLR_REWRITE_STATUS_OK, "left-factor pass should succeed");
  GLR_TEST_ASSERT_EQ (
      glr_rewrite_make_lr_compatible (NULL),
      GLR_REWRITE_STATUS_INVALID_ARGUMENT, "null pipeline should be invalid");
  glr_grammar_destroy (grammar);
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Cache (2, guarded)                                                 */
/* ------------------------------------------------------------------ */

GLR_TEST_CASE (test_strat_cache_hash)
{
#ifdef HAVE_LMDB
  uint8_t first[32];
  uint8_t second[32];
  uint8_t other[32];

  glr_test_begin ("strat cache hash determinism");
  glr_cache_compute_hash ((const uint8_t *) "hello", 5, first);
  glr_cache_compute_hash ((const uint8_t *) "hello", 5, second);
  glr_cache_compute_hash ((const uint8_t *) "world", 5, other);
  GLR_TEST_ASSERT_EQ (memcmp (first, second, 32), 0,
                      "same input should hash identically");
  GLR_TEST_ASSERT (memcmp (first, other, 32) != 0,
                   "different inputs should hash differently");
#else
  glr_test_begin ("strat cache hash (no cache build)");
  GLR_TEST_ASSERT_EQ (glr_version () == NULL, 0,
                      "version probe keeps the case non-empty");
#endif
  glr_test_end ();
}

GLR_TEST_CASE (test_strat_cache_open_stats)
{
#ifdef HAVE_LMDB
  char path[128];
  glr_cache_config_t config;
  glr_cache_t *cache;
  glr_cache_stats_t cstats;

  glr_test_begin ("strat cache open/stats/clear");
  GLR_TEST_ASSERT (strat_make_temp_dir (path, sizeof (path)),
                   "temp dir should be created");
  memset (&config, 0, sizeof (config));
  config.mdbx_path = path;
  config.map_size = 64 * 1024 * 1024;
  config.max_readers = 8;
  cache = glr_cache_open (&config);
  GLR_TEST_ASSERT_NOT_NULL (cache, "cache should open");
  GLR_TEST_ASSERT_EQ (glr_cache_get_stats (cache, &cstats), 0,
                      "stats should be readable");
  GLR_TEST_ASSERT_EQ (glr_cache_sync (cache), 0, "sync should succeed");
  GLR_TEST_ASSERT_EQ (glr_cache_clear (cache), 0, "clear should succeed");
  GLR_TEST_ASSERT_EQ (glr_cache_vacuum (cache), 0, "vacuum should succeed");
  GLR_TEST_ASSERT_EQ (glr_cache_invalidate_range (cache, 0, 10), 0,
                      "empty invalidation should succeed");
  GLR_TEST_ASSERT_EQ (glr_cache_get_stats (NULL, &cstats), -1,
                      "null cache stats should fail");
  GLR_TEST_ASSERT_EQ (glr_cache_sync (NULL), -1, "null sync should fail");
  GLR_TEST_ASSERT_EQ (glr_cache_clear (NULL), -1, "null clear should fail");
  glr_cache_close (cache);
  strat_remove_temp_dir (path);
  glr_cache_close (NULL);
#else
  glr_test_begin ("strat cache open (no cache build)");
  GLR_TEST_ASSERT_NOT_NULL (glr_name (), "library name should exist");
#endif
  glr_test_end ();
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int
main (void)
{
  GLR_TEST_INIT;

  printf ("=== LibGLR Stratified Tests ===\n\n");


  test_strat_grammar_create_destroy (&stats);
  test_strat_grammar_add_symbols (&stats);
  test_strat_grammar_reject_empty (&stats);
  test_strat_grammar_reject_null (&stats);
  test_strat_grammar_find_by_name (&stats);
  test_strat_grammar_find_any (&stats);
  test_strat_grammar_validate_ok (&stats);
  test_strat_grammar_validate_no_start (&stats);

  test_strat_stack_create_destroy (&stats);
  test_strat_stack_push_peek_pop (&stats);
  test_strat_stack_push_null_fails (&stats);
  test_strat_stack_pop_empty (&stats);
  test_strat_stack_fork (&stats);
  test_strat_stack_fork_too_tall (&stats);
  test_strat_stack_reset (&stats);
  test_strat_stack_node_create (&stats);
  test_strat_stack_node_parents (&stats);

  test_strat_fork_create_destroy (&stats);
  test_strat_fork_next_null (&stats);
  test_strat_fork_context (&stats);

  test_strat_forest_create_destroy (&stats);
  test_strat_forest_get_reuse (&stats);
  test_strat_forest_add_child (&stats);
  test_strat_forest_edge_roundtrip (&stats);
  test_strat_forest_clone_independent (&stats);
  test_strat_forest_counts (&stats);
  test_strat_forest_clear (&stats);

  test_strat_reduction_item_add (&stats);
  test_strat_reduction_item_duplicate (&stats);
  test_strat_reduction_clear_size_get (&stats);
  test_strat_reduction_create (&stats);

  test_strat_graph_create_destroy (&stats);
  test_strat_graph_add_get_node (&stats);
  test_strat_graph_add_has_edge (&stats);
  test_strat_graph_remove_edge (&stats);
  test_strat_graph_clear (&stats);
  test_strat_graph_counts_null (&stats);

  test_strat_parsetbl_create_destroy (&stats);
  test_strat_parsetbl_action_add_get (&stats);
  test_strat_parsetbl_oob_fails (&stats);
  test_strat_parsetbl_conflict (&stats);
  test_strat_parsetbl_goto (&stats);

  test_strat_parser_create_destroy (&stats);
  test_strat_parser_null_contract (&stats);
  test_strat_parser_parse_ascii (&stats);
  test_strat_parser_rejects_unknown_token (&stats);
  test_strat_parser_reset_userdata (&stats);
  test_strat_parser_table_override (&stats);
  test_strat_parser_lexer_attach (&stats);
  test_strat_parser_disambig_lifecycle (&stats);

  test_strat_reader_create_input (&stats);
  test_strat_reader_offset_eof (&stats);
  test_strat_reader_status_strings (&stats);
  test_strat_reader_utf16_token (&stats);

  test_strat_lexer_add_count (&stats);
  test_strat_lexer_dispatch_empty (&stats);
  test_strat_lexer_remove (&stats);

  test_strat_disambig_hook_null (&stats);
  test_strat_disambig_reject_select (&stats);
  test_strat_disambig_run_single (&stats);

  test_strat_serial_forest_roundtrip (&stats);
  test_strat_serial_node_roundtrip (&stats);
  test_strat_serial_stack_node_roundtrip (&stats);
  test_strat_serial_malformed_fails (&stats);

  test_strat_diff_prefix_suffix (&stats);
  test_strat_diff_insertion_deletion (&stats);
  test_strat_diff_empty_lengths (&stats);

  test_strat_dependency_add_get (&stats);
  test_strat_dependency_invalidate (&stats);
  test_strat_dependency_invalid_args (&stats);

  test_strat_merge_all_null (&stats);
  test_strat_merge_adjust_positions (&stats);

  test_strat_incremental_fallback (&stats);
  test_strat_incremental_identical (&stats);

  test_strat_rewrite_program_create (&stats);
  test_strat_rewrite_parse_apply (&stats);
  test_strat_rewrite_add_rename (&stats);
  test_strat_rewrite_lr_compat (&stats);

  test_strat_cache_hash (&stats);
  test_strat_cache_open_stats (&stats);

  return glr_test_finish ("LibGLR Stratified", stats);
}
