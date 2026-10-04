#include <glr/fork.h>
#include <stdio.h>

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
test_fork_create_destroy (void)
{
  print_test_name ("create_destroy");

  glr_fork_t *fork = glr_fork_create (5);
  ASSERT_NOT_NULL (fork, "Failed to create fork");

  ASSERT_EQ (fork->height, 5, "Fork height should be 5");

  glr_fork_destroy (fork);
  printf ("PASSED\n");
}

static void
test_fork_null (void)
{
  print_test_name ("null");

  ASSERT_NULL (glr_fork_next (NULL), "NULL fork next should be NULL");
  ASSERT (!glr_fork_has_context (NULL), "NULL fork should not have context");
  ASSERT_NULL (glr_fork_get_context (NULL),
               "NULL fork context should be NULL");

  glr_fork_set_context (NULL, (void *)1);

  printf ("PASSED\n");
}

static void
test_fork_context (void)
{
  print_test_name ("context");

  glr_fork_t *fork = glr_fork_create (3);
  ASSERT_NOT_NULL (fork, "Failed to create fork");

  ASSERT (!glr_fork_has_context (fork), "New fork should not have context");

  void *ctx = (void *)12345;
  glr_fork_set_context (fork, ctx);
  ASSERT (glr_fork_has_context (fork),
          "Fork should have context after setting");

  void *retrieved = glr_fork_get_context (fork);
  ASSERT_EQ (retrieved, ctx, "Retrieved context should match set context");

  glr_fork_destroy (fork);
  printf ("PASSED\n");
}

static void
test_fork_chain (void)
{
  print_test_name ("chain");

  glr_fork_t *fork1 = glr_fork_create (0);
  glr_fork_t *fork2 = glr_fork_create (1);

  fork1->next = fork2;

  ASSERT_EQ (glr_fork_next (fork1), fork2, "Next fork should be fork2");
  ASSERT_NULL (glr_fork_next (fork2), "Fork2 should not have next");

  glr_fork_destroy (fork1);
  printf ("PASSED\n");
}

int
main (void)
{
  printf ("=== LibGLR Fork Tests ===\n\n");

  test_fork_create_destroy ();
  test_fork_null ();
  test_fork_context ();
  test_fork_chain ();

  printf ("\n=== Results ===\n");
  printf ("Passed: %d\n", tests_passed);
  printf ("Failed: %d\n", tests_failed);

  return tests_failed > 0 ? 1 : 0;
}
