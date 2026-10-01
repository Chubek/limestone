// lua_format.cpp — parse a Lua-subset file and re-emit it formatted.
//
//   dsltk_generate lua_subset.g -o lua_parser.hpp
//   g++ -std=c++17 -O2 lua_format.cpp -o lua_format
//   ./lua_format input.lua            (or:  ./lua_format < input.lua)

#include "lua_parser.hpp"

#include <any>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Node = std::map<std::string, std::any>;
using List = std::vector<std::any>;

const int kIndentWidth = 4;

bool isNode(const std::any &a)  { return a.type() == typeid(Node); }
bool isList(const std::any &a)  { return a.type() == typeid(List); }

const Node &asNode(const std::any &a) { return *std::any_cast<Node>(&a); }
const List &asList(const std::any &a) { return *std::any_cast<List>(&a); }

std::string kindOf(const std::any &a) {
    if (!isNode(a)) return {};
    const Node &n = asNode(a);
    auto it = n.find("kind");
    if (it == n.end()) return {};
    if (auto *s = std::any_cast<std::string>(&it->second)) return *s;
    return {};
}

std::string opOf(const Node &n) {
    auto it = n.find("op");
    if (it == n.end()) return {};
    return std::any_cast<std::string>(it->second);
}

// ── Expression precedence (mirrors the grammar layering) ──────────────────

int precOf(const std::string &op) {
    if (op == "or")  return 1;
    if (op == "and") return 2;
    if (op == "==" || op == "~=" || op == "<" ||
        op == ">"  || op == "<=" || op == ">=") return 3;
    if (op == "+" || op == "-" || op == "..")   return 4;
    if (op == "*" || op == "/" ||
        op == "//"|| op == "%")                 return 5;
    return 0;
}
const int kUnaryPrec = 6;

// Precedence of an already-built expression node (0 = atom, never wrapped).
int exprPrec(const std::any &e) {
    if (!isNode(e)) return 0;
    const Node &n = asNode(e);
    auto it = n.find("op");
    if (it == n.end()) return 0;                 // call_expr / variable
    std::string op = std::any_cast<std::string>(it->second);
    return n.count("l") ? precOf(op) : kUnaryPrec;
}

std::string formatNumber(double d) {
    if (std::floor(d) == d && std::fabs(d) < 1e15) {
        std::ostringstream os;
        os << static_cast<std::int64_t>(d);
        return os.str();
    }
    std::ostringstream os;
    os.precision(14);
    os << d;
    return os.str();
}

std::string formatExpr(const std::any &e);

std::string formatChild(const std::any &child, int parentPrec) {
    std::string s = formatExpr(child);
    int p = exprPrec(child);
    if (p != 0 && p < parentPrec) return "(" + s + ")";
    return s;
}

std::string formatExprList(const List &xs) {
    std::string out;
    for (std::size_t i = 0; i < xs.size(); ++i) {
        if (i) out += ", ";
        out += formatExpr(xs[i]);
    }
    return out;
}

std::string formatExpr(const std::any &e) {
    if (!e.has_value())                         return "nil";
    if (auto *b = std::any_cast<bool>(&e))      return *b ? "true" : "false";
    if (auto *d = std::any_cast<double>(&e))    return formatNumber(*d);
    if (auto *s = std::any_cast<std::string>(&e)) return *s;   // quoted string literal

    // Table constructor: a plain vector of fields.
    if (isList(e)) {
        const List &fields = asList(e);
        if (fields.empty()) return "{}";
        std::string out = "{ ";
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (i) out += ", ";
            const std::any &f = fields[i];
            // A field node has "key"/"value" but no "kind"/"op".
            if (isNode(f)) {
                const Node &fn = asNode(f);
                if (fn.count("key")) {
                    out += std::any_cast<std::string>(fn.at("key"));
                    out += " = ";
                    out += formatExpr(fn.at("value"));
                    continue;
                }
            }
            out += formatExpr(f);
        }
        return out + " }";
    }

    const Node &n = asNode(e);
    std::string kind = kindOf(e);

    if (kind == "variable")
        return std::any_cast<std::string>(n.at("name"));

    if (kind == "call_expr" || kind == "call") {
        std::string out = std::any_cast<std::string>(
            n.count("func") ? n.at("func") : n.at("name"));
        out += "(";
        out += formatExprList(asList(n.at("args")));
        out += ")";
        return out;
    }

    std::string op = opOf(n);
    if (!op.empty()) {
        if (!n.count("l")) {                    // unary
            std::string rhs = formatChild(n.at("r"), kUnaryPrec);
            if (op == "not") return "not " + rhs;
            return op + rhs;
        }
        int p = precOf(op);
        std::string sep = (op == "..") ? " .. " : " " + op + " ";
        return formatChild(n.at("l"), p) + sep + formatChild(n.at("r"), p);
    }

    return "--[[?expr]]";
}

// ── Statements ─────────────────────────────────────────────────────────────

void formatBlock(const List &stmts, int indent, std::string &out);

std::string pad(int indent) { return std::string(indent * kIndentWidth, ' '); }

void formatStmt(const std::any &s, int indent, std::string &out) {
    // Bare vector = do-block (after the RetStat patch, return is a map node).
    if (isList(s)) {
        out += pad(indent) + "do\n";
        formatBlock(asList(s), indent + 1, out);
        out += pad(indent) + "end\n";
        return;
    }
    if (auto *str = std::any_cast<std::string>(&s)) {
        if (*str == "empty") return;            // stray ';' — drop it
        out += pad(indent) + *str + "\n";
        return;
    }

    const Node &n = asNode(s);
    std::string kind = kindOf(s);

    if (kind == "local") {
        out += pad(indent) + "local " + std::any_cast<std::string>(n.at("name"));
        if (n.at("init").has_value())
            out += " = " + formatExpr(n.at("init"));
        out += "\n";
    } else if (kind == "assign") {
        out += pad(indent) + std::any_cast<std::string>(n.at("name"))
             + " = " + formatExpr(n.at("value")) + "\n";
    } else if (kind == "call") {
        out += pad(indent) + std::any_cast<std::string>(n.at("func"))
             + "(" + formatExprList(asList(n.at("args"))) + ")\n";
    } else if (kind == "return") {
        const List &vals = asList(n.at("values"));
        out += pad(indent) + "return";
        if (!vals.empty()) out += " " + formatExprList(vals);
        out += "\n";
    } else if (kind == "while") {
        out += pad(indent) + "while " + formatExpr(n.at("cond")) + " do\n";
        formatBlock(asList(n.at("body")), indent + 1, out);
        out += pad(indent) + "end\n";
    } else if (kind == "repeat") {
        out += pad(indent) + "repeat\n";
        formatBlock(asList(n.at("body")), indent + 1, out);
        out += pad(indent) + "until " + formatExpr(n.at("cond")) + "\n";
    } else if (kind == "for") {
        out += pad(indent) + "for " + std::any_cast<std::string>(n.at("var"))
             + " = " + formatExpr(n.at("from")) + ", " + formatExpr(n.at("to"));
        // Suppress the default step of 1.
        const std::any &step = n.at("step");
        bool defaultStep = false;
        if (auto *d = std::any_cast<double>(&step)) defaultStep = (*d == 1.0);
        if (!defaultStep) out += ", " + formatExpr(step);
        out += " do\n";
        formatBlock(asList(n.at("body")), indent + 1, out);
        out += pad(indent) + "end\n";
    } else if (kind == "function") {
        out += pad(indent) + "function "
             + std::any_cast<std::string>(n.at("name")) + "(";
        const List &params = asList(n.at("params"));
        for (std::size_t i = 0; i < params.size(); ++i) {
            if (i) out += ", ";
            out += std::any_cast<std::string>(params[i]);
        }
        out += ")\n";
        formatBlock(asList(n.at("body")), indent + 1, out);
        out += pad(indent) + "end\n";
    } else if (kind == "if") {
        out += pad(indent) + "if " + formatExpr(n.at("cond")) + " then\n";
        formatBlock(asList(n.at("then")), indent + 1, out);
        // Walk the elseif chain.
        std::any tail = n.at("else");
        while (kindOf(tail) == "elseif") {
            const Node &e = asNode(tail);
            out += pad(indent) + "elseif " + formatExpr(e.at("cond")) + " then\n";
            formatBlock(asList(e.at("then")), indent + 1, out);
            tail = e.at("else");
        }
        if (tail.has_value() && isList(tail)) {
            out += pad(indent) + "else\n";
            formatBlock(asList(tail), indent + 1, out);
        }
        out += pad(indent) + "end\n";
    } else {
        out += pad(indent) + "--[[?stmt]]\n";
    }
}

void formatBlock(const List &stmts, int indent, std::string &out) {
    for (const std::any &s : stmts)
        formatStmt(s, indent, out);
}

} // namespace

int main(int argc, char **argv) {
    std::string source;
    if (argc > 1) {
        std::ifstream in(argv[1], std::ios::binary);
        if (!in) {
            std::cerr << "cannot open " << argv[1] << "\n";
            return 1;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        source = ss.str();
    } else {
        std::ostringstream ss;
        ss << std::cin.rdbuf();
        source = ss.str();
    }

    lua::SemanticValue yy;
    lua::ParseResult r = lua::parse(source, yy);   // adjust if your entry
                                                   // point is parse_Chunk(...)
    if (!r.ok) {
        std::cerr << "parse error at offset " << r.pos << "\n";
        return 1;
    }

    std::string out;
    formatBlock(std::any_cast<List>(yy.value), 0, out);
    std::cout << out;
    return 0;
}
