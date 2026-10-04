/**
 * AFL Fuzzing harness for forest operations (real libglr API).
 */

#include <glr/forest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifndef __AFL_LOOP
#define __AFL_LOOP(n) (0)
#endif

#define MAX_INPUT_SIZE 4096

static void
run_once (const uint8_t *buffer, size_t len)
{
  glr_forest_t *forest;
  size_t pos = 0;

  if (len < 4)
    {
      return;
    }

  forest = glr_forest_create ();
  if (!forest)
    {
      return;
    }

  while (pos + 4 < len)
    {
      uint8_t op = buffer[pos++];
      int symbol_id = (int) (buffer[pos++] % 16);
      uint32_t at = (uint32_t) ((buffer[pos] << 8) | buffer[pos + 1]);
      pos += 2;

      switch (op % 5)
        {
        case 0:
        case 1:
          {
            glr_forest_node_type_t type
                = (op % 2 == 0) ? GLR_NODE_TERMINAL : GLR_NODE_NONTERMINAL;
            glr_forest_node_t *node
                = glr_forest_get_node (forest, type, symbol_id,
                                       (size_t) (at % 64));
            if (node != NULL && type == GLR_NODE_NONTERMINAL && pos < len)
              {
                glr_forest_node_t *child = glr_forest_get_node (
                    forest, GLR_NODE_TERMINAL, (int) (buffer[pos++] % 16),
                    (size_t) (at % 64));
                if (child != NULL)
                  {
                    (void) glr_forest_add_child (node, child);
                  }
              }
            break;
          }
        case 2:
          {
            glr_forest_edge_t edge;
            memset (&edge, 0, sizeof (edge));
            edge.nonterminal_id = symbol_id;
            edge.start_position = (size_t) (at % 64);
            edge.end_position = edge.start_position + (buffer[pos++] % 8);
            (void) glr_forest_add_edge (forest, &edge);
            break;
          }
        case 3:
          {
            glr_forest_t *clone = glr_forest_clone (forest);
            if (clone != NULL)
              {
                (void) glr_forest_total_nodes (clone);
                glr_forest_destroy (clone);
              }
            break;
          }
        case 4:
          (void) glr_forest_node_count_at (forest, (size_t) (at % 64));
          glr_forest_get_edges (forest, (size_t) (at % 64));
          break;
        }
    }

  glr_forest_destroy (forest);
}

int
main (int argc, char **argv)
{
  uint8_t buffer[MAX_INPUT_SIZE];
  size_t len = 0;

  while (__AFL_LOOP (1000))
    {
      len = fread (buffer, 1, MAX_INPUT_SIZE, stdin);
      if (len == 0)
        {
          continue;
        }
      run_once (buffer, len);
    }

  if (argc > 1)
    {
      FILE *f = fopen (argv[1], "rb");
      if (!f)
        {
          return 1;
        }
      len = fread (buffer, 1, MAX_INPUT_SIZE, f);
      fclose (f);
      run_once (buffer, len);
      return 0;
    }

  {
    const uint8_t seed[] = "hello world fuzz seed";
    run_once (seed, sizeof (seed) - 1);
  }

  return 0;
}
