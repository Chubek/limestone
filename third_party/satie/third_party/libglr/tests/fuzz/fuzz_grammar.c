/**
 * AFL Fuzzing harness for grammar operations (real libglr API).
 */

#include <glr/grammar.h>
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
  glr_grammar_t *grammar;
  size_t pos = 0;

  if (len == 0)
    {
      return;
    }

  grammar = glr_grammar_create ();
  if (!grammar)
    {
      return;
    }

  while (pos + 2 < len)
    {
      uint8_t op = buffer[pos++];
      uint8_t name_len = buffer[pos++];

      if (pos + name_len > len)
        {
          break;
        }

      {
        char name[256];
        size_t copy_len = name_len < 255 ? name_len : 255;
        memcpy (name, buffer + pos, copy_len);
        name[copy_len] = '\0';
        pos += name_len;

        if (name[0] == '\0')
          {
            continue;
          }

        switch (op % 5)
          {
          case 0:
            glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, name);
            break;
          case 1:
            glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, name);
            break;
          case 2:
            {
              int head = (int) (buffer[pos % (len ? len : 1)] % 8);
              glr_symbol_t *body[4];
              size_t n = 0;
              while (n < 4 && pos < len)
                {
                  int id = buffer[pos++] % 8;
                  glr_symbol_t *s = glr_grammar_get_symbol (grammar, id);
                  if (s == NULL)
                    {
                      break;
                    }
                  body[n++] = s;
                }
              if (n > 0)
                {
                  glr_grammar_add_production (grammar, head, body, n);
                }
              break;
            }
          case 3:
            glr_grammar_find_symbol_any (grammar, name);
            break;
          case 4:
            {
              char err[128];
              glr_grammar_validate (grammar, err, sizeof (err));
              break;
            }
          }
      }
    }

  glr_grammar_destroy (grammar);
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

  /* Non-AFL smoke run so the target always exercises the API. */
  {
    const uint8_t seed[] = "ExprNUMPLUSabc";
    run_once (seed, sizeof (seed) - 1);
  }

  return 0;
}
