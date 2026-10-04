/**
 * AFL Fuzzing harness for stack operations (real libglr API).
 */

#include <glr/stack.h>
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
  glr_stack_t *stack;
  glr_stack_node_t *nodes[64] = { NULL };
  size_t node_count = 0;
  size_t pos = 0;

  if (len < 4)
    {
      return;
    }

  stack = glr_stack_create ();
  if (!stack)
    {
      return;
    }

  while (pos + 3 < len)
    {
      uint8_t op = buffer[pos++];
      uint32_t state = buffer[pos++];
      uint32_t position = (uint32_t) ((buffer[pos] << 8) | buffer[pos + 1]);
      pos += 2;

      switch (op % 6)
        {
        case 0:
          glr_stack_push (stack, (void *) (uintptr_t) state);
          break;
        case 1:
          glr_stack_pop (stack);
          break;
        case 2:
          glr_stack_peek (stack);
          glr_stack_height (stack);
          glr_stack_is_empty (stack);
          glr_stack_get_node_count (stack);
          break;
        case 3:
          if (node_count < 64)
            {
              glr_stack_node_t *parent
                  = node_count > 0 ? nodes[(state + position) % node_count]
                                   : NULL;
              glr_stack_node_t *node
                  = glr_stack_node_create (state, position);
              if (node != NULL)
                {
                  if (parent != NULL)
                    {
                      (void) glr_stack_node_add_parent (node, parent);
                    }
                  glr_stack_node_get_state (node);
                  glr_stack_node_get_position (node);
                  glr_stack_node_get_parent_count (node);
                  nodes[node_count++] = node;
                }
            }
          break;
        case 4:
          {
            glr_stack_t *fork = glr_stack_fork (stack, 0);
            if (fork != NULL)
              {
                glr_stack_destroy (fork);
              }
            break;
          }
        case 5:
          glr_stack_reset (stack);
          break;
        }
    }

  for (size_t i = 0; i < node_count; i++)
    {
      glr_stack_node_destroy (nodes[i]);
    }
  glr_stack_destroy (stack);
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
    const uint8_t seed[] = "0123456789abcdef";
    run_once (seed, sizeof (seed) - 1);
  }

  return 0;
}
