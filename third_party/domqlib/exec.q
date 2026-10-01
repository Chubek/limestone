# exec.q — popen-based command execution with output capture

@module  exec
@version 1.0

@param PREFIX exec

@include <stdio.h>
@include <sys/wait.h>

@guard

@fn run {
    /*
     * Run cmd via the shell, capturing stdout into buf (up to cap-1
     * bytes plus NUL). Returns the raw pclose status, or -1 when the
     * command could not be started. Use exit_code() to decode it.
     */
    static inline int
    ${PREFIX}_run(const char *cmd, char *buf, int cap) {
        FILE *p = popen(cmd, "r");
        int total = 0;
        int st;
        if (!p) {
            return -1;
        }
        if (buf && cap > 1) {
            size_t n;
            while (total < cap - 1) {
                n = fread(buf + total, 1, (size_t)(cap - 1 - total), p);
                if (n == 0) {
                    break;
                }
                total += (int)n;
            }
            buf[total] = 0;
        }
        st = pclose(p);
        return st;
    }
}

@fn exit_code {
    /* Decode a status from run() into the process exit code. */
    static inline int
    ${PREFIX}_exit_code(int status) {
        if (status < 0) {
            return -1;
        }
        return WEXITSTATUS(status);
    }
}

@fn status_ok {
    /* True when status represents a clean exit(0). */
    static inline int
    ${PREFIX}_status_ok(int status) {
        return status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
}
