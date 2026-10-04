#ifndef GLR_THREAD_H
#define GLR_THREAD_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct glr_thread_pool glr_thread_pool_t;
typedef void (*glr_thread_task_fn) (void *user_data);
typedef void (*glr_parallel_fn) (size_t index, size_t worker_id, void *user_data);

/** Persistent workers with a synchronized CTL job queue. Submit is asynchronous;
    wait is a barrier for submitted work. Destroy drains and joins the workers.
    A callback must not wait on or destroy its own pool. */
glr_thread_pool_t *glr_thread_pool_create (size_t thread_count);
void glr_thread_pool_destroy (glr_thread_pool_t *pool);
int glr_thread_pool_submit (glr_thread_pool_t *pool, glr_thread_task_fn task,
                            void *user_data);
int glr_thread_pool_wait (glr_thread_pool_t *pool);
size_t glr_thread_pool_size (const glr_thread_pool_t *pool);

/** Synchronous indexed work, scheduled through klib's work-stealing loop.
    The callback is invoked exactly once per index with a worker-local id.
    Independent calls may run concurrently. thread_count must be in [1, 256]. */
int glr_parallel_for (size_t thread_count, size_t count, glr_parallel_fn task,
                      void *user_data);

#ifdef __cplusplus
}
#endif
#endif
