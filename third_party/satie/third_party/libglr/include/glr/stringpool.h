#ifndef GLR_STRINGPOOL_H
#define GLR_STRINGPOOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Thread-safe, length-aware string interning. Pointers remain stable until
    the pool is destroyed, including across growth. Strings may contain NULs;
    an extra NUL byte is always appended. Destruction requires exclusive use. */
typedef struct glr_stringpool glr_stringpool_t;
glr_stringpool_t *glr_stringpool_create (void);
void glr_stringpool_destroy (glr_stringpool_t *pool);
const char *glr_stringpool_intern (glr_stringpool_t *pool, const char *text);
const char *glr_stringpool_intern_n (glr_stringpool_t *pool, const void *text,
                                    size_t length);
size_t glr_stringpool_count (const glr_stringpool_t *pool);
size_t glr_stringpool_bytes (const glr_stringpool_t *pool);

#ifdef __cplusplus
}
#endif
#endif
