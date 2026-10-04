// src/glr/parser-incr.c
#include <glr/parser.h>
#include <glr/cache.h>
#include <glr/dependency.h>
#include <glr/diff.h>
#include <glr/forest-merge.h>
#include <glr/scannerless.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_LMDB

void glr_parser_set_cache(glr_parser_t* parser, struct glr_cache_t* cache) {
    if (!parser) return;
    parser->cache = cache;
}

struct glr_cache_t* glr_parser_get_cache(const glr_parser_t* parser) {
    if (!parser) return NULL;
    return parser->cache;
}

#endif /* HAVE_LMDB */

int glr_parser_parse_incremental(glr_parser_t* parser,
                                  const glr_forest_t* old_forest,
                                  const char* old_content,
                                  size_t old_len,
                                  const char* new_content,
                                  size_t new_len,
                                  size_t edit_start,
                                  size_t edit_end,
                                  glr_forest_t** out_forest) {
    if (!parser || !new_content || !out_forest) return -1;

    *out_forest = NULL;

    /* Pattern extents and lexical alternatives may cross the edit boundary.
       Semantic actions must evaluate a complete accepted derivation, rather
       than a changed fragment. Use the full driver for those grammars. */
    if (!old_content || !old_forest || parser->scannerless
        || glr_scannerless_has_patterns(parser->grammar)
        || glr_grammar_has_semantic_actions(parser->grammar)) {
        glr_parse_result_t result = glr_parse(parser, new_content, new_len);
        if (result.error != GLR_PARSE_SUCCESS) {
            return -1;
        }
        *out_forest = glr_forest_clone (result.forest);
        if (!*out_forest) return -1;
        return 0;
    }

    /* Compute edit if not provided */
    glr_edit_t edit;
    memset (&edit, 0, sizeof (edit));
    if (edit_start == 0 && edit_end == 0) {
        if (glr_compute_edit(old_content, old_len, new_content, new_len, &edit) < 0) {
            return -1;
        }
        edit_start = edit.old_start;
        edit_end = edit.old_end;
    } else {
        /* Use provided edit bounds, clamped to the old content. */
        if (edit_start > old_len) edit_start = old_len;
        if (edit_end > old_len) edit_end = old_len;
        if (edit_start > edit_end) edit_end = edit_start;
        edit.old_start = edit_start;
        edit.old_end = edit_end;
        {
          size_t removed = edit_end - edit_start;
          size_t inserted = (new_len > old_len - removed)
                                ? new_len - (old_len - removed)
                                : 0;
          edit.new_start = edit_start;
          edit.new_end = edit_start + inserted;
          if (edit.new_end > new_len) edit.new_end = new_len;
        }
    }

    /* Check if edit is empty: byte-identical content. */
    if (edit_start == edit_end && old_len == new_len
        && memcmp (old_content, new_content, old_len) == 0) {
        /* No change - return an independent clone, never an alias. */
        *out_forest = glr_forest_clone (old_forest);
        return *out_forest ? 0 : -1;
    }

#ifdef HAVE_LMDB
    glr_cache_t* cache = parser->cache;
#else
    (void) parser;
#endif

    /* Parse the changed region in isolation. */
    glr_forest_t* middle_forest = NULL;
    size_t changed_start = edit.new_start;
    size_t changed_len = (edit.new_end > edit.new_start)
                             ? edit.new_end - edit.new_start
                             : 0;

    if (changed_len > 0 && changed_start < new_len) {
        size_t clip = new_len - changed_start;
        char* changed_region;
        if (changed_len > clip) changed_len = clip;
        changed_region = malloc(changed_len + 1);
        if (!changed_region) return -1;

        memcpy(changed_region, new_content + changed_start, changed_len);
        changed_region[changed_len] = '\0';

        {
          glr_parse_result_t result = glr_parse(parser, changed_region, changed_len);
          free(changed_region);

          if (result.error == GLR_PARSE_SUCCESS && result.forest) {
              middle_forest = glr_forest_clone (result.forest);
          }
          /* On fragment-parse failure middle stays NULL and the merge
             below degrades to prefix+suffix; the fallback after the
             merge guarantees a usable forest. */
        }
    }

    /* Merge: reuse the old forest's prefix/suffix structure by cloning
       the old forest as the merge base. The old forest is never aliased:
       glr_forest_merge deep-copies its inputs. */
    {
      glr_forest_t *merged = NULL;
      int rc = glr_forest_merge(parser, old_forest, middle_forest, NULL,
                                &merged);
      if (middle_forest) glr_forest_destroy(middle_forest);

      if (rc != 0 || !merged) {
          /* Fall back to a full parse so callers always get a forest. */
          glr_parse_result_t result = glr_parse(parser, new_content, new_len);
          if (result.error != GLR_PARSE_SUCCESS) return -1;
          *out_forest = glr_forest_clone (result.forest);
          return *out_forest ? 0 : -1;
      }

#ifdef HAVE_LMDB
      /* Store result in cache and record its source-range dependency so
         later glr_cache_invalidate_range() calls can evict it. */
      if (cache && merged) {
          glr_forest_cache_key_t key;
          uint8_t full_hash[32];
          glr_cache_compute_hash((const uint8_t*)new_content, new_len, full_hash);
          memcpy(key.content_hash, full_hash, 32);
          key.grammar_crc = 0;
          key.start_symbol = 0;

          if (glr_cache_store_forest(cache, &key, merged) == 0 && new_len > 0) {
              uint64_t packed = 0;
              memcpy(&packed, full_hash, sizeof (packed) > 8 ? 8 : sizeof (packed));
              /* Best effort: ignore dependency errors. */
              (void) glr_dependency_add (cache, packed,
                                         GLR_CACHE_ENTRY_FOREST, 0,
                                         (uint32_t) new_len);
          }
      }
#endif
      *out_forest = merged;
      return 0;
    }
}

#ifdef HAVE_LMDB

int glr_parser_enable_incremental(glr_parser_t* parser, const char* cache_path) {
    glr_cache_config_t config;
    glr_cache_t* cache;

    if (!parser || !cache_path) return -1;

    /* Close any previous cache to avoid leaking the old handle. */
    if (parser->cache) {
        glr_cache_close (parser->cache);
        parser->cache = NULL;
    }

    config = GLR_CACHE_DEFAULT_CONFIG;
    config.mdbx_path = cache_path;

    cache = glr_cache_open(&config);
    if (!cache) return -1;

    glr_parser_set_cache(parser, cache);
    return 0;
}

void glr_parser_disable_incremental(glr_parser_t* parser) {
    if (!parser) return;

    if (parser->cache) {
        glr_cache_close(parser->cache);
        parser->cache = NULL;
    }
}

int glr_parser_get_cache_stats(glr_parser_t* parser, struct glr_cache_stats_t* stats) {
    if (!parser || !stats) return -1;

    if (!parser->cache) return -1;

    return glr_cache_get_stats(parser->cache, (glr_cache_stats_t*)stats);
}

#endif /* HAVE_LMDB */
