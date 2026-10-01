# alloc.q — debug counting allocator wrapping malloc/free.
# Tracks live bytes and live allocation count. Counters are per
# translation unit (static storage in a header-only library).

@module  alloc
@version 1.0

@param PREFIX alloc

@include <stdlib.h>

@guard

@raw {
    static unsigned long ${PREFIX}_live_bytes = 0;
    static unsigned long ${PREFIX}_live_count = 0;
}

@fn malloc {
    static inline void *
    ${PREFIX}_malloc(unsigned long n) {
        unsigned long *base = (unsigned long *)malloc(n + sizeof(unsigned long));
        unsigned long *user;
        if (!base) {
            return 0;
        }
        base[0] = n;
        user = base + 1;
        ${PREFIX}_live_bytes += n;
        ${PREFIX}_live_count += 1;
        return (void *)user;
    }
}

@fn calloc {
    static inline void *
    ${PREFIX}_calloc(unsigned long n, unsigned long sz) {
        unsigned long total = n * sz;
        unsigned long *base;
        unsigned long i;
        unsigned char *p;
        unsigned long *user;
        if (sz && total / sz != n) {
            return 0;
        }
        base = (unsigned long *)malloc(total + sizeof(unsigned long));
        if (!base) {
            return 0;
        }
        base[0] = total;
        p = (unsigned char *)(base + 1);
        for (i = 0; i < total; i++) {
            p[i] = 0;
        }
        user = base + 1;
        ${PREFIX}_live_bytes += total;
        ${PREFIX}_live_count += 1;
        return (void *)user;
    }
}

@fn free {
    static inline void
    ${PREFIX}_free(void *p) {
        if (p) {
            unsigned long *base = (unsigned long *)p - 1;
            ${PREFIX}_live_bytes -= base[0];
            ${PREFIX}_live_count -= 1;
            free(base);
        }
    }
}

@fn stats {
    static inline void
    ${PREFIX}_stats(unsigned long *bytes_out, unsigned long *count_out) {
        if (bytes_out) {
            *bytes_out = ${PREFIX}_live_bytes;
        }
        if (count_out) {
            *count_out = ${PREFIX}_live_count;
        }
    }
}

@fn reset {
    static inline void
    ${PREFIX}_reset(void) {
        ${PREFIX}_live_bytes = 0;
        ${PREFIX}_live_count = 0;
    }
}
