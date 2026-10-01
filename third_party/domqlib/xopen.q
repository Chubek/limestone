# xopen.q — whole-file read/write helpers plus existence check

@module  xopen
@version 1.0

@param PREFIX xopen

@include <stdio.h>

@guard

@fn write_all {
    /*
     * Write len bytes to path (created/truncated). Returns the number
     * of bytes written, or -1 on error.
     */
    static inline long
    ${PREFIX}_write_all(const char *path, const void *data, unsigned long len) {
        FILE *f = fopen(path, "wb");
        const char *p = (const char *)data;
        unsigned long left = len;
        if (!f) {
            return -1;
        }
        while (left) {
            size_t w = fwrite(p, 1, (size_t)left, f);
            if (w == 0) {
                fclose(f);
                return -1;
            }
            p    += w;
            left -= (unsigned long)w;
        }
        if (fclose(f) != 0) {
            return -1;
        }
        return (long)len;
    }
}

@fn read_all {
    /*
     * Read up to cap bytes from path into buf. Returns the number of
     * bytes read, or -1 on error.
     */
    static inline long
    ${PREFIX}_read_all(const char *path, void *buf, unsigned long cap) {
        FILE *f = fopen(path, "rb");
        char *p = (char *)buf;
        unsigned long total = 0;
        if (!f) {
            return -1;
        }
        while (total < cap) {
            size_t r = fread(p + total, 1, (size_t)(cap - total), f);
            if (r == 0) {
                break;
            }
            total += (unsigned long)r;
        }
        if (ferror(f)) {
            fclose(f);
            return -1;
        }
        fclose(f);
        return (long)total;
    }
}

@fn exists {
    static inline int
    ${PREFIX}_exists(const char *path) {
        FILE *f = fopen(path, "rb");
        if (!f) {
            return 0;
        }
        fclose(f);
        return 1;
    }
}
