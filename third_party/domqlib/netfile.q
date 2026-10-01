# netfile.q — length-prefixed message framing over a byte-stream fd.
# Wire format: 4-byte big-endian length followed by the payload bytes.

@module  netfile
@version 1.0

@param PREFIX netfile

@include <unistd.h>
@include <stdint.h>
@include <errno.h>

@guard

@fn write_all {
    static int
    ${PREFIX}_write_all(int fd, const void *buf, unsigned long len) {
        const char *p = (const char *)buf;
        while (len) {
            ssize_t w = write(fd, p, (size_t)len);
            if (w < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return -1;
            }
            if (w == 0) {
                return -1;
            }
            p   += w;
            len -= (unsigned long)w;
        }
        return 0;
    }
}

@fn read_all {
    static int
    ${PREFIX}_read_all(int fd, void *buf, unsigned long len) {
        char *p = (char *)buf;
        while (len) {
            ssize_t r = read(fd, p, (size_t)len);
            if (r < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return -1;
            }
            if (r == 0) {
                return -1;
            }
            p   += r;
            len -= (unsigned long)r;
        }
        return 0;
    }
}

@fn send_msg {
    /* Send one message. Returns 0 on success, -1 on error. */
    static inline int
    ${PREFIX}_send_msg(int fd, const void *buf, unsigned long len) {
        unsigned char hdr[4];
        if (len > 0xFFFFFFFFu) {
            return -1;
        }
        hdr[0] = (unsigned char)((len >> 24) & 0xFFu);
        hdr[1] = (unsigned char)((len >> 16) & 0xFFu);
        hdr[2] = (unsigned char)((len >> 8) & 0xFFu);
        hdr[3] = (unsigned char)(len & 0xFFu);
        if (${PREFIX}_write_all(fd, hdr, 4)) {
            return -1;
        }
        return ${PREFIX}_write_all(fd, buf, len);
    }
}

@fn recv_msg {
    /*
     * Receive one message into buf (capacity cap). Returns the payload
     * length on success, -1 on error or when the message exceeds cap.
     */
    static inline long
    ${PREFIX}_recv_msg(int fd, void *buf, unsigned long cap) {
        unsigned char hdr[4];
        unsigned long len;
        if (${PREFIX}_read_all(fd, hdr, 4)) {
            return -1;
        }
        len = ((unsigned long)hdr[0] << 24) |
              ((unsigned long)hdr[1] << 16) |
              ((unsigned long)hdr[2] << 8) |
              (unsigned long)hdr[3];
        if (len > cap) {
            return -1;
        }
        if (${PREFIX}_read_all(fd, buf, len)) {
            return -1;
        }
        return (long)len;
    }
}
