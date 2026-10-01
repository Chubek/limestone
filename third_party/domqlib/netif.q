# netif.q — small network-interface utilities: hostname and
# dotted-decimal IPv4 parsing/formatting (no resolver involved).

@module  netif
@version 1.0

@param PREFIX netif

@include <unistd.h>
@include <stdio.h>

@guard

@fn hostname {
    /* Copy the host name into buf (capacity n). Returns 0 on success. */
    static inline int
    ${PREFIX}_hostname(char *buf, int n) {
        if (!buf || n <= 0) {
            return -1;
        }
        if (gethostname(buf, (size_t)n) != 0) {
            return -1;
        }
        buf[n - 1] = 0;
        return 0;
    }
}

@fn ipv4_parse {
    /*
     * Parse "a.b.c.d" into out[4]. Returns 1 on success, 0 on any
     * malformed input (bad digits, out-of-range octet, trailing text).
     */
    static inline int
    ${PREFIX}_ipv4_parse(const char *s, unsigned char out[4]) {
        int i;
        for (i = 0; i < 4; i++) {
            unsigned long v = 0;
            int digits = 0;
            if (!s || !*s) {
                return 0;
            }
            while (*s >= '0' && *s <= '9') {
                v = v * 10 + (unsigned long)(*s - '0');
                if (v > 255) {
                    return 0;
                }
                s++;
                digits++;
            }
            if (!digits) {
                return 0;
            }
            out[i] = (unsigned char)v;
            if (i < 3) {
                if (*s != '.') {
                    return 0;
                }
                s++;
            }
        }
        return *s == 0;
    }
}

@fn ipv4_format {
    /* Format addr[4] as "a.b.c.d" into buf (capacity n). */
    static inline int
    ${PREFIX}_ipv4_format(const unsigned char addr[4], char *buf, int n) {
        int w;
        if (!buf || n < 16) {
            return -1;
        }
        w = snprintf(buf, (size_t)n, "%u.%u.%u.%u",
                     (unsigned)addr[0], (unsigned)addr[1],
                     (unsigned)addr[2], (unsigned)addr[3]);
        return (w > 0 && w < n) ? 0 : -1;
    }
}
