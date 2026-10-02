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
    size_t offset;

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

    string toString() const {
        if(kind==ValueKind.stringLiteral) {
            import std.string : replace;
            return `"` ~ text.replace(`\`,`\\`).replace(`"`,`\"`).replace("\n",`\n`).replace("\r",`\r`).replace("\t",`\t`) ~ `"`;
        }
        if(kind==ValueKind.object) {
            string s="{"; foreach(f;fields)s~=f.name~"="~f.value.toString()~";"; return s~"}";
        }
        if(kind==ValueKind.array||kind==ValueKind.bareList) {
            string s=kind==ValueKind.array?"[":"";
            foreach(i,x;items){if(i)s~=",";s~=x.toString();}
            return s~(kind==ValueKind.array?"]":"");
        }
        return text;
    }

    string asString(string fallback = "") const {
        if (kind == ValueKind.stringLiteral || kind == ValueKind.scalar ||
            kind == ValueKind.number || kind == ValueKind.boolean) return text;
        if (kind == ValueKind.bareList && items.length == 1) return items[0].asString(fallback);
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
    size_t offset;
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
                    switch (e) {
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
    private size_t nesting;
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
        auto position=cur.pos;
        if(++nesting>256)throw new ParseError("ISA nesting limit exceeded",position);
        scope(exit)--nesting;
        auto v=valueImpl();v.offset=position;return v;
    }

    private Value valueImpl() {
        if (cur.kind == Tok.Kind.lparen) {
            auto begin=cur.pos;
            auto b=appender!string(); int depth;
            do {
                if (cur.kind==Tok.Kind.eof) throw new ParseError("unterminated S-expression",begin);
                if (cur.kind==Tok.Kind.lparen) { ++depth; b.put("("); }
                else if (cur.kind==Tok.Kind.rparen) { --depth; b.put(") "); }
                else if (cur.kind==Tok.Kind.stringLiteral) {
                    b.put(Value.str(cur.text).toString()); b.put(" ");
                } else { b.put(cur.text); b.put(" "); }
                cur=lex.next();
            } while(depth);
            return Value.scalar(parseSExpr(b.data).toString());
        }
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
                if (depth) throw new ParseError("unterminated call value",cur.pos);
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
        while (cur.kind != Tok.Kind.rbrace) addField(fs,blockField());
        take(Tok.Kind.rbrace);
        return Value.object(fs);
    }

    private Value bareAfter(string first) {
        string[] xs; string acc = first; size_t parentheses;
        while (cur.kind != Tok.Kind.semicolon && cur.kind != Tok.Kind.eof) {
            if(cur.kind==Tok.Kind.rbrace)throw new ParseError("expected ';'",cur.pos);
            if(cur.kind==Tok.Kind.lparen) { ++parentheses;acc~="(";cur=lex.next();continue; }
            if(cur.kind==Tok.Kind.rparen) {
                if(!parentheses)throw new ParseError("unexpected ')'",cur.pos);
                --parentheses;acc~=")";cur=lex.next();continue;
            }
            if (cur.kind == Tok.Kind.comma) {
                if(parentheses){acc~=",";cur=lex.next();continue;}
                if (acc.length) xs ~= acc;
                acc = ""; cur=lex.next(); continue;
            }
            if (cur.kind == Tok.Kind.colon) { acc ~= ":"; cur=lex.next(); continue; }
            acc ~= " " ~ cur.text; cur=lex.next();
        }
        if(parentheses)throw new ParseError("unterminated call value",cur.pos);
        if (acc.length) xs ~= acc;
        return Value.bare(xs);
    }

    private Field blockField() {
        auto n=id();
        if(cur.kind==Tok.Kind.lbracket) {
            take(Tok.Kind.lbracket);auto high=cur.text;take(Tok.Kind.number);
            take(Tok.Kind.colon);auto low=cur.text;take(Tok.Kind.number);take(Tok.Kind.rbracket);
            n~="["~high~":"~low~"]";
        }
        if (cur.kind == Tok.Kind.lbrace) {
            auto v=object();
            if (cur.kind == Tok.Kind.semicolon) take(Tok.Kind.semicolon);
            return Field(n,v);
        }
        take(Tok.Kind.equals);
        auto position=cur.pos;
        Value v;
        if (cur.kind == Tok.Kind.semicolon) v=Value.scalar("");
        else if (cur.kind == Tok.Kind.lbrace || cur.kind == Tok.Kind.lbracket || cur.kind == Tok.Kind.lparen ||
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
        if(v.kind!=ValueKind.object||cur.kind==Tok.Kind.semicolon)take(Tok.Kind.semicolon);
        v.offset=position;
        return Field(n,v);
    }

    private void addField(ref Field[] fields,Field field) {
        foreach(existing;fields)if(existing.name==field.name)throw new ParseError("duplicate field: "~field.name,field.value.offset);
        fields~=field;
    }

    private Declaration regClassDecl(string k) {
        auto n=id(); take(Tok.Kind.lbrace);
        Field[] fs;
        while (cur.kind != Tok.Kind.rbrace) {
            auto rn=id(); take(Tok.Kind.lparen); auto w=id(); take(Tok.Kind.rparen);
            take(Tok.Kind.equals); auto idx=id();
            addField(fs,Field(rn, Value.object([
                Field("width", Value.number(w)), Field("index", Value.scalar(idx))
            ])));
            if (cur.kind == Tok.Kind.comma) take(Tok.Kind.comma);
            else if (cur.kind == Tok.Kind.semicolon) take(Tok.Kind.semicolon);
        }
        take(Tok.Kind.rbrace);
        return Declaration(k,n,fs);
    }

    private Declaration decl() {
        auto position=cur.pos;
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
        while (cur.kind != Tok.Kind.rbrace) addField(fs,blockField());
        take(Tok.Kind.rbrace);
        return Declaration(k,n,fs,position);
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
