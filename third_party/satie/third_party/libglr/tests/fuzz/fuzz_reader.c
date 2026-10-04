/**
 * AFL Fuzzing harness for reader operations (real libglr API).
 */

#include <glr/reader.h>
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
  glr_reader_t *reader;
  glr_reader_token_t token;

  if (len < 2)
    {
      return;
    }

  reader = glr_reader_create ();
  if (!reader)
    {
      return;
    }

  memset (&token, 0, sizeof (token));
  glr_reader_set_encoding (reader, GLR_READER_ENCODING_UTF16_AUTO);
  if (glr_reader_set_input (reader, buffer, len) != 0)
    {
      glr_reader_destroy (reader);
      return;
    }
  glr_reader_reset (reader);

  for (size_t i = 0; i < 64; i++)
    {
      glr_reader_status_t st = glr_reader_next (reader, &token);
      if (st == GLR_READER_STATUS_EOF)
        {
          break;
        }
      if (st != GLR_READER_STATUS_OK)
        {
          break;
        }
    }

  glr_reader_token_clear (&token);
  (void) glr_reader_get_offset (reader);
  (void) glr_reader_remaining (reader);
  (void) glr_reader_at_eof (reader);
  (void) glr_reader_get_encoding (reader);
  glr_reader_destroy (reader);
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
    static const uint8_t seed[] = { 0x48, 0x00, 0x69, 0x00, 0x21, 0x00 };
    run_once (seed, sizeof (seed));
  }

  return 0;
}
