# regexp.q — tiny regular expression engine (Rob Pike style).
# Supports ^ $ . * + ? [...] [^...] and literals. No backslash escapes.
# match() finds the leftmost match anywhere in the text (like grep);
# anchored() requires the whole text to match (like fnmatch).

@module  regexp
@version 1.0

@param PREFIX regexp

@guard

@fn match_one {
    /* True when the atom at re matches the single char c. */
    static int
    ${PREFIX}_match_one(const char *re, char c) {
        int i;
        int neg;
        int hit;
        if (re[0] == '[') {
            if (c == 0) {
                return 0;
            }
            i   = 1;
            neg = 0;
            hit = 0;
            if (re[i] == '^') {
                neg = 1;
                i++;
            }
            if (re[i] == ']') {
                if (c == ']') {
                    hit = 1;
                }
                i++;
            }
            while (re[i] && re[i] != ']') {
                if (re[i + 1] == '-' && re[i + 2] && re[i + 2] != ']') {
                    if (c >= re[i] && c <= re[i + 2]) {
                        hit = 1;
                    }
                    i += 3;
                } else {
                    if (c == re[i]) {
                        hit = 1;
                    }
                    i++;
                }
            }
            return neg ? !hit : hit;
        }
        if (re[0] == '.') {
            return c != 0;
        }
        return re[0] == c;
    }
}

@fn atom_len {
    /* Length of one atom: a [...] class or a single char. */
    static int
    ${PREFIX}_atom_len(const char *re) {
        int i;
        if (re[0] != '[') {
            return 1;
        }
        i = 1;
        if (re[i] == '^') {
            i++;
        }
        if (re[i] == ']') {
            i++;
        }
        while (re[i] && re[i] != ']') {
            i++;
        }
        return re[i] ? i + 1 : 1;
    }
}

@fn match_here {
    static const char *
    ${PREFIX}_match_here(const char *re, const char *text) {
        int alen;
        char op;
        const char *t;
        if (!re[0]) {
            return text;
        }
        if (re[0] == '$' && !re[1]) {
            return *text ? 0 : text;
        }
        alen = ${PREFIX}_atom_len(re);
        op = re[alen];
        if (op == '*') {
            t = text;
            while (*t && ${PREFIX}_match_one(re, *t)) {
                t++;
            }
            for (; t >= text; t--) {
                const char *r = ${PREFIX}_match_here(re + alen + 1, t);
                if (r) {
                    return r;
                }
                if (t == text) {
                    break;
                }
            }
            return 0;
        }
        if (op == '+') {
            if (!*text || !${PREFIX}_match_one(re, *text)) {
                return 0;
            }
            return ${PREFIX}_match_here(re, text + 1) ?
                ${PREFIX}_match_here(re, text + 1) :
                ${PREFIX}_match_here(re + alen + 1, text + 1);
        }
        if (op == '?') {
            if (*text && ${PREFIX}_match_one(re, *text)) {
                const char *r = ${PREFIX}_match_here(re + alen + 1, text + 1);
                if (r) {
                    return r;
                }
            }
            return ${PREFIX}_match_here(re + alen + 1, text);
        }
        if (*text && ${PREFIX}_match_one(re, *text)) {
            return ${PREFIX}_match_here(re + alen, text + 1);
        }
        return 0;
    }
}

@fn match {
    /* Leftmost match anywhere (NULL when none). '^' anchors to start. */
    static inline const char *
    ${PREFIX}_match(const char *re, const char *text) {
        const char *t;
        if (re[0] == '^') {
            return ${PREFIX}_match_here(re + 1, text);
        }
        for (t = text; ; t++) {
            const char *r = ${PREFIX}_match_here(re, t);
            if (r) {
                return t;
            }
            if (!*t) {
                return 0;
            }
        }
    }
}

@fn anchored {
    /*
     * True when the whole text matches the pattern. A match that merely
     * consumes a suffix is not enough -- matching starts at the text.
     */
    static inline int
    ${PREFIX}_anchored(const char *re, const char *text) {
        const char *r;
        if (re[0] == '^') {
            re++;
        }
        r = ${PREFIX}_match_here(re, text);
        return r != 0 && *r == 0;
    }
}
