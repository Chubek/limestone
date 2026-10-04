#include <glr/diff.h>
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
    size_t split;
    const char *old_text;
    size_t old_len;
    const char *new_text;
    size_t new_len;
    glr_edit_t edit;

    if (len < 2)
      {
        return;
      }

    split = len / 2;

    old_text = (const char *) buf;
    old_len = split;

    new_text = (const char *) (buf + split);
    new_len = len - split;

    memset (&edit, 0, sizeof (edit));
    if (glr_compute_edit (old_text, old_len, new_text, new_len, &edit) == 0)
      {
        (void) glr_edit_is_empty (&edit);
        (void) glr_edit_old_length (&edit);
        (void) glr_edit_new_length (&edit);
      }

    (void) glr_find_common_prefix (old_text, new_text, old_len, new_len);
    {
      size_t prefix
          = glr_find_common_prefix (old_text, new_text, old_len, new_len);
      (void) glr_find_common_suffix (old_text, new_text, old_len, new_len,
                                     prefix);
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
        static const unsigned char seed[] = "old\nnew\nsplit here";
        run_once (seed, sizeof (seed) - 1);
    }

    return 0;
#endif
}
