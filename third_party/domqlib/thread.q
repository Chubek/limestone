# thread.q — thin pthreads wrapper: mutexes plus spawn/join

@module  thread
@version 1.0

@param PREFIX thread

@include <pthread.h>

@guard

@struct {
    typedef struct {
        pthread_mutex_t m;
    } ${PREFIX}_mutex_t;

    typedef struct {
        pthread_t h;
        int      live;
    } ${PREFIX}_t;
}

@fn mutex_init {
    static inline int
    ${PREFIX}_mutex_init(${PREFIX}_mutex_t *m) {
        return pthread_mutex_init(&m->m, 0) == 0;
    }
}

@fn mutex_lock {
    static inline int
    ${PREFIX}_mutex_lock(${PREFIX}_mutex_t *m) {
        return pthread_mutex_lock(&m->m) == 0;
    }
}

@fn mutex_unlock {
    static inline int
    ${PREFIX}_mutex_unlock(${PREFIX}_mutex_t *m) {
        return pthread_mutex_unlock(&m->m) == 0;
    }
}

@fn mutex_destroy {
    static inline int
    ${PREFIX}_mutex_destroy(${PREFIX}_mutex_t *m) {
        return pthread_mutex_destroy(&m->m) == 0;
    }
}

@fn create {
    /* Spawn fn(arg). Returns 1 on success, 0 on failure. */
    static inline int
    ${PREFIX}_create(${PREFIX}_t *t, void *(*fn)(void *), void *arg) {
        t->live = 0;
        if (pthread_create(&t->h, 0, fn, arg) != 0) {
            return 0;
        }
        t->live = 1;
        return 1;
    }
}

@fn join {
    /* Join a live thread, optionally capturing its return value. */
    static inline int
    ${PREFIX}_join(${PREFIX}_t *t, void **ret) {
        void *rv = 0;
        if (!t->live) {
            return 0;
        }
        if (pthread_join(t->h, &rv) != 0) {
            return 0;
        }
        t->live = 0;
        if (ret) {
            *ret = rv;
        }
        return 1;
    }
}
