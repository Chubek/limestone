#ifndef GLR_TEST_COMMON_H
#define GLR_TEST_COMMON_H

#include <stdio.h>
#include <string.h>

typedef struct
{
  int passed;
  int failed;
} glr_test_stats_t;

#define GLR_TEST_INIT                                                            \
  glr_test_stats_t stats = { 0, 0 }

#define GLR_TEST_ASSERT(cond, msg)                                               \
  do                                                                             \
    {                                                                            \
      if (cond)                                                                  \
        {                                                                        \
          stats->passed++;                                                       \
        }                                                                        \
      else                                                                       \
        {                                                                        \
          printf ("FAILED: %s\n", msg);                                         \
          stats->failed++;                                                       \
        }                                                                        \
    }                                                                            \
  while (0)

#define GLR_TEST_ASSERT_EQ(a, b, msg) GLR_TEST_ASSERT ((a) == (b), msg)
#define GLR_TEST_ASSERT_NE(a, b, msg) GLR_TEST_ASSERT ((a) != (b), msg)
#define GLR_TEST_ASSERT_NULL(ptr, msg) GLR_TEST_ASSERT ((ptr) == NULL, msg)
#define GLR_TEST_ASSERT_NOT_NULL(ptr, msg) GLR_TEST_ASSERT ((ptr) != NULL, msg)

#define GLR_TEST_CASE(name) static void name (glr_test_stats_t *stats)

static inline void
glr_test_begin (const char *name)
{
  printf ("Testing: %s... ", name);
}

static inline void
glr_test_end (void)
{
  printf ("PASSED\n");
}

static inline int
glr_test_finish (const char *suite_name, glr_test_stats_t stats)
{
  printf ("\n=== %s Results ===\n", suite_name);
  printf ("Passed: %d\n", stats.passed);
  printf ("Failed: %d\n", stats.failed);
  return stats.failed > 0 ? 1 : 0;
}

static inline int
glr_test_string_eq (const char *lhs, const char *rhs)
{
  if (lhs == NULL || rhs == NULL)
    {
      return lhs == rhs;
    }

  return strcmp (lhs, rhs) == 0;
}

#endif /* GLR_TEST_COMMON_H */
