# glob.q — glob pattern matcher (*, ?, [...] character classes).
# No backslash escapes. The whole string must match the whole pattern.

@module  glob
@version 1.0

@param PREFIX glob

@guard

@fn match_class {
    /* Match one char against a [...] class at pat (pat[0] == '[').
     * Returns the length of the class expression, or 0 if no match. */
    static int
    ${PREFIX}_match_class(const char *pat, char c) {
        int neg = 0;
        int hit = 0;
        int i = 1;
        if (c == 0) {
            return 0;
        }
        if (pat[i] == '^') {
            neg = 1;
            i++;
        }
        if (pat[i] == ']') {
            if (c == ']') {
                hit = 1;
            }
            i++;
        }
        while (pat[i] && pat[i] != ']') {
            if (pat[i + 1] == '-' && pat[i + 2] && pat[i + 2] != ']') {
                if (c >= pat[i] && c <= pat[i + 2]) {
                    hit = 1;
                }
                i += 3;
            } else {
                if (c == pat[i]) {
                    hit = 1;
                }
                i++;
            }
        }
        if (!pat[i]) {
            return 0;
        }
        if (neg) {
            hit = !hit;
        }
        return hit ? i + 1 : 0;
    }
}

@fn match {
    static int
    ${PREFIX}_match(const char *pat, const char *str) {
        if (*pat == '*') {
            while (*pat == '*') {
                pat++;
            }
            if (!*pat) {
                return 1;
            }
            for (; *str; str++) {
                if (${PREFIX}_match(pat, str)) {
                    return 1;
                }
            }
            return ${PREFIX}_match(pat, str);
        }
        if (!*str) {
            return *pat == 0;
        }
        if (*pat == '?') {
            return ${PREFIX}_match(pat + 1, str + 1);
        }
        if (*pat == '[') {
            int n = ${PREFIX}_match_class(pat, *str);
            if (!n) {
                return 0;
            }
            return ${PREFIX}_match(pat + n, str + 1);
        }
        if (*pat == *str) {
            return ${PREFIX}_match(pat + 1, str + 1);
        }
        return 0;
    }
}
