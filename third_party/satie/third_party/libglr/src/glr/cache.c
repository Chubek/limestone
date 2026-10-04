// src/glr/cache.c
#include <glr/cache.h>
#include <glr/dependency.h>
#include <glr/forest.h>
#include <glr/stack.h>
#include <glr/grammar.h>
#include <glr/serialization.h>

#include <mdbx.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <openssl/evp.h>

/* Internal cache structure */
struct glr_cache_t {
    MDBX_env* env;
    MDBX_dbi dbi_forest;
    MDBX_dbi dbi_gss;
    MDBX_dbi dbi_subtree;
    MDBX_dbi dbi_metadata;
    glr_cache_config_t config;
    glr_cache_stats_t stats;
    uint32_t grammar_crc;
};

const glr_cache_config_t GLR_CACHE_DEFAULT_CONFIG = {
    .mdbx_path = ".",
    .map_size = 1024 * 1024 * 1024,  /* 1GB */
    .max_readers = 126,
    .use_async = false,
    .ttl_seconds = 86400              /* 1 day */
};

/* Helper: Convert time_t to 64-bit for storage */
static uint64_t time_to_u64(time_t t) {
    return (uint64_t)t;
}

glr_cache_t* glr_cache_open(const glr_cache_config_t* config) {
    glr_cache_t* cache = calloc(1, sizeof(glr_cache_t));
    if (!cache) return NULL;
    
    memcpy(&cache->config, config, sizeof(glr_cache_config_t));
    memset(&cache->stats, 0, sizeof(glr_cache_stats_t));
    
    /* Create libmdbx environment */
    int rc = mdbx_env_create(&cache->env);
    if (rc != 0) {
        fprintf(stderr, "mdbx_env_create: %s\n", mdbx_strerror(rc));
        free(cache);
        return NULL;
    }
    
    /* mdbx_env_set_mapsize() is the only entry point available across the
       libmdbx releases this library supports, so silence the upstream
       deprecation marker locally. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    rc = mdbx_env_set_mapsize(cache->env, cache->config.map_size);
#pragma GCC diagnostic pop
    if (rc != 0) {
        fprintf(stderr, "mdbx_env_set_mapsize: %s\n", mdbx_strerror(rc));
        mdbx_env_close(cache->env);
        free(cache);
        return NULL;
    }
    
    rc = mdbx_env_set_maxreaders(cache->env, cache->config.max_readers);
    if (rc != 0) {
        fprintf(stderr, "mdbx_env_set_maxreaders: %s\n", mdbx_strerror(rc));
        mdbx_env_close(cache->env);
        free(cache);
        return NULL;
    }
    
    rc = mdbx_env_set_maxdbs(cache->env, 4);
    if (rc != 0) {
        fprintf(stderr, "mdbx_env_set_maxdbs: %s\n", mdbx_strerror(rc));
        mdbx_env_close(cache->env);
        free(cache);
        return NULL;
    }
    
    unsigned int env_flags = 0;
    if (!cache->config.use_async) {
        env_flags |= MDBX_MAPASYNC;
    }
    
    rc = mdbx_env_open(cache->env, cache->config.mdbx_path, env_flags, 0664);
    if (rc != 0) {
        fprintf(stderr, "mdbx_env_open: %s\n", mdbx_strerror(rc));
        mdbx_env_close(cache->env);
        free(cache);
        return NULL;
    }
    
    /* Open databases */
    MDBX_txn* txn;
    rc = mdbx_txn_begin(cache->env, NULL, 0, &txn);
    if (rc != 0) {
        fprintf(stderr, "mdbx_txn_begin: %s\n", mdbx_strerror(rc));
        mdbx_env_close(cache->env);
        free(cache);
        return NULL;
    }
    
    rc = mdbx_dbi_open(txn, "forests", MDBX_CREATE, &cache->dbi_forest);
    if (rc != 0) goto cleanup;
    
    rc = mdbx_dbi_open(txn, "gss_nodes", MDBX_CREATE, &cache->dbi_gss);
    if (rc != 0) goto cleanup;
    
    rc = mdbx_dbi_open(txn, "subtrees", MDBX_CREATE, &cache->dbi_subtree);
    if (rc != 0) goto cleanup;
    
    rc = mdbx_dbi_open(txn, "metadata", MDBX_CREATE, &cache->dbi_metadata);
    if (rc != 0) goto cleanup;
    
    mdbx_txn_commit(txn);
    
    return cache;
    
cleanup:
    mdbx_txn_abort(txn);
    mdbx_env_close(cache->env);
    free(cache);
    return NULL;
}

void glr_cache_close(glr_cache_t* cache) {
    extern void glr_dependency_drop_cache(glr_cache_t *cache);
    if (!cache) return;

    glr_dependency_drop_cache(cache);

    if (cache->env) {
        mdbx_env_close(cache->env);
    }

    free(cache);
}

int glr_cache_sync(glr_cache_t* cache) {
    if (!cache) return -1;
    return mdbx_env_sync(cache->env);
}

void glr_cache_compute_hash(const uint8_t* data, size_t len, uint8_t hash[32]) {
    unsigned int out_len = 0;
    EVP_MD_CTX *ctx;

    /* Use the EVP interface: the low-level SHA256_* helpers are
       deprecated in OpenSSL 3 and removed in later releases. */
    if (!data && len > 0) {
        memset(hash, 0, 32);
        return;
    }

    ctx = EVP_MD_CTX_new();
    if (!ctx) {
        memset(hash, 0, 32);
        return;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1
        || EVP_DigestUpdate(ctx, data, len) != 1
        || EVP_DigestFinal_ex(ctx, hash, &out_len) != 1
        || out_len != 32) {
        memset(hash, 0, 32);
    }

    EVP_MD_CTX_free(ctx);
}

/* Helper: Pack a cache key into a single integer for indexing */
static uint64_t pack_forest_key(const glr_forest_cache_key_t* key) {
    uint64_t packed = 0;
    /* Use first 8 bytes of hash + grammar_crc + start_symbol */
    memcpy(&packed, key->content_hash, 8);
    packed ^= ((uint64_t)key->grammar_crc << 32);
    packed ^= ((uint64_t)key->start_symbol << 48);
    return packed;
}

static uint64_t pack_gss_key(const glr_gss_cache_key_t* key) {
    return ((uint64_t)key->state_id << 32) | 
           ((uint64_t)key->position << 0) |
           ((uint64_t)key->grammar_crc << 48);
}

static uint64_t pack_subtree_key(const glr_subtree_cache_key_t* key) {
    uint64_t packed = 0;
    memcpy(&packed, key->content_hash, 8);
    packed ^= ((uint64_t)key->production_id << 32);
    packed ^= ((uint64_t)key->grammar_crc << 48);
    return packed;
}

int glr_cache_lookup_forest(glr_cache_t* cache,
                            const glr_forest_cache_key_t* key,
                            glr_forest_t** out_forest) {
    if (!cache || !key || !out_forest) return -1;
    
    uint64_t packed_key = pack_forest_key(key);
    
    MDBX_txn* txn;
    MDBX_val mdb_key, mdb_data;
    
    int rc = mdbx_txn_begin(cache->env, NULL, MDBX_TXN_RDONLY, &txn);
    if (rc != 0) return -1;
    
    mdb_key.iov_base = &packed_key;
    mdb_key.iov_len = sizeof(packed_key);
    
    rc = mdbx_get(txn, cache->dbi_forest, &mdb_key, &mdb_data);
    if (rc == MDBX_NOTFOUND) {
        mdbx_txn_abort(txn);
        cache->stats.forest_misses++;
        return 0;
    } else if (rc != 0) {
        mdbx_txn_abort(txn);
        return -1;
    }
    
    /* Deserialize forest */
    rc = glr_deserialize_forest((const uint8_t*)mdb_data.iov_base, 
                                mdb_data.iov_len, out_forest);
    
    mdbx_txn_abort(txn);
    
    if (rc == 0) {
        cache->stats.forest_hits++;
        return 1;
    } else {
        cache->stats.forest_misses++;
        return -1;
    }
}

int glr_cache_store_forest(glr_cache_t* cache,
                           const glr_forest_cache_key_t* key,
                           const glr_forest_t* forest) {
    if (!cache || !key || !forest) return -1;
    
    uint64_t packed_key = pack_forest_key(key);
    
    /* Serialize forest */
    uint8_t* data;
    size_t len;
    if (glr_serialize_forest(forest, &data, &len) < 0) {
        return -1;
    }
    
    MDBX_txn* txn;
    MDBX_val mdb_key, mdb_data;
    
    int rc = mdbx_txn_begin(cache->env, NULL, 0, &txn);
    if (rc != 0) {
        free(data);
        return -1;
    }
    
    mdb_key.iov_base = &packed_key;
    mdb_key.iov_len = sizeof(packed_key);
    mdb_data.iov_base = data;
    mdb_data.iov_len = len;
    
    rc = mdbx_put(txn, cache->dbi_forest, &mdb_key, &mdb_data, 0);
    
    free(data);
    
    if (rc != 0) {
        mdbx_txn_abort(txn);
        return -1;
    }
    
    /* Store timestamp in metadata */
    char meta_key[64];
    snprintf(meta_key, sizeof(meta_key), "forest_ts:%lu", (unsigned long)packed_key);
    uint64_t timestamp = time_to_u64(time(NULL));
    
    mdb_key.iov_base = meta_key;
    mdb_key.iov_len = strlen(meta_key);
    mdb_data.iov_base = &timestamp;
    mdb_data.iov_len = sizeof(timestamp);
    
    mdbx_put(txn, cache->dbi_metadata, &mdb_key, &mdb_data, 0);
    
    mdbx_txn_commit(txn);
    cache->stats.forest_count++;
    
    return 0;
}

int glr_cache_lookup_gss_node(glr_cache_t* cache,
                              const glr_gss_cache_key_t* key,
                              glr_stack_node_t** out_node) {
    if (!cache || !key || !out_node) return -1;
    
    uint64_t packed_key = pack_gss_key(key);
    
    MDBX_txn* txn;
    MDBX_val mdb_key, mdb_data;
    
    int rc = mdbx_txn_begin(cache->env, NULL, MDBX_TXN_RDONLY, &txn);
    if (rc != 0) return -1;
    
    mdb_key.iov_base = &packed_key;
    mdb_key.iov_len = sizeof(packed_key);
    
    rc = mdbx_get(txn, cache->dbi_gss, &mdb_key, &mdb_data);
    if (rc == MDBX_NOTFOUND) {
        mdbx_txn_abort(txn);
        cache->stats.gss_misses++;
        return 0;
    } else if (rc != 0) {
        mdbx_txn_abort(txn);
        return -1;
    }
    
    rc = glr_deserialize_stack_node((const uint8_t*)mdb_data.iov_base,
                                   mdb_data.iov_len, out_node);
    
    mdbx_txn_abort(txn);
    
    if (rc == 0) {
        cache->stats.gss_hits++;
        return 1;
    } else {
        cache->stats.gss_misses++;
        return -1;
    }
}

int glr_cache_store_gss_node(glr_cache_t* cache,
                             const glr_gss_cache_key_t* key,
                             const glr_stack_node_t* node) {
    if (!cache || !key || !node) return -1;
    
    uint64_t packed_key = pack_gss_key(key);
    
    uint8_t* data;
    size_t len;
    if (glr_serialize_stack_node(node, &data, &len) < 0) {
        return -1;
    }
    
    MDBX_txn* txn;
    MDBX_val mdb_key, mdb_data;
    
    int rc = mdbx_txn_begin(cache->env, NULL, 0, &txn);
    if (rc != 0) {
        free(data);
        return -1;
    }
    
    mdb_key.iov_base = &packed_key;
    mdb_key.iov_len = sizeof(packed_key);
    mdb_data.iov_base = data;
    mdb_data.iov_len = len;
    
    rc = mdbx_put(txn, cache->dbi_gss, &mdb_key, &mdb_data, 0);
    
    free(data);
    
    if (rc != 0) {
        mdbx_txn_abort(txn);
        return -1;
    }
    
    mdbx_txn_commit(txn);
    cache->stats.gss_count++;
    
    return 0;
}

int glr_cache_lookup_subtree(glr_cache_t* cache,
                             const glr_subtree_cache_key_t* key,
                             uint8_t** out_data,
                             size_t* out_len) {
    if (!cache || !key || !out_data || !out_len) return -1;
    
    uint64_t packed_key = pack_subtree_key(key);
    
    MDBX_txn* txn;
    MDBX_val mdb_key, mdb_data;
    
    int rc = mdbx_txn_begin(cache->env, NULL, MDBX_TXN_RDONLY, &txn);
    if (rc != 0) return -1;
    
    mdb_key.iov_base = &packed_key;
    mdb_key.iov_len = sizeof(packed_key);
    
    rc = mdbx_get(txn, cache->dbi_subtree, &mdb_key, &mdb_data);
    if (rc == MDBX_NOTFOUND) {
        mdbx_txn_abort(txn);
        cache->stats.subtree_misses++;
        return 0;
    } else if (rc != 0) {
        mdbx_txn_abort(txn);
        return -1;
    }
    
    *out_data = malloc(mdb_data.iov_len);
    if (!*out_data) {
        mdbx_txn_abort(txn);
        return -1;
    }
    
    memcpy(*out_data, mdb_data.iov_base, mdb_data.iov_len);
    *out_len = mdb_data.iov_len;
    
    mdbx_txn_abort(txn);
    cache->stats.subtree_hits++;
    
    return 1;
}

int glr_cache_store_subtree(glr_cache_t* cache,
                            const glr_subtree_cache_key_t* key,
                            const uint8_t* data,
                            size_t len) {
    if (!cache || !key || !data) return -1;
    
    uint64_t packed_key = pack_subtree_key(key);
    
    MDBX_txn* txn;
    MDBX_val mdb_key, mdb_data;
    
    int rc = mdbx_txn_begin(cache->env, NULL, 0, &txn);
    if (rc != 0) return -1;
    
    mdb_key.iov_base = &packed_key;
    mdb_key.iov_len = sizeof(packed_key);
    mdb_data.iov_base = (void*)data;
    mdb_data.iov_len = len;
    
    rc = mdbx_put(txn, cache->dbi_subtree, &mdb_key, &mdb_data, 0);
    
    if (rc != 0) {
        mdbx_txn_abort(txn);
        return -1;
    }
    
    mdbx_txn_commit(txn);
    cache->stats.subtree_count++;
    
    return 0;
}

int glr_cache_invalidate_range(glr_cache_t* cache,
                               uint32_t start_byte,
                               uint32_t end_byte) {
    glr_dependency_t *affected = NULL;
    size_t affected_count = 0;
    MDBX_txn *txn = NULL;
    int rc;

    if (!cache) return -1;
    if (start_byte >= end_byte) return -1;

    /* Find every cached key whose source range overlaps the edit. */
    if (glr_dependency_get_affected (cache, start_byte, end_byte,
                                     &affected, &affected_count) != 0)
      {
        return -1;
      }

    rc = mdbx_txn_begin(cache->env, NULL, 0, &txn);
    if (rc != 0) {
        glr_dependency_free_list(affected);
        return -1;
    }

    for (size_t i = 0; i < affected_count; i++)
      {
        MDBX_val key;
        uint64_t packed = affected[i].cache_key;
        MDBX_dbi dbi = 0;
        int have_dbi = 1;

        key.iov_base = &packed;
        key.iov_len = sizeof(packed);

        switch (affected[i].entry_type)
          {
          case GLR_CACHE_ENTRY_FOREST:
            dbi = cache->dbi_forest;
            break;
          case GLR_CACHE_ENTRY_GSS_NODE:
            dbi = cache->dbi_gss;
            break;
          case GLR_CACHE_ENTRY_SUBTREE:
            dbi = cache->dbi_subtree;
            break;
          default:
            have_dbi = 0;
            break;
          }

        if (have_dbi)
          {
            /* Best effort: missing keys are fine, real errors abort. */
            int del = mdbx_del (txn, dbi, &key, NULL);
            if (del != 0 && del != MDBX_NOTFOUND)
              {
                mdbx_txn_abort (txn);
                glr_dependency_free_list (affected);
                return -1;
              }
            switch (affected[i].entry_type)
              {
              case GLR_CACHE_ENTRY_FOREST:
                if (cache->stats.forest_count > 0)
                  cache->stats.forest_count--;
                break;
              case GLR_CACHE_ENTRY_GSS_NODE:
                if (cache->stats.gss_count > 0)
                  cache->stats.gss_count--;
                break;
              case GLR_CACHE_ENTRY_SUBTREE:
                if (cache->stats.subtree_count > 0)
                  cache->stats.subtree_count--;
                break;
              default:
                break;
              }
          }
      }

    rc = mdbx_txn_commit (txn);
    glr_dependency_free_list (affected);
    if (rc != 0)
      {
        return -1;
      }

    /* Drop the dependency records themselves so future edits do not see
       stale keys. */
    return glr_dependency_invalidate_range (cache, start_byte, end_byte);
}

int glr_cache_get_stats(glr_cache_t* cache, glr_cache_stats_t* stats) {
    if (!cache || !stats) return -1;
    
    memcpy(stats, &cache->stats, sizeof(glr_cache_stats_t));
    
    /* Get database sizes */
    MDBX_stat mdbx_stat;
    mdbx_stat.ms_psize = 0;
    
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    if (mdbx_env_stat(cache->env, &mdbx_stat, sizeof(mdbx_stat)) == 0) {
        stats->cache_size_bytes = mdbx_stat.ms_psize * 
                                  (mdbx_stat.ms_leaf_pages + mdbx_stat.ms_branch_pages);
    }
#pragma GCC diagnostic pop
    
    return 0;
}

int glr_cache_clear(glr_cache_t* cache) {
    if (!cache) return -1;
    
    MDBX_txn* txn;
    int rc = mdbx_txn_begin(cache->env, NULL, 0, &txn);
    if (rc != 0) return -1;
    
    rc = mdbx_drop(txn, cache->dbi_forest, 0);
    if (rc != 0) goto cleanup;
    
    rc = mdbx_drop(txn, cache->dbi_gss, 0);
    if (rc != 0) goto cleanup;
    
    rc = mdbx_drop(txn, cache->dbi_subtree, 0);
    if (rc != 0) goto cleanup;
    
    mdbx_txn_commit(txn);
    
    /* Reset stats */
    memset(&cache->stats, 0, sizeof(glr_cache_stats_t));
    
    return 0;
    
cleanup:
    mdbx_txn_abort(txn);
    return -1;
}

int glr_cache_vacuum(glr_cache_t* cache) {
    /* libmdbx doesn't support online vacuum */
    /* Just sync for now */
    return glr_cache_sync(cache);
}
