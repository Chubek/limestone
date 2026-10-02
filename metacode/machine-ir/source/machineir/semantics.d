module machineir.semantics;

import std.array : appender;
import std.string : strip, replace;

enum SExprKind { atom, stringLiteral, number, list }

struct SExpr {
    SExprKind kind;
    string text;
    SExpr[] children;

    static SExpr atom(string s) { return SExpr(SExprKind.atom, s, null); }
    static SExpr str(string s) { return SExpr(SExprKind.stringLiteral, s, null); }
    static SExpr num(string s) { return SExpr(SExprKind.number, s, null); }
    static SExpr list(SExpr[] xs) { return SExpr(SExprKind.list, "", xs); }

    bool isAtom(string s) const { return kind == SExprKind.atom && text == s; }

    string toString() const {
        auto buffer = appender!string();
        write(buffer);
        return buffer.data;
    }

    void write(R)(ref R buffer) const {
        final switch (kind) {
        case SExprKind.atom: buffer.put(text); break;
        case SExprKind.number: buffer.put(text); break;
        case SExprKind.stringLiteral:
            buffer.put(`"`); buffer.put(text.replace(`\`, `\\`).replace(`"`, `\"`)); buffer.put(`"`); break;
        case SExprKind.list:
            buffer.put("(");
            foreach (i, x; children) { if (i) buffer.put(" "); x.write(buffer); }
            buffer.put(")");
            break;
        }
    }
}

class SemanticError : Exception {
    this(string msg) { super(msg); }
}

private struct STok {
    enum Kind { lparen, rparen, atom, stringLiteral, number, eof }
    Kind kind;
    string text;
}

private class SLexer {
    string s; size_t p;
    this(string s) { this.s = s; }
    STok next() {
        while (p < s.length && (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' || s[p] == '\n')) ++p;
        if (p == s.length) return STok(STok.Kind.eof, "");
        char c = s[p++];
        if (c == '(') return STok(STok.Kind.lparen, "(");
        if (c == ')') return STok(STok.Kind.rparen, ")");
        if (c == '"') {
            auto b = appender!string();
            while (p < s.length) {
                c = s[p++];
                if (c == '"') return STok(STok.Kind.stringLiteral, b.data);
                if (c == '\\' && p < s.length) b.put(s[p++]);
                else b.put(c);
            }
            throw new SemanticError("unterminated semantic string");
        }
        size_t q = p - 1;
        while (p < s.length && s[p] != '(' && s[p] != ')' &&
               s[p] != ' ' && s[p] != '\t' && s[p] != '\r' && s[p] != '\n') ++p;
        auto t = s[q .. p];
        bool num = t.length > 0 && ((t[0] >= '0' && t[0] <= '9') || t[0] == '-');
        return STok(num ? STok.Kind.number : STok.Kind.atom, t.idup);
    }
}

SExpr parseSExpr(string s) {
    auto l = new SLexer(s.strip);
    auto t = l.next();
    SExpr delegate(STok,size_t) parse;
    parse = (STok first,size_t depth) {
        if(depth>256)throw new SemanticError("semantic nesting limit exceeded");
        if (first.kind == STok.Kind.lparen) {
            SExpr[] xs;
            while (true) {
                auto x = l.next();
                if (x.kind == STok.Kind.eof) throw new SemanticError("unterminated semantic expression");
                if (x.kind == STok.Kind.rparen) break;
                xs ~= parse(x,depth+1);
            }
            return SExpr.list(xs);
        }
        if (first.kind == STok.Kind.rparen)
            throw new SemanticError("unexpected ')' in semantic expression");
        if (first.kind == STok.Kind.stringLiteral) return SExpr.str(first.text);
        if (first.kind == STok.Kind.number) return SExpr.num(first.text);
        if (first.kind == STok.Kind.atom) return SExpr.atom(first.text);
        throw new SemanticError("invalid semantic expression");
    };
    auto x = parse(t,0);
    if (l.next().kind != STok.Kind.eof) throw new SemanticError("trailing semantic expression");
    return x;
}
