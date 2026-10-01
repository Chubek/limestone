# expr.q — recursive-descent arithmetic expression evaluator.
# Grammar: expr := term (('+'|'-') term)* ; term := factor (('*'|'/') factor)*
# factor := number | '(' expr ')' | '-' factor. Numbers via strtod.

@module  expr
@version 1.0

@param SCALAR double
@param PREFIX expr

@include <stdlib.h>

@guard

@struct {
    typedef struct {
        const char *s;
        int        err;
    } ${PREFIX}_p;
}

@raw {
    /* forward declarations: the parser functions are mutually recursive */
    static void ${PREFIX}_skip(${PREFIX}_p *p);
    static ${SCALAR} ${PREFIX}_parse_expr(${PREFIX}_p *p);
    static ${SCALAR} ${PREFIX}_parse_term(${PREFIX}_p *p);
    static ${SCALAR} ${PREFIX}_parse_factor(${PREFIX}_p *p);
}

@fn skip {
    static void
    ${PREFIX}_skip(${PREFIX}_p *p) {
        while (*p->s == ' ' || *p->s == 9 || *p->s == 10) {
            p->s++;
        }
    }
}

@fn parse_expr {
    static ${SCALAR}
    ${PREFIX}_parse_expr(${PREFIX}_p *p) {
        ${SCALAR} v = ${PREFIX}_parse_term(p);
        for (;;) {
            ${PREFIX}_skip(p);
            if (*p->s == '+') {
                p->s++;
                v += ${PREFIX}_parse_term(p);
            } else if (*p->s == '-') {
                p->s++;
                v -= ${PREFIX}_parse_term(p);
            } else {
                return v;
            }
        }
    }
}

@fn parse_term {
    static ${SCALAR}
    ${PREFIX}_parse_term(${PREFIX}_p *p) {
        ${SCALAR} v = ${PREFIX}_parse_factor(p);
        for (;;) {
            ${PREFIX}_skip(p);
            if (*p->s == '*') {
                p->s++;
                v *= ${PREFIX}_parse_factor(p);
            } else if (*p->s == '/') {
                ${SCALAR} d;
                p->s++;
                d = ${PREFIX}_parse_factor(p);
                if (d == 0) {
                    p->err = 1;
                    return 0;
                }
                v /= d;
            } else {
                return v;
            }
        }
    }
}

@fn parse_factor {
    static ${SCALAR}
    ${PREFIX}_parse_factor(${PREFIX}_p *p) {
        ${SCALAR} v;
        char *end;
        ${PREFIX}_skip(p);
        if (*p->s == '(') {
            p->s++;
            v = ${PREFIX}_parse_expr(p);
            ${PREFIX}_skip(p);
            if (*p->s != ')') {
                p->err = 1;
                return 0;
            }
            p->s++;
            return v;
        }
        if (*p->s == '-') {
            p->s++;
            return -${PREFIX}_parse_factor(p);
        }
        v = (${SCALAR})strtod(p->s, &end);
        if (end == p->s) {
            p->err = 1;
            return 0;
        }
        p->s = end;
        return v;
    }
}

@fn eval {
    /*
     * Evaluate s, storing the result in *out. Returns 1 on success,
     * 0 on syntax error (or trailing garbage).
     */
    static inline int
    ${PREFIX}_eval(const char *s, ${SCALAR} *out) {
        ${PREFIX}_p p;
        ${SCALAR} v;
        p.s   = s;
        p.err = 0;
        v = ${PREFIX}_parse_expr(&p);
        ${PREFIX}_skip(&p);
        if (p.err || *p.s) {
            return 0;
        }
        *out = v;
        return 1;
    }
}
