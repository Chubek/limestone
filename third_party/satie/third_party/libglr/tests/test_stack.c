#include <glr/stack.h>
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

#define ASSERT_NULL(ptr, msg) ASSERT ((ptr) == NULL, msg)

#define ASSERT_NOT_NULL(ptr, msg) ASSERT ((ptr) != NULL, msg)

static void
print_test_name (const char *name)
{
  printf ("Testing: %s... ", name);
}

static void
test_stack_create_destroy (void)
{
  print_test_name ("create_destroy");

  glr_stack_t *stack = glr_stack_create ();
  ASSERT_NOT_NULL (stack, "Failed to create stack");

  glr_stack_destroy (stack);

  printf ("PASSED\n");
}

static void
test_stack_push_pop (void)
{
  print_test_name ("push_pop");

  glr_stack_t *stack = glr_stack_create ();
  ASSERT_NOT_NULL (stack, "Failed to create stack");

  void *data[5] = { (void *)1, (void *)2, (void *)3, (void *)4, (void *)5 };

  for (int i = 0; i < 5; i++)
    {
      int ret = glr_stack_push (stack, data[i]);
      ASSERT_EQ (ret, 0, "Push should succeed");
    }

  ASSERT_EQ (glr_stack_height (stack), 5, "Stack height should be 5");

  for (int i = 4; i >= 0; i--)
    {
      void *item = glr_stack_pop (stack);
      ASSERT_NOT_NULL (item, "Should pop item");
      ASSERT_EQ (item, data[i], "Should get correct item");
    }

  ASSERT_EQ (glr_stack_height (stack), 0, "Stack height should be 0");

  glr_stack_destroy (stack);
  printf ("PASSED\n");
}

static void
test_stack_peek (void)
{
  print_test_name ("peek");

  glr_stack_t *stack = glr_stack_create ();
  ASSERT_NOT_NULL (stack, "Failed to create stack");

  void *data1 = (void *)100;
  void *data2 = (void *)200;

  glr_stack_push (stack, data1);
  glr_stack_push (stack, data2);

  void *peeked = glr_stack_peek (stack);
  ASSERT_EQ (peeked, data2, "Peek should return top item");

  glr_stack_destroy (stack);
  printf ("PASSED\n");
}

static void
test_stack_get (void)
{
  print_test_name ("get");

  glr_stack_t *stack = glr_stack_create ();
  ASSERT_NOT_NULL (stack, "Failed to create stack");

  void *data[3] = { (void *)10, (void *)20, (void *)30 };

  glr_stack_push (stack, data[0]);
  glr_stack_push (stack, data[1]);
  glr_stack_push (stack, data[2]);

  void *item0 = glr_stack_get (stack, 0);
  ASSERT_EQ (item0, data[0], "Should get item at height 0");

  void *item2 = glr_stack_get (stack, 2);
  ASSERT_EQ (item2, data[2], "Should get item at height 2");

  void *invalid = glr_stack_get (stack, 10);
  ASSERT_NULL (invalid, "Should not get invalid height");

  glr_stack_destroy (stack);
  printf ("PASSED\n");
}

static void
test_stack_fork (void)
{
  print_test_name ("fork");

  glr_stack_t *stack = glr_stack_create ();
  ASSERT_NOT_NULL (stack, "Failed to create stack");

  void *data[3] = { (void *)1, (void *)2, (void *)3 };

  glr_stack_push (stack, data[0]);
  glr_stack_push (stack, data[1]);
  glr_stack_push (stack, data[2]);

  glr_stack_t *fork = glr_stack_fork (stack, 1);
  ASSERT_NOT_NULL (fork, "Failed to fork stack");

  ASSERT_EQ (glr_stack_height (fork), 1, "Fork should have height 1");

  void *forked_item = glr_stack_get (fork, 0);
  ASSERT_EQ (forked_item, data[0], "Fork should have original items");

  void *new_data = (void *)999;
  glr_stack_push (fork, new_data);
  ASSERT_EQ (glr_stack_height (fork), 2, "Fork height should be 2");
  ASSERT_EQ (glr_stack_height (stack), 3, "Original stack height unchanged");

  glr_stack_destroy (fork);
  glr_stack_destroy (stack);
  printf ("PASSED\n");
}

static void
test_stack_empty (void)
{
  print_test_name ("empty");

  glr_stack_t *stack = glr_stack_create ();
  ASSERT_NOT_NULL (stack, "Failed to create stack");

  ASSERT (glr_stack_empty (stack), "New stack should be empty");

  void *data = (void *)1;
  glr_stack_push (stack, data);
  ASSERT (!glr_stack_empty (stack), "Stack with item should not be empty");

  glr_stack_destroy (stack);
  printf ("PASSED\n");
}

static void
test_stack_null (void)
{
  print_test_name ("null");

  ASSERT_NULL (glr_stack_peek (NULL), "NULL stack peek should return NULL");
  ASSERT_NULL (glr_stack_pop (NULL), "NULL stack pop should return NULL");
  ASSERT_EQ (glr_stack_height (NULL), 0, "NULL stack height should be 0");

  printf ("PASSED\n");
}

int
main (void)
{
  printf ("=== LibGLR Stack Tests ===\n\n");

  test_stack_create_destroy ();
  test_stack_push_pop ();
  test_stack_peek ();
  test_stack_get ();
  test_stack_fork ();
  test_stack_empty ();
  test_stack_null ();

  printf ("\n=== Results ===\n");
  printf ("Passed: %d\n", tests_passed);
  printf ("Failed: %d\n", tests_failed);

  return tests_failed > 0 ? 1 : 0;
}
