#include <glr/dependency.h>
#include <glr/cache.h>
#include <stdlib.h>
#include <string.h>

/* In-memory dependency registry keyed by cache handle.
 *
 * Each cache owns an append-only list of dependency records describing
 * which packed cache key depends on which source byte range. Range
 * queries are linear scans; this is intentional: dependency lists are
 * small (one entry per cached subtree) and the scan keeps the
 * implementation free of extra third-party containers. When a libmdbx
 * backing store is present the registry mirrors the authoritative
 * in-memory state; persistence of dependency records themselves is left
 * to future work without breaking the API contract.
 */

typedef struct dep_entry {
    uint64_t cache_key;
    uint32_t entry_type;
    uint32_t start;
    uint32_t end;
} dep_entry_t;

typedef struct dep_store {
    const glr_cache_t *cache;
    dep_entry_t *entries;
    size_t count;
    size_t capacity;
    struct dep_store *next;
} dep_store_t;

static dep_store_t *g_stores = NULL;

static dep_store_t *
dep_store_for (const glr_cache_t *cache, int create)
{
  dep_store_t *s;
  for (s = g_stores; s != NULL; s = s->next)
    {
      if (s->cache == cache)
        {
          return s;
        }
    }
  if (!create)
    {
      return NULL;
    }
  s = calloc (1, sizeof (*s));
  if (s == NULL)
    {
      return NULL;
    }
  s->cache = cache;
  s->next = g_stores;
  g_stores = s;
  return s;
}

static int
dep_ranges_overlap (uint32_t a_start, uint32_t a_end, uint32_t b_start,
                    uint32_t b_end)
{
  uint32_t lo = a_start > b_start ? a_start : b_start;
  uint32_t hi = a_end < b_end ? a_end : b_end;
  return lo < hi;
}

int glr_dependency_add(glr_cache_t* cache, uint64_t cache_key,
                       uint32_t entry_type, uint32_t start, uint32_t end) {
    dep_store_t *store;
    dep_entry_t *grown;

    if (!cache) return -1;
    if (start >= end) return -1;

    store = dep_store_for (cache, 1);
    if (!store) return -1;

    /* Entry type 0 is accepted as a legacy alias for FOREST so older
       callers (and the existing comprehensive suite) keep working;
       values above SUBTREE are rejected as invalid. */
    if (entry_type > GLR_CACHE_ENTRY_SUBTREE) {
        return -1;
    }
    if (entry_type == 0) entry_type = GLR_CACHE_ENTRY_FOREST;
    for (size_t i = 0; i < store->count; i++)
      {
        dep_entry_t *e = &store->entries[i];
        if (e->cache_key == cache_key && e->entry_type == entry_type
            && e->start == start && e->end == end)
          {
            return 0;
          }
      }

    if (store->count >= store->capacity)
      {
        size_t new_cap = store->capacity == 0 ? 16 : store->capacity * 2;
        grown = realloc (store->entries, new_cap * sizeof (*grown));
        if (!grown) return -1;
        store->entries = grown;
        store->capacity = new_cap;
      }

    store->entries[store->count].cache_key = cache_key;
    store->entries[store->count].entry_type = entry_type;
    store->entries[store->count].start = start;
    store->entries[store->count].end = end;
    store->count++;
    return 0;
}

int glr_dependency_invalidate_range(glr_cache_t* cache,
                                    uint32_t start, uint32_t end) {
    dep_store_t *store;

    if (!cache) return -1;
    if (start >= end) return -1;

    store = dep_store_for (cache, 0);
    if (!store) return 0;

    /* Compact out every record overlapping [start, end). */
    {
      size_t w = 0;
      for (size_t r = 0; r < store->count; r++)
        {
          if (dep_ranges_overlap (store->entries[r].start,
                                  store->entries[r].end, start, end))
            {
              continue;
            }
          if (w != r)
            {
              store->entries[w] = store->entries[r];
            }
          w++;
        }
      store->count = w;
    }
    return 0;
}

int glr_dependency_get_affected(glr_cache_t* cache,
                                uint32_t start, uint32_t end,
                                glr_dependency_t** out_deps,
                                size_t* out_count) {
    dep_store_t *store;
    size_t hits = 0;
    glr_dependency_t *result;

    if (!cache || !out_deps || !out_count) return -1;
    if (start >= end) return -1;

    *out_deps = NULL;
    *out_count = 0;

    store = dep_store_for (cache, 0);
    if (!store) return 0;

    for (size_t i = 0; i < store->count; i++)
      {
        if (dep_ranges_overlap (store->entries[i].start,
                                store->entries[i].end, start, end))
          {
            hits++;
          }
      }

    if (hits == 0) return 0;

    result = calloc (hits, sizeof (*result));
    if (!result) return -1;

    {
      size_t w = 0;
      for (size_t i = 0; i < store->count; i++)
        {
          if (!dep_ranges_overlap (store->entries[i].start,
                                   store->entries[i].end, start, end))
            {
              continue;
            }
          result[w].start_byte = store->entries[i].start;
          result[w].end_byte = store->entries[i].end;
          result[w].cache_key = store->entries[i].cache_key;
          result[w].entry_type = store->entries[i].entry_type;
          w++;
        }
    }

    *out_deps = result;
    *out_count = hits;
    return 0;
}

void glr_dependency_free_list(glr_dependency_t* deps) {
    free(deps);
}

size_t glr_dependency_count(const glr_cache_t* cache)
{
  dep_store_t *store;
  if (cache == NULL)
    {
      return 0;
    }
  store = dep_store_for (cache, 0);
  return store != NULL ? store->count : 0;
}

/* Internal cleanup hook: drop the whole registry entry for a cache that
 * is being closed. Called from glr_cache_close() so long-lived processes
 * do not accumulate one store per cache lifetime. */
void glr_dependency_drop_cache (glr_cache_t *cache)
{
  dep_store_t **slot;
  if (cache == NULL)
    {
      return;
    }
  for (slot = &g_stores; *slot != NULL; slot = &(*slot)->next)
    {
      if ((*slot)->cache == cache)
        {
          dep_store_t *victim = *slot;
          *slot = victim->next;
          free (victim->entries);
          free (victim);
          return;
        }
    }
}
