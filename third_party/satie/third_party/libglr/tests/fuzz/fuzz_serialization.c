#include <glr/serialization.h>
#include <glr/forest.h>
#include <glr/stack.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __AFL_FUZZ_TESTCASE_LEN
__AFL_FUZZ_INIT();
#endif

#ifndef __AFL_LOOP
#define __AFL_LOOP(n) (0)
#endif

static void
run_once (const unsigned char *buf, size_t len)
{
    glr_forest_t *forest = NULL;
    uint8_t *serialized = NULL;
    size_t out_size = 0;

    if (len < 8)
      {
        return;
      }

    /* Build a small forest, round-trip it, then try parsing fuzz bytes
       as a foreign payload (must fail cleanly, never crash). */
    forest = glr_forest_create ();
    if (forest != NULL)
      {
        glr_forest_node_t *a
            = glr_forest_get_node (forest, GLR_NODE_TERMINAL,
                                   (int) (buf[0] % 8), 0);
        glr_forest_node_t *b = glr_forest_get_node (
            forest, GLR_NODE_NONTERMINAL, (int) (buf[1] % 8), 1);
        if (a != NULL && b != NULL)
          {
            (void) glr_forest_add_child (b, a);
          }
        if (glr_serialize_forest (forest, &serialized, &out_size) == 0)
          {
            glr_forest_t *back = NULL;
            if (glr_deserialize_forest (serialized, out_size, &back) == 0)
              {
                glr_forest_destroy (back);
              }
            free (serialized);
          }
        glr_forest_destroy (forest);
      }

    {
      glr_forest_t *foreign = NULL;
      (void) glr_deserialize_forest (buf, len, &foreign);
      if (foreign != NULL)
        {
          glr_forest_destroy (foreign);
        }
    }

    {
      glr_stack_node_t *node
          = glr_stack_node_create ((uint32_t) buf[0], (uint32_t) buf[1]);
      if (node != NULL)
        {
          uint8_t *gdata = NULL;
          size_t glen = 0;
          if (glr_serialize_stack_node (node, &gdata, &glen) == 0)
            {
              glr_stack_node_t *back = NULL;
              if (glr_deserialize_stack_node (gdata, glen, &back) == 0)
                {
                  glr_stack_node_destroy_tree (back);
                }
              free (gdata);
            }
          glr_stack_node_destroy (node);
        }
    }

    {
      glr_stack_node_t *foreign = NULL;
      (void) glr_deserialize_stack_node (buf, len, &foreign);
      if (foreign != NULL)
        {
          glr_stack_node_destroy_tree (foreign);
        }
    }
}

int main(int argc, char** argv) {
#ifdef __AFL_FUZZ_TESTCASE_LEN
    __AFL_INIT();
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
    while (__AFL_LOOP(10000)) {
        int len = __AFL_FUZZ_TESTCASE_LEN;
        run_once (buf, (size_t) len);
    }
    return 0;
#else
    unsigned char *buf = NULL;
    size_t len = 0;

    if (argc > 1) {
        FILE* f = fopen(argv[1], "rb");
        long tell;
        if (!f) return 1;

        fseek(f, 0, SEEK_END);
        tell = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (tell < 0) tell = 0;
        len = (size_t) tell;

        buf = malloc(len > 0 ? len : 1);
        if (!buf) {
            fclose(f);
            return 1;
        }

        len = fread(buf, 1, len, f);
        fclose(f);
        run_once (buf, len);
        free(buf);
        return 0;
    }

    {
        static const unsigned char seed[] = "binary data seed 1234";
        run_once (seed, sizeof (seed) - 1);
    }

    return 0;
#endif
}
