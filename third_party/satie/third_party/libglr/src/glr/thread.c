#include <glr/thread.h>
#include "containers.h"

#include <klib/kthread.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>

typedef struct
{
  glr_thread_task_fn callback;
  void *data;
} glr_job_t;

#define T glr_job_t
#define P
#include <ctl/vec.h>

struct glr_thread_pool
{
  pthread_t *threads;
  size_t thread_count;
  vec_glr_job_t queue;
  size_t head;
  size_t active;
  bool stopping;
  pthread_mutex_t mutex;
  pthread_cond_t ready;
  pthread_cond_t idle;
};

static void *
pool_worker (void *data)
{
  glr_thread_pool_t *pool = data;
  pthread_mutex_lock (&pool->mutex);
  for (;;)
    {
      glr_job_t job;
      while (pool->head == pool->queue.size && !pool->stopping)
        pthread_cond_wait (&pool->ready, &pool->mutex);
      if (pool->head == pool->queue.size && pool->stopping)
        break;
      job = *vec_glr_job_t_at (&pool->queue, pool->head++);
      if (pool->head == pool->queue.size)
        {
          vec_glr_job_t_clear (&pool->queue);
          pool->head = 0;
        }
      pool->active++;
      pthread_mutex_unlock (&pool->mutex);
      job.callback (job.data);
      pthread_mutex_lock (&pool->mutex);
      pool->active--;
      if (pool->active == 0 && pool->queue.size == pool->head)
        pthread_cond_broadcast (&pool->idle);
    }
  pthread_mutex_unlock (&pool->mutex);
  return NULL;
}

glr_thread_pool_t *
glr_thread_pool_create (size_t thread_count)
{
  glr_thread_pool_t *pool;
  if (thread_count == 0 || thread_count > INT_MAX
      || thread_count > SIZE_MAX / sizeof (pthread_t))
    return NULL;
  pool = calloc (1, sizeof (*pool));
  if (pool == NULL)
    return NULL;
  pool->queue = vec_glr_job_t_init ();
  if (pthread_mutex_init (&pool->mutex, NULL) != 0)
    goto fail;
  if (pthread_cond_init (&pool->ready, NULL) != 0)
    goto fail_mutex;
  if (pthread_cond_init (&pool->idle, NULL) != 0)
    goto fail_ready;
  pool->threads = calloc (thread_count, sizeof (*pool->threads));
  if (pool->threads == NULL)
    goto fail_idle;
  for (size_t i = 0; i < thread_count; i++)
    {
      if (pthread_create (&pool->threads[i], NULL, pool_worker, pool) != 0)
        {
          glr_thread_pool_destroy (pool);
          return NULL;
        }
      pool->thread_count++;
    }
  return pool;
fail_idle:
  pthread_cond_destroy (&pool->idle);
fail_ready:
  pthread_cond_destroy (&pool->ready);
fail_mutex:
  pthread_mutex_destroy (&pool->mutex);
fail:
  free (pool);
  return NULL;
}

int
glr_thread_pool_submit (glr_thread_pool_t *pool, glr_thread_task_fn task,
                         void *user_data)
{
  glr_job_t *grown;
  glr_job_t job = { task, user_data };
  if (pool == NULL || task == NULL)
    return -1;
  pthread_mutex_lock (&pool->mutex);
  if (pool->stopping || pool->queue.size == SIZE_MAX)
    {
      pthread_mutex_unlock (&pool->mutex);
      return -1;
    }
  grown = GLR_VECTOR_RESERVE (&pool->queue, pool->queue.size + 1);
  if (grown == NULL)
    {
      pthread_mutex_unlock (&pool->mutex);
      return -1;
    }
  pool->queue.value = grown;
  vec_glr_job_t_push_back (&pool->queue, job);
  pthread_cond_signal (&pool->ready);
  pthread_mutex_unlock (&pool->mutex);
  return 0;
}

int
glr_thread_pool_wait (glr_thread_pool_t *pool)
{
  if (pool == NULL)
    return -1;
  pthread_mutex_lock (&pool->mutex);
  while (pool->active != 0 || pool->queue.size != pool->head)
    pthread_cond_wait (&pool->idle, &pool->mutex);
  pthread_mutex_unlock (&pool->mutex);
  return 0;
}

void
glr_thread_pool_destroy (glr_thread_pool_t *pool)
{
  if (pool == NULL)
    return;
  pthread_mutex_lock (&pool->mutex);
  pool->stopping = true;
  pthread_cond_broadcast (&pool->ready);
  pthread_mutex_unlock (&pool->mutex);
  for (size_t i = 0; i < pool->thread_count; i++)
    pthread_join (pool->threads[i], NULL);
  vec_glr_job_t_free (&pool->queue);
  free (pool->threads);
  pthread_cond_destroy (&pool->idle);
  pthread_cond_destroy (&pool->ready);
  pthread_mutex_destroy (&pool->mutex);
  free (pool);
}

size_t
glr_thread_pool_size (const glr_thread_pool_t *pool)
{
  return pool != NULL ? pool->thread_count : 0;
}

typedef struct
{
  glr_parallel_fn callback;
  void *data;
} parallel_context_t;

static void
parallel_task (void *data, long index, int worker_id)
{
  parallel_context_t *context = data;
  context->callback ((size_t) index, (size_t) worker_id, context->data);
}

int
glr_parallel_for (size_t thread_count, size_t count, glr_parallel_fn task,
                   void *user_data)
{
  parallel_context_t context = { task, user_data };
  if (task == NULL || thread_count == 0 || thread_count > 256
      || count > (size_t) LONG_MAX - 256)
    return -1;
  if (count == 0)
    return 0;
  if (thread_count > count)
    thread_count = count;
  kt_for ((int) thread_count, parallel_task, &context, (long) count);
  return 0;
}
