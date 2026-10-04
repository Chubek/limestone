#include <glr/forest.h>
#include <stdio.h>
#include <stdlib.h>

static int tests_passed = 0;
static int tests_failed = 0;

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

#define ASSERT_NULL(ptr, msg) ASSERT ((ptr) == NULL, msg)

#define ASSERT_NOT_NULL(ptr, msg) ASSERT ((ptr) != NULL, msg)

#define ASSERT_EQ(a, b, msg)                                                  \
  if ((a) == (b))                                                             \
    {                                                                         \
      tests_passed++;                                                         \
    }                                                                         \
  else                                                                        \
    {                                                                         \
      printf ("FAILED: %s\n", msg);                                           \
      tests_failed++;                                                         \
    }

static void
print_test_name (const char *name)
{
  printf ("Testing: %s... ", name);
}

static void
test_forest_create_destroy (void)
{
  print_test_name ("create_destroy");

  glr_forest_t *forest = glr_forest_create ();
  ASSERT_NOT_NULL (forest, "Failed to create forest");

  glr_forest_destroy (forest);
  printf ("PASSED\n");
}

static void
test_forest_get_node (void)
{
  print_test_name ("get_node");

  glr_forest_t *forest = glr_forest_create ();
  ASSERT_NOT_NULL (forest, "Failed to create forest");

  glr_forest_node_t *node
      = glr_forest_get_node (forest, GLR_NODE_TERMINAL, 1, 0);
  ASSERT_NOT_NULL (node, "Should create terminal node");
  ASSERT_EQ (node->type, GLR_NODE_TERMINAL, "Node type should be terminal");
  ASSERT_EQ (node->symbol_id, 1, "Symbol ID should be 1");

  glr_forest_node_t *node2
      = glr_forest_get_node (forest, GLR_NODE_TERMINAL, 1, 0);
  ASSERT_EQ (node, node2, "Should return same node");

  glr_forest_node_t *nonterm
      = glr_forest_get_node (forest, GLR_NODE_NONTERMINAL, 2, 0);
  ASSERT_NOT_NULL (nonterm, "Should create non-terminal node");

  glr_forest_destroy (forest);
  printf ("PASSED\n");
}

static void
test_forest_add_child (void)
{
  print_test_name ("add_child");

  glr_forest_t *forest = glr_forest_create ();
  ASSERT_NOT_NULL (forest, "Failed to create forest");

  glr_forest_node_t *parent
      = glr_forest_get_node (forest, GLR_NODE_NONTERMINAL, 1, 0);
  glr_forest_node_t *child
      = glr_forest_get_node (forest, GLR_NODE_TERMINAL, 2, 1);

  int ret = glr_forest_add_child (parent, child);
  ASSERT_EQ (ret, 0, "Should successfully add child");

  glr_forest_node_t **children = glr_forest_get_children (parent);
  ASSERT_NOT_NULL (children, "Should get children array");
  ASSERT_EQ (children[0], child, "Should have added child");
  ASSERT_EQ (parent->child_count, 1, "Should have 1 child");

  glr_forest_destroy (forest);
  printf ("PASSED\n");
}

static void
test_forest_add_edge (void)
{
  print_test_name ("add_edge");

  glr_forest_t *forest = glr_forest_create ();
  ASSERT_NOT_NULL (forest, "Failed to create forest");

  glr_forest_edge_t *edge = calloc (1, sizeof (glr_forest_edge_t));
  ASSERT_NOT_NULL (edge, "Failed to allocate edge");

  edge->nonterminal_id = 1;
  edge->start_position = 0;
  edge->end_position = 1;

  int ret = glr_forest_add_edge (forest, edge);
  ASSERT_EQ (ret, 0, "Should successfully add edge");

  glr_forest_edge_t *edges = glr_forest_get_edges (forest, 1);
  ASSERT_NOT_NULL (edges, "Should get edges at position 1");
  ASSERT_EQ (edges->nonterminal_id, 1, "Edge should have correct nonterminal");

  free (edge);
  glr_forest_destroy (forest);
  printf ("PASSED\n");
}

static void
test_forest_node_helpers (void)
{
  print_test_name ("node_helpers");

  glr_forest_t *forest = glr_forest_create ();
  ASSERT_NOT_NULL (forest, "Failed to create forest");

  glr_forest_node_t *term
      = glr_forest_get_node (forest, GLR_NODE_TERMINAL, 1, 0);
  glr_forest_node_t *nonterm
      = glr_forest_get_node (forest, GLR_NODE_NONTERMINAL, 2, 0);

  ASSERT (glr_forest_node_is_terminal (term), "Node should be terminal");
  ASSERT (!glr_forest_node_is_terminal (nonterm),
          "Node should not be terminal");
  ASSERT (glr_forest_node_is_nonterminal (nonterm),
          "Node should be non-terminal");
  ASSERT (!glr_forest_node_is_nonterminal (term),
          "Node should not be non-terminal");

  glr_forest_destroy (forest);
  printf ("PASSED\n");
}

static void
test_forest_null (void)
{
  print_test_name ("null");

  ASSERT_NULL (glr_forest_get_node (NULL, GLR_NODE_TERMINAL, 1, 0),
               "NULL forest get_node");
  ASSERT_NULL (glr_forest_get_children (NULL), "NULL get_children");
  ASSERT_NULL (glr_forest_get_edges (NULL, 0), "NULL get_edges");

  printf ("PASSED\n");
}

int
main (void)
{
  printf ("=== LibGLR Forest Tests ===\n\n");

  test_forest_create_destroy ();
  test_forest_get_node ();
  test_forest_add_child ();
  test_forest_add_edge ();
  test_forest_node_helpers ();
  test_forest_null ();

  printf ("\n=== Results ===\n");
  printf ("Passed: %d\n", tests_passed);
  printf ("Failed: %d\n", tests_failed);

  return tests_failed > 0 ? 1 : 0;
}
