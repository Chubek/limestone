#include <glr/reduction.h>
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
test_item_set_create_destroy (void)
{
  print_test_name ("create_destroy");

  glr_item_set_t *set = glr_item_set_create ();
  ASSERT_NOT_NULL (set, "Failed to create item set");

  glr_item_set_destroy (set);
  printf ("PASSED\n");
}

static void
test_item_set_add (void)
{
  print_test_name ("add");

  glr_item_set_t *set = glr_item_set_create ();
  ASSERT_NOT_NULL (set, "Failed to create item set");

  glr_item_t item1 = { 0, 0, 0 };
  glr_item_t item2 = { 0, 1, 0 };

  int ret = glr_item_set_add (set, item1);
  ASSERT_EQ (ret, 0, "Should successfully add item");

  ret = glr_item_set_add (set, item2);
  ASSERT_EQ (ret, 0, "Should successfully add different item");

  ASSERT_EQ (set->item_count, 2, "Should have 2 items");

  glr_item_set_destroy (set);
  printf ("PASSED\n");
}

static void
test_item_set_contains (void)
{
  print_test_name ("contains");

  glr_item_set_t *set = glr_item_set_create ();
  ASSERT_NOT_NULL (set, "Failed to create item set");

  glr_item_t item = { 5, 3, 10 };
  glr_item_set_add (set, item);

  ASSERT (glr_item_set_contains (set, item), "Set should contain item");

  glr_item_t different = { 5, 4, 10 };
  ASSERT (!glr_item_set_contains (set, different),
          "Set should not contain different item");

  glr_item_set_destroy (set);
  printf ("PASSED\n");
}

static void
test_item_set_duplicate (void)
{
  print_test_name ("duplicate");

  glr_item_set_t *set = glr_item_set_create ();
  ASSERT_NOT_NULL (set, "Failed to create item set");

  glr_item_t item = { 1, 2, 3 };

  int ret1 = glr_item_set_add (set, item);
  int ret2 = glr_item_set_add (set, item);

  ASSERT_EQ (ret1, 0, "First add should succeed");
  ASSERT_EQ (ret2, 0, "Second add should return 0 (duplicate)");
  ASSERT_EQ (set->item_count, 1, "Should have only 1 unique item");

  glr_item_set_destroy (set);
  printf ("PASSED\n");
}

static void
test_reduction_create_destroy (void)
{
  print_test_name ("create_destroy");

  glr_reduction_t *red = glr_reduction_create (5, 10);
  ASSERT_NOT_NULL (red, "Failed to create reduction");

  ASSERT_EQ (red->production_id, 5, "Production ID should be 5");
  ASSERT_EQ (red->position, 10, "Position should be 10");

  glr_reduction_destroy (red);
  printf ("PASSED\n");
}

static void
test_reduction_helpers (void)
{
  print_test_name ("helpers");

  glr_reduction_t *red = glr_reduction_create (42, 100);
  ASSERT_NOT_NULL (red, "Failed to create reduction");

  ASSERT_EQ (glr_reduction_get_production (red), 42,
             "Should get production ID");

  glr_reduction_destroy (red);
  printf ("PASSED\n");
}

static void
test_item_set_null (void)
{
  print_test_name ("null");

  glr_item_t item = { 0, 0, 0 };
  ASSERT (!glr_item_set_contains (NULL, item), "NULL should not contain");
  printf ("PASSED\n");
}

static void
test_item_helpers (void)
{
  print_test_name ("item_helpers");

  glr_item_t item = { 5, 10, 20 };

  ASSERT_EQ (glr_item_get_production (&item), 5, "Should get production ID");
  ASSERT (glr_item_is_complete (&item),
          "Item with dot > 0 should be complete");

  glr_item_t complete = { 5, 1, 0 };
  ASSERT (glr_item_is_complete (&complete), "Complete item");

  printf ("PASSED\n");
}

int
main (void)
{
  printf ("=== LibGLR Reduction Tests ===\n\n");

  test_item_set_create_destroy ();
  test_item_set_add ();
  test_item_set_contains ();
  test_item_set_duplicate ();
  test_reduction_create_destroy ();
  test_reduction_helpers ();
  test_item_set_null ();
  test_item_helpers ();

  printf ("\n=== Results ===\n");
  printf ("Passed: %d\n", tests_passed);
  printf ("Failed: %d\n", tests_failed);

  return tests_failed > 0 ? 1 : 0;
}
