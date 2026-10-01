# string.q — dynamic string builder (char buffer)

@module  string
@version 1.0

@param PREFIX string

@include <stdlib.h>
@include <string.h>
@include <assert.h>
@include <limits.h>

@guard

@struct {
    typedef struct {
        char *data;
        int  len;
        int  cap;
    } ${PREFIX}_t;
}

@fn init {
    static inline void
    ${PREFIX}_init(${PREFIX}_t *s) {
        s->data = 0;
        s->len  = 0;
        s->cap  = 0;
    }
}

@fn reserve {
    /* Ensure room for at least n characters (excluding NUL). */
    static inline int
    ${PREFIX}_reserve(${PREFIX}_t *s, int n) {
        if (n < 0 || n == INT_MAX) return 0;
        if (n + 1 > s->cap) {
            int ncap = s->cap ? s->cap : 16;
            char *nd;
            while (ncap < n + 1) {
                if (ncap > INT_MAX / 2) { ncap = n + 1; break; }
                ncap *= 2;
            }
            nd = (char *)realloc(s->data, (size_t)ncap);
            if (!nd) {
                return 0;
            }
            s->data = nd;
            s->cap  = ncap;
        }
        return 1;
    }
}

@fn append_char {
    static inline void
    ${PREFIX}_append_char(${PREFIX}_t *s, char c) {
        if (s->len >= INT_MAX - 1 || !${PREFIX}_reserve(s, s->len + 1)) abort();
        s->data[s->len++] = c;
    }
}

@fn append_cstr {
    static inline void
    ${PREFIX}_append_cstr(${PREFIX}_t *s, const char *src) {
        size_t n = strlen(src);
        size_t i;
        if (n >= (size_t)(INT_MAX - s->len) || !${PREFIX}_reserve(s, s->len + (int)n)) abort();
        for (i = 0; i < n; i++) {
            s->data[s->len++] = src[i];
        }
    }
}

@fn append_n {
    static inline void
    ${PREFIX}_append_n(${PREFIX}_t *s, const char *src, int n) {
        int i;
        if (n < 0 || n >= INT_MAX - s->len || !${PREFIX}_reserve(s, s->len + n)) abort();
        for (i = 0; i < n; i++) {
            s->data[s->len++] = src[i];
        }
    }
}

@fn len {
    static inline int
    ${PREFIX}_len(const ${PREFIX}_t *s) {
        return s->len;
    }
}

@fn cstr {
    /* NUL-terminate and return the buffer. Never returns NULL. */
    static inline const char *
    ${PREFIX}_cstr(${PREFIX}_t *s) {
        if (!${PREFIX}_reserve(s, s->len)) abort();
        s->data[s->len] = 0;
        return s->data ? s->data : "";
    }
}

@fn clear {
    static inline void
    ${PREFIX}_clear(${PREFIX}_t *s) {
        s->len = 0;
    }
}

@fn free {
    static inline void
    ${PREFIX}_free(${PREFIX}_t *s) {
        free(s->data);
        s->data = 0;
        s->len  = 0;
        s->cap  = 0;
    }
}
