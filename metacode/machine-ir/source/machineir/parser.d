module machineir.parser;

import std.conv : to;
import std.string : strip, toLower;
import std.array : appender;
import machineir.semantics : SExpr, parseSExpr;

enum ValueKind { scalar, stringLiteral, number, boolean, array, object, bareList }

struct Field {
    string name;
    Value value;
}

struct Value {
    ValueKind kind;
    string text;
    Value[] items;
    Field[] fields;

    static Value scalar(string s) { return Value(ValueKind.scalar, s, null, null); }
    static Value str(string s) { return Value(ValueKind.stringLiteral, s, null, null); }
    static Value number(string s) { return Value(ValueKind.number, s, null, null); }
    static Value boolean(bool b) { return Value(ValueKind.boolean, b ? "true" : "false", null, null); }
    static Value array(Value[] x) { return Value(ValueKind.array, "", x, null); }
    static Value object(Field[] x) { return Value(ValueKind.object, "", null, x); }
    static Value bare(string[] xs) {
        Value[] x; foreach (s; xs) x ~= Value.scalar(s);
        return Value(ValueKind.bareList, "", x, null);
    }

    string asString(string fallback = "") const {
        if (kind == ValueKind.stringLiteral || kind == ValueKind.scalar ||
            kind == ValueKind.number || kind == ValueKind.boolean) return text;
        return fallback;
    }

    bool asBool(bool fallback = false) const {
        if (kind == ValueKind.boolean) return text == "true";
        return fallback;
    }

    Value* field(string n) {
        foreach (ref f; fields) if (f.name == n) return &f.value;
        return null;
    }

    const(Value)* fieldConst(string n) const {
        foreach (ref f; fields) if (f.name == n) if (f.name == n) return &f.value;
        return null;
    }
}

struct Declaration {
    string kind;
    string name;
    Field[] fields;
}

struct IsaDocument {
    Declaration[] declarations;
}

class ParseError : Exception {
    size_t offset;
    this(string msg, size_t off) { offset = off; super(msg); }
}

private struct Tok {
    enum Kind { ident, number, stringLiteral, lbrace, rbrace, lbracket, rbracket,
                lparen, rparen, equals, semicolon, comma, colon, eof }
    Kind kind;
    string text;
    size_t pos;
}

private class Lexer {
    string s; size_t p;
    this(string s) { this.s = s; }
    private bool identStart(char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
    }
    private bool identCont(char c) {
        return identStart(c) || (c >= '0' && c <= '9') || c == '.' || c == '-';
    }
    Tok next() {
        while (p < s.length) {
            char c = s[p];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { ++p; continue; }
            if (c == '#') { while (p < s.length && s[p] != '\n') ++p; continue; }
            break;
        }
        auto at = p;
        if (p == s.length) return Tok(Tok.Kind.eof, "", at);
        char c = s[p++];
        if (c == '{') return Tok(Tok.Kind.lbrace, "{", at);
        if (c == '}') return Tok(Tok.Kind.rbrace, "}", at);
        if (c == '[') return Tok(Tok.Kind.lbracket, "[", at);
        if (c == ']') return Tok(Tok.Kind.rbracket, "]", at);
        if (c == '(') return Tok(Tok.Kind.lparen, "(", at);
        if (c == ')') return Tok(Tok.Kind.rparen, ")", at);
        if (c == '=') return Tok(Tok.Kind.equals, "=", at);
        if (c == ';') return Tok(Tok.Kind.semicolon, ";", at);
        if (c == ',') return Tok(Tok.Kind.comma, ",", at);
        if (c == ':') return Tok(Tok.Kind.colon, ":", at);
        if (c == '"') {
            auto b = appender!string();
            while (p < s.length) {
                c = s[p++];
                if (c == '"') return Tok(Tok.Kind.stringLiteral, b.data, at);
                if (c == '\\' && p < s.length) {
                    char e = s[p++];
                    final switch (e) {
                    case 'n': b.put('\n'); break; case 'r': b.put('\r'); break;
                    case 't': b.put('\t'); break; default: b.put(e); break;
                    }
                } else b.put(c);
            }
            throw new ParseError("unterminated string", at);
        }
        if (identStart(c) || c == '$' || c == '%') {
            size_t q = p - 1;
            while (p < s.length && identCont(s[p])) ++p;
            return Tok(Tok.Kind.ident, s[q .. p].idup, at);
        }
        if ((c >= '0' && c <= '9') || c == '-') {
            size_t q = p - 1;
            while (p < s.length) {
                char d = s[p];
                if (!((d >= '0' && d <= '9') || (d >= 'a' && d <= 'f') ||
                      (d >= 'A' && d <= 'F') || d == 'x' || d == 'X' || d == '_')) break;
                ++p;
            }
            return Tok(Tok.Kind.number, s[q .. p].idup, at);
        }
        throw new ParseError("unexpected character '" ~ c ~ "'", at);
    }
}

class IsaParser {
    private Lexer lex; private Tok cur;
    this(string source) { lex = new Lexer(source); cur = lex.next(); }

    private void take(Tok.Kind k) {
        if (cur.kind != k)
            throw new ParseError("expected " ~ k.to!string ~ ", got '" ~ cur.text ~ "'", cur.pos);
        cur = lex.next();
    }

    private string id() {
        if (cur.kind != Tok.Kind.ident && cur.kind != Tok.Kind.number)
            throw new ParseError("expected identifier", cur.pos);
        auto s = cur.text; cur = lex.next(); return s;
    }

    private Value value() {
        if (cur.kind == Tok.Kind.stringLiteral) { auto s=cur.text; cur=lex.next(); return Value.str(s); }
        if (cur.kind == Tok.Kind.number) { auto s=cur.text; cur=lex.next(); return Value.number(s); }
        if (cur.kind == Tok.Kind.ident) {
            auto s=cur.text; cur=lex.next();
            if (s == "true" || s == "false") return Value.boolean(s == "true");
            if (cur.kind == Tok.Kind.lparen) {
                auto b = appender!string(); b.put(s); b.put("("); cur=lex.next();
                int depth=1;
                while (depth && cur.kind != Tok.Kind.eof) {
                    if (cur.kind == Tok.Kind.lparen) { ++depth; b.put("("); }
                    else if (cur.kind == Tok.Kind.rparen) {
                        --depth; if (!depth) { cur=lex.next(); break; } b.put(")");
                    } else { b.put(cur.text); }
                    cur=lex.next();
                }
                return Value.scalar(b.data);
            }
            return Value.scalar(s);
        }
        if (cur.kind == Tok.Kind.lbracket) {
            take(Tok.Kind.lbracket); Value[] xs;
            while (cur.kind != Tok.Kind.rbracket) {
                xs ~= value();
                if (cur.kind == Tok.Kind.comma) take(Tok.Kind.comma);
                else if (cur.kind != Tok.Kind.rbracket)
                    throw new ParseError("expected ',' or ']'", cur.pos);
            }
            take(Tok.Kind.rbracket); return Value.array(xs);
        }
        if (cur.kind == Tok.Kind.lbrace) return object();
        throw new ParseError("expected value", cur.pos);
    }

    private Value object() {
        take(Tok.Kind.lbrace); Field[] fs;
        while (cur.kind != Tok.Kind.rbrace) fs ~= blockField();
        take(Tok.Kind.rbrace);
        return Value.object(fs);
    }

    private Value bareAfter(string first) {
        string[] xs; string acc = first;
        while (cur.kind != Tok.Kind.semicolon && cur.kind != Tok.Kind.eof) {
            if (cur.kind == Tok.Kind.comma) {
                if (acc.length) xs ~= acc;
                acc = ""; cur=lex.next(); continue;
            }
            if (cur.kind == Tok.Kind.colon) { acc ~= ":"; cur=lex.next(); continue; }
            acc ~= " " ~ cur.text; cur=lex.next();
        }
        if (acc.length) xs ~= acc;
        return Value.bare(xs);
    }

    private Field blockField() {
        auto n=id();
        if (cur.kind == Tok.Kind.lbrace) {
            auto v=object();
            if (cur.kind == Tok.Kind.semicolon) take(Tok.Kind.semicolon);
            return Field(n,v);
        }
        take(Tok.Kind.equals);
        Value v;
        if (cur.kind == Tok.Kind.lbrace || cur.kind == Tok.Kind.lbracket ||
            cur.kind == Tok.Kind.stringLiteral) {
            v=value();
        } else if (cur.kind == Tok.Kind.number) {
            auto first=cur.text; cur=lex.next();
            v = cur.kind == Tok.Kind.semicolon ? Value.number(first) : bareAfter(first);
        } else {
            auto first=id();
            if (cur.kind == Tok.Kind.semicolon) {
                v = (first == "true" || first == "false")
                    ? Value.boolean(first == "true") : Value.scalar(first);
            } else v=bareAfter(first);
        }
        take(Tok.Kind.semicolon);
        return Field(n,v);
    }

    private Declaration regClassDecl(string k) {
        auto n=id(); take(Tok.Kind.lbrace);
        Field[] fs;
        while (cur.kind != Tok.Kind.rbrace) {
            auto rn=id(); take(Tok.Kind.lparen); auto w=id(); take(Tok.Kind.rparen);
            take(Tok.Kind.equals); auto idx=id();
            fs ~= Field(rn, Value.object([
                Field("width", Value.number(w)), Field("index", Value.scalar(idx))
            ]));
            if (cur.kind == Tok.Kind.comma) take(Tok.Kind.comma);
            else if (cur.kind == Tok.Kind.semicolon) take(Tok.Kind.semicolon);
        }
        take(Tok.Kind.rbrace);
        return Declaration(k,n,fs);
    }

    private Declaration decl() {
        auto k=id();
        if (k == "alias") {
            auto n=id(); take(Tok.Kind.equals);
            auto v=value(); take(Tok.Kind.semicolon);
            return Declaration(k,n,[Field("value",v)]);
        }
        if (k == "regclass") return regClassDecl(k);
        string n="";
        if (cur.kind == Tok.Kind.ident) {
            // Declarations with a name: arch/encoding/op. profile/compiler are anonymous.
            if (k == "arch" || k == "encoding" || k == "op") n=id();
        }
        take(Tok.Kind.lbrace);
        Field[] fs;
        while (cur.kind != Tok.Kind.rbrace) fs ~= blockField();
        take(Tok.Kind.rbrace);
        return Declaration(k,n,fs);
    }

    IsaDocument parse() {
        IsaDocument d;
        while (cur.kind != Tok.Kind.eof) d.declarations ~= decl();
        return d;
    }
}

IsaDocument parseISA(string source) {
    return (new IsaParser(source)).parse();
}
