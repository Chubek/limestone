#include <glr/stringpool.h>

#include <klib/kalloc.h>
#include <klib/khash.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
  const char *text;
  size_t length;
} pool_key_t;

static khint_t
pool_hash (pool_key_t key)
{
  khint_t hash = 2166136261u;
  for (size_t i = 0; i < key.length; i++)
    hash = (hash ^ (unsigned char) key.text[i]) * 16777619u;
  return hash;
}

static int
pool_equal (pool_key_t a, pool_key_t b)
{
  return a.length == b.length
         && (a.length == 0 || memcmp (a.text, b.text, a.length) == 0);
}

KHASH_INIT (glr_strings, pool_key_t, char, 0, pool_hash, pool_equal)

struct glr_stringpool
{
  khash_t (glr_strings) *index;
  void *arena;
  pthread_mutex_t mutex;
  size_t bytes;
};

glr_stringpool_t *
glr_stringpool_create (void)
{
  glr_stringpool_t *pool = calloc (1, sizeof (*pool));
  if (pool == NULL)
    return NULL;
  if (pthread_mutex_init (&pool->mutex, NULL) != 0)
    {
      free (pool);
      return NULL;
    }
  pool->index = kh_init (glr_strings);
  pool->arena = km_init2 (NULL, 1024);
  if (pool->index == NULL || pool->arena == NULL)
    {
      glr_stringpool_destroy (pool);
      return NULL;
    }
  return pool;
}

void
glr_stringpool_destroy (glr_stringpool_t *pool)
{
  if (pool == NULL)
    return;
  kh_destroy (glr_strings, pool->index);
  km_destroy (pool->arena);
  pthread_mutex_destroy (&pool->mutex);
  free (pool);
}

const char *
glr_stringpool_intern_n (glr_stringpool_t *pool, const void *text, size_t length)
{
  pool_key_t key = { text, length };
  khint_t slot;
  int inserted;
  char *copy;
  if (pool == NULL || (text == NULL && length != 0)
      || length > SIZE_MAX - 2 * sizeof (size_t))
    return NULL;
  pthread_mutex_lock (&pool->mutex);
  slot = kh_get (glr_strings, pool->index, key);
  if (slot != kh_end (pool->index))
    {
      const char *existing = kh_key (pool->index, slot).text;
      pthread_mutex_unlock (&pool->mutex);
      return existing;
    }
  if (length + 1 > SIZE_MAX - pool->bytes)
    {
      pthread_mutex_unlock (&pool->mutex);
      return NULL;
    }
  copy = (kmalloc) (pool->arena, length + 1);
  if (copy == NULL)
    {
      pthread_mutex_unlock (&pool->mutex);
      return NULL;
    }
  if (length != 0)
    memcpy (copy, text, length);
  copy[length] = '\0';
  key.text = copy;
  slot = kh_put (glr_strings, pool->index, key, &inserted);
  if (inserted < 0)
    {
      (kfree) (pool->arena, copy);
      pthread_mutex_unlock (&pool->mutex);
      return NULL;
    }
  pool->bytes += length + 1;
  pthread_mutex_unlock (&pool->mutex);
  return copy;
}

const char *
glr_stringpool_intern (glr_stringpool_t *pool, const char *text)
{
  return text != NULL ? glr_stringpool_intern_n (pool, text, strlen (text)) : NULL;
}

size_t
glr_stringpool_count (const glr_stringpool_t *pool)
{
  size_t count;
  if (pool == NULL)
    return 0;
  pthread_mutex_lock ((pthread_mutex_t *) &pool->mutex);
  count = kh_size (pool->index);
  pthread_mutex_unlock ((pthread_mutex_t *) &pool->mutex);
  return count;
}

size_t
glr_stringpool_bytes (const glr_stringpool_t *pool)
{
  size_t bytes;
  if (pool == NULL)
    return 0;
  pthread_mutex_lock ((pthread_mutex_t *) &pool->mutex);
  bytes = pool->bytes;
  pthread_mutex_unlock ((pthread_mutex_t *) &pool->mutex);
  return bytes;
}
