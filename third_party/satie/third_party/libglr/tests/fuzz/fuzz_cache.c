#include <glr/cache.h>
#include <glr/dependency.h>
#include <glr/forest.h>
#include <glr/diff.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __AFL_FUZZ_TESTCASE_LEN
__AFL_FUZZ_INIT();
#endif

#ifndef __AFL_LOOP
#define __AFL_LOOP(n) (0)
#endif

#define CACHE_DIR "/tmp/libglr_fuzz_cache"

static void
run_once (const unsigned char *buf, size_t len)
{
    glr_cache_config_t config;
    glr_cache_t *cache;
    uint8_t hash[32];
    glr_forest_cache_key_t key;
    glr_forest_t *forest;
    glr_forest_t *retrieved = NULL;

    if (len < 4)
      {
        return;
      }

    mkdir (CACHE_DIR, 0755);

    config = GLR_CACHE_DEFAULT_CONFIG;
    config.mdbx_path = CACHE_DIR;
    config.map_size = 10 * 1024 * 1024;

    cache = glr_cache_open (&config);
    if (!cache)
      {
        return;
      }

    glr_cache_compute_hash (buf, len, hash);

    memcpy (key.content_hash, hash, 32);
    key.grammar_crc = 0;
    key.start_symbol = 1;

    forest = glr_forest_create ();
    if (forest)
      {
        glr_forest_node_t *node = glr_forest_get_node (
            forest, GLR_NODE_TERMINAL, (int) (buf[0] % 8), 0);
        (void) node;
        glr_cache_store_forest (cache, &key, forest);
        glr_cache_lookup_forest (cache, &key, &retrieved);
        if (retrieved)
          {
            glr_forest_destroy (retrieved);
          }
        glr_forest_destroy (forest);
      }

    if (len >= 8)
      {
        uint32_t state;
        uint32_t pos;
        glr_gss_cache_key_t gkey;
        glr_stack_node_t *tostore;
        glr_stack_node_t *got = NULL;

        memcpy (&state, buf, sizeof (state));
        memcpy (&pos, buf + 4, sizeof (pos));

        tostore = glr_stack_node_create (state, pos);
        if (tostore != NULL)
          {
            gkey.state_id = state;
            gkey.position = pos;
            gkey.grammar_crc = 0;
            glr_cache_store_gss_node (cache, &gkey, tostore);
            glr_cache_lookup_gss_node (cache, &gkey, &got);
            if (got != NULL)
              {
                glr_stack_node_destroy_tree (got);
              }
            glr_stack_node_destroy (tostore);
          }
      }

    /* Exercise dependency + invalidation + diff on the same bytes. */
    {
      glr_edit_t edit;
      uint64_t packed = 0;
      glr_dependency_t *deps = NULL;
      size_t count = 0;
      memcpy (&packed, hash, sizeof (packed));
      (void) glr_compute_edit ((const char *) buf, len / 2,
                               (const char *) buf, len, &edit);
      (void) glr_dependency_add (cache, packed, GLR_CACHE_ENTRY_FOREST,
                                 0, (uint32_t) (len > 0 ? len - 1 : 0));
      (void) glr_dependency_get_affected (cache, 0,
                                          (uint32_t) (len > 0 ? len : 1),
                                          &deps, &count);
      glr_dependency_free_list (deps);
      (void) glr_cache_invalidate_range (cache, 0,
                                         (uint32_t) (len > 0 ? len : 1));
    }

    glr_cache_close (cache);
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
        static const unsigned char seed[] = "a + b * c hello";
        char cmd[256];
        snprintf (cmd, sizeof (cmd), "rm -rf %s", CACHE_DIR);
        (void) system (cmd);
        run_once (seed, sizeof (seed) - 1);
    }

    return 0;
#endif
}
