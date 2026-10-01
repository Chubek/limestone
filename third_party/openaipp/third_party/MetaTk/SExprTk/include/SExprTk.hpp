/*
 * SExprTk.hpp
 *
 * Header-only s-expression toolkit: parser, serializer, XAS event
 * streaming, analyzer/transformer pass framework, and semanticizer
 * kernels backed by real language runtimes (QaMRpp/Lua and S7/Scheme).
 *
 * A *semanticizer* is a class that gives MEANING to a parsed program:
 * it walks the parse tree (a Cartable) and evaluates it, producing a
 * result value. SExprTk ships two semanticizers:
 *
 *   - LuaKernelSemanticizer: meaning = evaluate the program with the
 *     QaMRpp Lua runtime. The tree is compiled to a Lua chunk.
 *   - S7KernelSemanticizer:  meaning = evaluate the program with the
 *     S7 Scheme interpreter. The tree is already Scheme source.
 *
 * Both are also Kernels (a Kernel wraps a semanticizer so it can be
 * used in the generic `SExprTk::run(source, kernel)` pipeline).
 *
 * Analyzers and Transformers are abstract base classes from which
 * concrete passes derive (e.g. an e-graph optimizer combines an
 * analyzer that extracts equalities with a transformer that rewrites
 * the tree to a canonical form).
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

extern "C" {
#include "SExprTk-XASEvent.h"
}

#ifdef SEXPRTK_WITH_LUA
/* QaMRpp references the minizip namespace (from SerdeTk's MiniZIP.hpp)
 * in its podlet-archive loader but does not include it itself, so it
 * must be visible before QaMRpp.hpp is pulled in. */
#include <MiniZIP.hpp>
#include <QaMRpp.hpp>
#endif

#ifdef SEXPRTK_WITH_S7
#include <s7.h>
#endif

namespace sexprtk {

struct Atom;
struct Cell;
struct List;
struct Cartable;
struct Iterator;
struct Source;
struct Serializer;
struct Package;
struct PackageManifest;
struct XASEvent;
struct XASEventDispatcher;
struct CartableDispatcher;
class Analyzer;
class Transformer;
class Semanticizer;
class Kernel;
class LuaKernelSemanticizer;
class S7KernelSemanticizer;
class LuaKernel;
class S7Kernel;
class SExprTk;

enum class NodeKind : std::uint8_t {
    Nil, Bool, Integer, Float, String, Symbol, List
};

inline std::string_view to_string(NodeKind k) {
    switch (k) {
    case NodeKind::Nil:     return "nil";
    case NodeKind::Bool:    return "bool";
    case NodeKind::Integer: return "integer";
    case NodeKind::Float:   return "float";
    case NodeKind::String:  return "string";
    case NodeKind::Symbol:  return "symbol";
    case NodeKind::List:    return "list";
    }
    return "unknown";
}

struct List {
    std::vector<Cell> cells {};

    void push(Cell c);
    void pop();
    bool empty() const { return cells.empty(); }
    std::size_t size() const { return cells.size(); }
    Cell& front() { return cells.front(); }
    const Cell& front() const { return cells.front(); }
    Cell& back() { return cells.back(); }
    const Cell& back() const { return cells.back(); }
    Cell& operator[](std::size_t i) { return cells[i]; }
    const Cell& operator[](std::size_t i) const { return cells[i]; }
};

struct Atom {
    using ListPtr = std::shared_ptr<List>;
    using Value = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, ListPtr>;

    Value value {nullptr};
    NodeKind kind {NodeKind::Nil};

    Atom() = default;
    Atom(std::nullptr_t) : value(nullptr), kind(NodeKind::Nil) {}
    Atom(bool v) : value(v), kind(NodeKind::Bool) {}
    Atom(std::int64_t v) : value(v), kind(NodeKind::Integer) {}
    Atom(double v) : value(v), kind(NodeKind::Float) {}
    Atom(std::string v, NodeKind k = NodeKind::String) : value(std::move(v)), kind(k) {}
    Atom(ListPtr v) : value(std::move(v)), kind(NodeKind::List) {}

    bool is_nil()    const { return kind == NodeKind::Nil; }
    bool is_bool()   const { return kind == NodeKind::Bool; }
    bool is_int()    const { return kind == NodeKind::Integer; }
    bool is_float()  const { return kind == NodeKind::Float; }
    bool is_string() const { return kind == NodeKind::String; }
    bool is_symbol() const { return kind == NodeKind::Symbol; }
    bool is_list()   const { return kind == NodeKind::List; }

    bool truthy() const {
        if (is_nil()) return false;
        if (is_bool()) return std::get<bool>(value);
        return true;
    }

    std::int64_t as_int() const {
        if (is_int()) return std::get<std::int64_t>(value);
        if (is_float()) return static_cast<std::int64_t>(std::get<double>(value));
        throw std::runtime_error("as_int on non-numeric atom");
    }

    double as_float() const {
        if (is_float()) return std::get<double>(value);
        if (is_int()) return static_cast<double>(std::get<std::int64_t>(value));
        throw std::runtime_error("as_float on non-numeric atom");
    }

    const std::string& as_string() const {
        if (is_string() || is_symbol()) return std::get<std::string>(value);
        throw std::runtime_error("as_string on non-string atom");
    }

    const List& as_list() const {
        if (is_list()) return *std::get<ListPtr>(value);
        throw std::runtime_error("as_list on non-list atom");
    }

    List& as_list() {
        if (is_list()) return *std::get<ListPtr>(value);
        throw std::runtime_error("as_list on non-list atom");
    }
};

struct Cell {
    Atom head {};
    std::vector<Cell> tail {};

    Cell() = default;
    Cell(Atom h) : head(std::move(h)) {}
    Cell(Atom h, std::vector<Cell> t) : head(std::move(h)), tail(std::move(t)) {}

    bool is_atom() const { return tail.empty() && !head.is_list(); }
    bool is_pair() const { return !tail.empty() && !head.is_list(); }
    bool is_list_cell() const { return head.is_list(); }

    const Cell& car() const { return head.is_list() ? std::get<Atom::ListPtr>(head.value)->cells.front() : *this; }
    std::vector<Cell> cdr() const { return tail; }
};

inline void List::push(Cell c) { cells.push_back(std::move(c)); }
inline void List::pop() { if (!cells.empty()) cells.pop_back(); }

struct Source {
    std::string name {};
    std::string text {};

    static Source from_string(std::string input, std::string name = "<memory>") {
        return {std::move(name), std::move(input)};
    }

    static Source from_file(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("cannot open source file: " + path.string());
        std::ostringstream ss;
        ss << in.rdbuf();
        return {path.string(), ss.str()};
    }
};

struct Serializer {
    static std::string escape(std::string_view text) {
        std::ostringstream out;
        for (char ch : text) {
            switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"':  out << "\\\""; break;
            case '\n': out << "\\n";  break;
            case '\r': out << "\\r";  break;
            case '\t': out << "\\t";  break;
            default:   out << ch;       break;
            }
        }
        return out.str();
    }

    static std::string unescape(std::string_view text) {
        std::ostringstream out;
        for (std::size_t i = 0; i < text.size(); ++i) {
            char ch = text[i];
            if (ch == '\\' && i + 1 < text.size()) {
                char esc = text[++i];
                switch (esc) {
                case 'n': out << '\n'; break;
                case 'r': out << '\r'; break;
                case 't': out << '\t'; break;
                case '\\': out << '\\'; break;
                case '"': out << '"'; break;
                default:  out << esc; break;
                }
            } else {
                out << ch;
            }
        }
        return out.str();
    }

    static std::string to_string(const Atom& atom);
    static std::string to_string(const Cell& cell);
    static std::string to_string(const List& list);
    static std::string to_json(const Atom& atom);
    static std::string to_json(const Cell& cell);
    static std::string to_json(const List& list);
    static std::string to_toml(const PackageManifest& manifest);
};

/* ------------------------------------------------------------------ */
/* XAS events (C++ side of the C protocol in SExprTk-XASEvent.h)       */
/* ------------------------------------------------------------------ */

struct XASEvent {
    sexprtk_xas_event_kind kind {SEXPRTK_XAS_EVENT_ATOM};
    std::uint64_t sequence {0};
    std::uint16_t line {0};
    std::uint16_t column {0};
    std::string payload {};
    std::string source {};

    /* Convert to the C protocol representation (borrows payload). */
    sexprtk_xas_event to_c(std::uint16_t source_id = 0) const {
        sexprtk_xas_event e;
        sexprtk_xas_event_init(&e);
        e.kind = static_cast<std::uint8_t>(kind);
        e.sequence = sequence;
        e.line = line;
        e.column = column;
        e.source_id = source_id;
        e.payload = payload.empty() ? nullptr : payload.c_str();
        e.payload_length = static_cast<std::uint32_t>(payload.size());
        return e;
    }

    static XASEvent from_c(const sexprtk_xas_event& e, std::string source_name = {}) {
        XASEvent out;
        out.kind = static_cast<sexprtk_xas_event_kind>(e.kind);
        out.sequence = e.sequence;
        out.line = e.line;
        out.column = e.column;
        if (e.payload && e.payload_length)
            out.payload.assign(e.payload, e.payload_length);
        out.source = std::move(source_name);
        return out;
    }
};

struct Cartable {
    List root {};
    std::map<std::string, std::string> metadata {};
    std::vector<XASEvent> events {};
    std::vector<std::string> errors {};

    std::string to_string() const { return Serializer::to_string(root); }
    std::string to_json() const { return Serializer::to_json(root); }
    bool ok() const { return errors.empty(); }
};

struct Iterator {
    const List* list {nullptr};
    std::size_t index {0};

    explicit Iterator(const List& l) : list(&l) {}

    bool done() const { return !list || index >= list->cells.size(); }
    const Cell& operator*() const { return list->cells[index]; }
    const Cell* operator->() const { return &list->cells[index]; }
    Iterator& operator++() { if (!done()) ++index; return *this; }
    Iterator begin() const { return Iterator(*list); }
    Iterator end() const { Iterator it(*list); it.index = list ? list->cells.size() : 0; return it; }
    bool operator==(const Iterator& o) const { return list == o.list && index == o.index; }
    bool operator!=(const Iterator& o) const { return !(*this == o); }
};

struct LazyStream {
    std::string buffer {};
    std::size_t pos {0};
    explicit LazyStream(std::string s = {}) : buffer(std::move(s)) {}

    bool empty() const { return pos >= buffer.size(); }
    std::optional<char> peek() const { if (pos < buffer.size()) return buffer[pos]; return std::nullopt; }
    std::optional<char> take() { if (pos < buffer.size()) return buffer[pos++]; return std::nullopt; }
    void append(std::string s) { buffer += std::move(s); }
};

struct PackageManifest {
    std::string name {"SExprTk"};
    std::string version {"0.1.0"};
    std::string entry {"main.sx"};
    std::map<std::string, std::string> fields {};

    std::string to_toml() const { return Serializer::to_toml(*this); }

    static PackageManifest from_toml(std::string_view text) {
        PackageManifest m;
        std::istringstream in{std::string(text)};
        std::string line;
        while (std::getline(in, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            auto trim = [](std::string& s) {
                std::size_t a = 0;
                while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
                std::size_t b = s.size();
                while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
                s = s.substr(a, b - a);
            };
            trim(key); trim(val);
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
            if (key == "name") m.name = val;
            else if (key == "version") m.version = val;
            else if (key == "entry") m.entry = val;
            else m.fields[key] = val;
        }
        return m;
    }
};

struct Package {
    PackageManifest manifest {};
    Cartable cartable {};
    std::map<std::string, std::string> metadata {};

    std::string to_toml() const {
        std::ostringstream out;
        out << manifest.to_toml();
        for (const auto& [k, v] : metadata) out << k << " = \"" << Serializer::escape(v) << "\"\n";
        return out.str();
    }
};

/* ------------------------------------------------------------------ */
/* XAS protocol: reference implementation of the C interface           */
/* ------------------------------------------------------------------ */

namespace xas {

inline const char* kind_name(int kind) {
    switch (kind) {
    case SEXPRTK_XAS_EVENT_DOCUMENT_BEGIN: return "document-begin";
    case SEXPRTK_XAS_EVENT_DOCUMENT_END:   return "document-end";
    case SEXPRTK_XAS_EVENT_LIST_BEGIN:     return "list-begin";
    case SEXPRTK_XAS_EVENT_LIST_END:       return "list-end";
    case SEXPRTK_XAS_EVENT_ATOM:           return "atom";
    case SEXPRTK_XAS_EVENT_COMMENT:        return "comment";
    case SEXPRTK_XAS_EVENT_QUOTE:          return "quote";
    case SEXPRTK_XAS_EVENT_ERROR:          return "error";
    default:                               return "unknown";
    }
}

inline int kind_from_name(std::string_view name) {
    if (name == "document-begin") return SEXPRTK_XAS_EVENT_DOCUMENT_BEGIN;
    if (name == "document-end")   return SEXPRTK_XAS_EVENT_DOCUMENT_END;
    if (name == "list-begin")     return SEXPRTK_XAS_EVENT_LIST_BEGIN;
    if (name == "list-end")       return SEXPRTK_XAS_EVENT_LIST_END;
    if (name == "atom")           return SEXPRTK_XAS_EVENT_ATOM;
    if (name == "comment")        return SEXPRTK_XAS_EVENT_COMMENT;
    if (name == "quote")          return SEXPRTK_XAS_EVENT_QUOTE;
    if (name == "error")          return SEXPRTK_XAS_EVENT_ERROR;
    return -1;
}

inline bool kind_valid(int kind) {
    return kind >= SEXPRTK_XAS_EVENT_DOCUMENT_BEGIN && kind <= SEXPRTK_XAS_EVENT_ERROR;
}

/* Encode an event into a frame, following the wire format documented
 * in SExprTk-XASEvent.h. All multi-byte integers are big-endian. */
inline int frame_encode(const sexprtk_xas_event& ev, std::vector<std::uint8_t>& out) {
    if (!kind_valid(ev.kind)) return SEXPRTK_XAS_ERR_BAD_KIND;
    if (ev.payload_length > SEXPRTK_XAS_MAX_PAYLOAD) return SEXPRTK_XAS_ERR_TOO_LARGE;

    const std::uint32_t plen = ev.payload_length;
    out.clear();
    out.reserve(SEXPRTK_XAS_HEADER_SIZE + plen);
    out.push_back(SEXPRTK_XAS_MAGIC0);
    out.push_back(SEXPRTK_XAS_MAGIC1);
    out.push_back(SEXPRTK_XAS_VERSION);
    out.push_back(ev.flags);
    out.push_back(ev.kind);
    out.push_back(0); /* reserved */
    out.push_back(static_cast<std::uint8_t>(ev.source_id >> 8));
    out.push_back(static_cast<std::uint8_t>(ev.source_id & 0xFF));
    for (int shift = 56; shift >= 0; shift -= 8)
        out.push_back(static_cast<std::uint8_t>((ev.sequence >> shift) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(ev.line >> 8));
    out.push_back(static_cast<std::uint8_t>(ev.line & 0xFF));
    out.push_back(static_cast<std::uint8_t>(ev.column >> 8));
    out.push_back(static_cast<std::uint8_t>(ev.column & 0xFF));
    out.push_back(static_cast<std::uint8_t>((plen >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((plen >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((plen >> 8)  & 0xFF));
    out.push_back(static_cast<std::uint8_t>(plen & 0xFF));
    if (plen && ev.payload)
        out.insert(out.end(), ev.payload, ev.payload + plen);
    return SEXPRTK_XAS_OK;
}

inline int frame_validate(const std::uint8_t* bytes, std::size_t length) {
    if (!bytes) return SEXPRTK_XAS_ERR_INVALID;
    if (length < SEXPRTK_XAS_HEADER_SIZE) return SEXPRTK_XAS_ERR_TRUNCATED;
    if (bytes[0] != SEXPRTK_XAS_MAGIC0 || bytes[1] != SEXPRTK_XAS_MAGIC1)
        return SEXPRTK_XAS_ERR_BAD_MAGIC;
    if (bytes[2] != SEXPRTK_XAS_VERSION) return SEXPRTK_XAS_ERR_BAD_VERSION;
    if (!kind_valid(bytes[4])) return SEXPRTK_XAS_ERR_BAD_KIND;
    const std::uint32_t plen = (static_cast<std::uint32_t>(bytes[20]) << 24)
                             | (static_cast<std::uint32_t>(bytes[21]) << 16)
                             | (static_cast<std::uint32_t>(bytes[22]) << 8)
                             |  static_cast<std::uint32_t>(bytes[23]);
    if (length < SEXPRTK_XAS_HEADER_SIZE + plen) return SEXPRTK_XAS_ERR_TRUNCATED;
    return SEXPRTK_XAS_OK;
}

/* Decode a frame. The returned event's payload borrows from `bytes`,
 * which must outlive the event. */
inline int frame_decode(const std::uint8_t* bytes, std::size_t length, sexprtk_xas_event& ev) {
    const int status = frame_validate(bytes, length);
    if (status != SEXPRTK_XAS_OK) return status;

    sexprtk_xas_event_init(&ev);
    ev.flags = bytes[3];
    ev.kind  = bytes[4];
    ev.source_id = static_cast<std::uint16_t>((bytes[6] << 8) | bytes[7]);
    ev.sequence = 0;
    for (int i = 0; i < 8; ++i)
        ev.sequence = (ev.sequence << 8) | bytes[8 + i];
    ev.line   = static_cast<std::uint16_t>((bytes[16] << 8) | bytes[17]);
    ev.column = static_cast<std::uint16_t>((bytes[18] << 8) | bytes[19]);
    ev.payload_length = (static_cast<std::uint32_t>(bytes[20]) << 24)
                      | (static_cast<std::uint32_t>(bytes[21]) << 16)
                      | (static_cast<std::uint32_t>(bytes[22]) << 8)
                      |  static_cast<std::uint32_t>(bytes[23]);
    ev.payload = ev.payload_length
        ? reinterpret_cast<const char*>(bytes + SEXPRTK_XAS_HEADER_SIZE)
        : nullptr;
    return SEXPRTK_XAS_OK;
}

} // namespace xas

/* C-callable wrappers: these provide the reference implementation of
 * the interface declared in SExprTk-XASEvent.h. They are inline so the
 * header-only library can provide them without a separate TU. */

inline const char* sexprtk_xas_kind_name_impl(int kind) { return xas::kind_name(kind); }
inline int sexprtk_xas_kind_from_name_impl(const char* name) {
    return name ? xas::kind_from_name(name) : -1;
}
inline int sexprtk_xas_kind_valid_impl(int kind) { return xas::kind_valid(kind) ? 1 : 0; }

inline int sexprtk_xas_frame_encode_impl(const sexprtk_xas_event* event, sexprtk_xas_frame* frame) {
    if (!event || !frame || !frame->bytes) return SEXPRTK_XAS_ERR_INVALID;
    std::vector<std::uint8_t> tmp;
    const int status = xas::frame_encode(*event, tmp);
    if (status != SEXPRTK_XAS_OK) return status;
    if (frame->capacity < tmp.size()) return SEXPRTK_XAS_ERR_TOO_LARGE;
    std::memcpy(frame->bytes, tmp.data(), tmp.size());
    frame->length = tmp.size();
    return SEXPRTK_XAS_OK;
}

inline int sexprtk_xas_frame_decode_impl(const sexprtk_xas_frame* frame, sexprtk_xas_event* event) {
    if (!frame || !event) return SEXPRTK_XAS_ERR_INVALID;
    return xas::frame_decode(frame->bytes, frame->length, *event);
}

inline int sexprtk_xas_frame_validate_impl(const sexprtk_xas_frame* frame) {
    if (!frame) return SEXPRTK_XAS_ERR_INVALID;
    return xas::frame_validate(frame->bytes, frame->length);
}

inline std::uint32_t sexprtk_xas_frame_payload_length_impl(const sexprtk_xas_frame* frame) {
    if (!frame || xas::frame_validate(frame->bytes, frame->length) != SEXPRTK_XAS_OK) return 0;
    sexprtk_xas_event ev;
    if (xas::frame_decode(frame->bytes, frame->length, ev) != SEXPRTK_XAS_OK) return 0;
    return ev.payload_length;
}

inline void sexprtk_xas_event_init_impl(sexprtk_xas_event* event) {
    if (!event) return;
    event->sequence = 0;
    event->source_id = 0;
    event->line = 0;
    event->column = 0;
    event->kind = SEXPRTK_XAS_EVENT_ATOM;
    event->flags = 0;
    event->payload = nullptr;
    event->payload_length = 0;
}

inline void sexprtk_xas_frame_init_impl(sexprtk_xas_frame* frame) {
    if (!frame) return;
    frame->bytes = nullptr;
    frame->length = 0;
    frame->capacity = 0;
}

inline int sexprtk_xas_pump_impl(sexprtk_xas_event_source* source, sexprtk_xas_event_sink* sink) {
    if (!source || !source->next || !sink || !sink->handle) return SEXPRTK_XAS_ERR_INVALID;
    for (;;) {
        sexprtk_xas_event ev;
        sexprtk_xas_event_init(&ev);
        const int status = source->next(&ev, source->userdata);
        if (status == SEXPRTK_XAS_ERR_TRUNCATED) return SEXPRTK_XAS_OK; /* end of stream */
        if (status != SEXPRTK_XAS_OK) return status;
        const int sink_status = sink->handle(&ev, sink->userdata);
        if (sink_status < 0) return sink_status;
    }
}

} // namespace sexprtk

/* Bind the C interface names to the reference implementation. These
 * definitions satisfy the prototypes in SExprTk-XASEvent.h for any
 * C++ TU that includes this header. */
extern "C" {
inline const char* sexprtk_xas_event_kind_name(int kind) {
    return sexprtk::sexprtk_xas_kind_name_impl(kind);
}
inline int sexprtk_xas_event_kind_from_name(const char* name) {
    return sexprtk::sexprtk_xas_kind_from_name_impl(name);
}
inline int sexprtk_xas_event_kind_valid(int kind) {
    return sexprtk::sexprtk_xas_kind_valid_impl(kind);
}
inline int sexprtk_xas_frame_encode(const sexprtk_xas_event* event, sexprtk_xas_frame* frame) {
    return sexprtk::sexprtk_xas_frame_encode_impl(event, frame);
}
inline int sexprtk_xas_frame_decode(const sexprtk_xas_frame* frame, sexprtk_xas_event* event) {
    return sexprtk::sexprtk_xas_frame_decode_impl(frame, event);
}
inline int sexprtk_xas_frame_validate(const sexprtk_xas_frame* frame) {
    return sexprtk::sexprtk_xas_frame_validate_impl(frame);
}
inline std::uint32_t sexprtk_xas_frame_payload_length(const sexprtk_xas_frame* frame) {
    return sexprtk::sexprtk_xas_frame_payload_length_impl(frame);
}
inline int sexprtk_xas_pump(sexprtk_xas_event_source* source, sexprtk_xas_event_sink* sink) {
    return sexprtk::sexprtk_xas_pump_impl(source, sink);
}
inline void sexprtk_xas_event_init(sexprtk_xas_event* event) {
    sexprtk::sexprtk_xas_event_init_impl(event);
}
inline void sexprtk_xas_frame_init(sexprtk_xas_frame* frame) {
    sexprtk::sexprtk_xas_frame_init_impl(frame);
}
}

namespace sexprtk {

struct XASEventDispatcher {
    std::function<void(const XASEvent&)> sink {};
    std::vector<XASEvent> buffered {};

    void emit(const XASEvent& event) {
        buffered.push_back(event);
        if (sink) sink(buffered.back());
    }

    void clear() { buffered.clear(); }
    std::size_t size() const { return buffered.size(); }
    const XASEvent& front() const { return buffered.front(); }
    const XASEvent& back() const { return buffered.back(); }

    /* Adapt this dispatcher to the C sink interface. */
    sexprtk_xas_event_sink as_c_sink() {
        return {&XASEventDispatcher::c_sink_trampoline, this};
    }

private:
    static int c_sink_trampoline(const sexprtk_xas_event* e, void* userdata) {
        auto* self = static_cast<XASEventDispatcher*>(userdata);
        self->emit(XASEvent::from_c(*e));
        return SEXPRTK_XAS_OK;
    }
};

struct CartableDispatcher {
    XASEventDispatcher* dispatcher {nullptr};
    std::uint64_t seq {0};

    std::uint64_t emit(sexprtk_xas_event_kind kind, std::string payload = {},
                       std::uint16_t line = 0, std::uint16_t column = 0) {
        const std::uint64_t s = ++seq;
        if (dispatcher) {
            XASEvent e;
            e.kind = kind;
            e.sequence = s;
            e.line = line;
            e.column = column;
            e.payload = std::move(payload);
            dispatcher->emit(e);
        }
        return s;
    }

    std::uint64_t document_begin() { return emit(SEXPRTK_XAS_EVENT_DOCUMENT_BEGIN); }
    std::uint64_t document_end()   { return emit(SEXPRTK_XAS_EVENT_DOCUMENT_END); }
    std::uint64_t begin_list(std::string p = {}) { return emit(SEXPRTK_XAS_EVENT_LIST_BEGIN, std::move(p)); }
    std::uint64_t atom(std::string p = {})       { return emit(SEXPRTK_XAS_EVENT_ATOM, std::move(p)); }
    std::uint64_t end_list(std::string p = {})   { return emit(SEXPRTK_XAS_EVENT_LIST_END, std::move(p)); }
};

/* ------------------------------------------------------------------ */
/* Analyzer: abstract base class for program-analysis passes.          */
/*                                                                     */
/* An Analyzer inspects a Cartable (without modifying it) and          */
/* produces an AnalysisResult: arbitrary named facts about the         */
/* program. Concrete analyzers derive from this class and override     */
/* analyze(). Examples: symbol counting, free-variable discovery,      */
/* depth/weight metrics, equality extraction for an e-graph.           */
/* ------------------------------------------------------------------ */

struct AnalysisResult {
    std::map<std::string, std::string> facts {};
    std::vector<std::string> notes {};

    void set(std::string key, std::string value) { facts[std::move(key)] = std::move(value); }
    void set(std::string key, std::size_t value) { facts[std::move(key)] = std::to_string(value); }
    std::optional<std::string> get(std::string_view key) const {
        auto it = facts.find(std::string(key));
        if (it == facts.end()) return std::nullopt;
        return it->second;
    }
    std::size_t get_count(std::string_view key, std::size_t fallback = 0) const {
        auto v = get(key);
        if (!v) return fallback;
        return static_cast<std::size_t>(std::strtoull(v->c_str(), nullptr, 10));
    }
};

class Analyzer {
public:
    virtual ~Analyzer() = default;

    /* Inspect the program and return collected facts. Must not
     * modify the cartable. */
    virtual AnalysisResult analyze(const Cartable& cartable) const = 0;
    virtual std::string name() const = 0;
};

/* Concrete analyzer: basic shape metrics of the program tree. */
class ShapeAnalyzer : public Analyzer {
public:
    AnalysisResult analyze(const Cartable& cartable) const override {
        AnalysisResult r;
        r.set("atoms", count_atoms(cartable.root));
        r.set("lists", count_lists(cartable.root));
        r.set("depth", depth(cartable.root));
        return r;
    }
    std::string name() const override { return "shape"; }

    static std::size_t count_atoms(const List& list) {
        std::size_t n = 0;
        for (const auto& cell : list.cells) {
            if (cell.head.kind != NodeKind::List) ++n;
            else n += count_atoms(cell.head.as_list());
            n += count_atoms(cell.tail);
        }
        return n;
    }
    static std::size_t count_atoms(const std::vector<Cell>& tail) {
        std::size_t n = 0;
        for (const auto& c : tail) {
            if (c.head.kind != NodeKind::List) ++n;
            else n += count_atoms(c.head.as_list());
            n += count_atoms(c.tail);
        }
        return n;
    }
    static std::size_t count_lists(const List& list) {
        std::size_t n = 0;
        for (const auto& cell : list.cells) {
            if (cell.head.kind == NodeKind::List) { ++n; n += count_lists(cell.head.as_list()); }
            n += count_lists(cell.tail);
        }
        return n;
    }
    static std::size_t count_lists(const std::vector<Cell>& tail) {
        std::size_t n = 0;
        for (const auto& c : tail) {
            if (c.head.kind == NodeKind::List) { ++n; n += count_lists(c.head.as_list()); }
            n += count_lists(c.tail);
        }
        return n;
    }
    static std::size_t depth(const List& list) {
        std::size_t d = 1;
        for (const auto& cell : list.cells) {
            std::size_t local = 1;
            if (cell.head.kind == NodeKind::List) local += depth(cell.head.as_list()) + 1;
            if (!cell.tail.empty()) local += depth(cell.tail) + 1;
            d = std::max(d, local);
        }
        return d;
    }
    static std::size_t depth(const std::vector<Cell>& tail) {
        std::size_t d = 1;
        for (const auto& c : tail) {
            std::size_t local = 1;
            if (c.head.kind == NodeKind::List) local += depth(c.head.as_list()) + 1;
            if (!c.tail.empty()) local += depth(c.tail) + 1;
            d = std::max(d, local);
        }
        return d;
    }
};

/* Concrete analyzer: collects the set of distinct symbols used. */
class SymbolAnalyzer : public Analyzer {
public:
    AnalysisResult analyze(const Cartable& cartable) const override {
        AnalysisResult r;
        std::set<std::string> syms;
        collect(cartable.root, syms);
        r.set("unique-symbols", syms.size());
        std::ostringstream joined;
        bool first = true;
        for (const auto& s : syms) { if (!first) joined << ' '; joined << s; first = false; }
        r.set("symbols", joined.str());
        return r;
    }
    std::string name() const override { return "symbols"; }

    static bool has_symbol(const List& list, std::string_view sym) {
        for (const auto& cell : list.cells) {
            if (cell.head.is_symbol() && cell.head.as_string() == sym) return true;
            if (cell.head.is_list() && has_symbol(cell.head.as_list(), sym)) return true;
            if (has_symbol(cell.tail, sym)) return true;
        }
        return false;
    }
    static bool has_symbol(const std::vector<Cell>& tail, std::string_view sym) {
        for (const auto& c : tail) {
            if (c.head.is_symbol() && c.head.as_string() == sym) return true;
            if (c.head.is_list() && has_symbol(c.head.as_list(), sym)) return true;
            if (has_symbol(c.tail, sym)) return true;
        }
        return false;
    }

private:
    static void collect(const List& list, std::set<std::string>& out) {
        for (const auto& cell : list.cells) {
            if (cell.head.is_symbol()) out.insert(cell.head.as_string());
            else if (cell.head.is_list()) collect(cell.head.as_list(), out);
            collect(cell.tail, out);
        }
    }
    static void collect(const std::vector<Cell>& tail, std::set<std::string>& out) {
        for (const auto& c : tail) {
            if (c.head.is_symbol()) out.insert(c.head.as_string());
            else if (c.head.is_list()) collect(c.head.as_list(), out);
            collect(c.tail, out);
        }
    }
};

/* ------------------------------------------------------------------ */
/* Transformer: abstract base class for program-rewriting passes.      */
/*                                                                     */
/* A Transformer takes a Cartable and returns a new, rewritten one:    */
/* optimization, desugaring, canonicalization, partial evaluation.     */
/* Concrete transformers derive from this class and override           */
/* transform(). Combined with an Analyzer it can implement an          */
/* e-graph: the analyzer extracts equalities, the transformer applies  */
/* them as rewrites.                                                   */
/* ------------------------------------------------------------------ */

class Transformer {
public:
    virtual ~Transformer() = default;

    /* Rewrite the program. Must not modify the input in place. */
    virtual Cartable transform(const Cartable& cartable) const = 0;
    virtual std::string name() const = 0;
};

/* Concrete transformer: flattens nested lists into a single-level
 * list of atoms. */
class FlattenTransformer : public Transformer {
public:
    Cartable transform(const Cartable& cartable) const override {
        Cartable out = cartable;
        out.root = flatten(cartable.root);
        return out;
    }
    std::string name() const override { return "flatten"; }

    static List flatten(const List& list) {
        List out;
        for (const auto& c : list.cells) {
            if (c.head.is_list()) {
                auto inner = flatten(c.head.as_list());
                for (auto& x : inner.cells) out.push(std::move(x));
            } else {
                out.push(c);
            }
            if (!c.tail.empty()) {
                auto tail_cells = c.tail;
                auto tail = flatten(List{tail_cells});
                for (auto& x : tail.cells) out.push(std::move(x));
            }
        }
        return out;
    }
};

/* Concrete transformer: constant-folds integer arithmetic in
 * (+ ...) and (* ...) forms. */
class ConstantFoldTransformer : public Transformer {
public:
    Cartable transform(const Cartable& cartable) const override {
        Cartable out = cartable;
        out.root = fold_list(cartable.root);
        return out;
    }
    std::string name() const override { return "constant-fold"; }

private:
    static List fold_list(const List& list) {
        List out;
        for (const auto& c : list.cells) out.push(fold_cell(c));
        return out;
    }
    static Cell fold_cell(const Cell& c) {
        if (c.head.is_list()) {
            return Cell(Atom(std::make_shared<List>(fold_list(c.head.as_list()))));
        }
        if (c.head.is_symbol() && !c.tail.empty() &&
            (c.head.as_string() == "+" || c.head.as_string() == "*")) {
            const bool plus = c.head.as_string() == "+";
            std::int64_t acc = plus ? 0 : 1;
            std::vector<Cell> folded;
            for (const auto& arg : c.tail) folded.push_back(fold_cell(arg));
            const bool all_int = std::all_of(folded.begin(), folded.end(),
                [](const Cell& x) { return x.tail.empty() && x.head.is_int(); });
            if (all_int && !folded.empty()) {
                for (const auto& x : folded)
                    acc = plus ? acc + x.head.as_int() : acc * x.head.as_int();
                return Cell(Atom(acc));
            }
            return Cell(c.head, std::move(folded));
        }
        std::vector<Cell> folded_tail;
        for (const auto& t : c.tail) folded_tail.push_back(fold_cell(t));
        return Cell(c.head, std::move(folded_tail));
    }
};

/* Concrete transformer: maps a function over every cell. */
class MapTransformer : public Transformer {
public:
    using Fn = std::function<Cell(const Cell&)>;
    explicit MapTransformer(Fn fn, std::string name = "map")
        : fn_(std::move(fn)), name_(std::move(name)) {}

    Cartable transform(const Cartable& cartable) const override {
        Cartable out = cartable;
        out.root.cells = map_cells(cartable.root.cells);
        return out;
    }
    std::string name() const override { return name_; }

private:
    std::vector<Cell> map_cells(const std::vector<Cell>& cells) const {
        std::vector<Cell> out;
        out.reserve(cells.size());
        for (const auto& c : cells) out.push_back(fn_(c));
        return out;
    }
    Fn fn_;
    std::string name_;
};

/* ------------------------------------------------------------------ */
/* Semanticizer: gives MEANING to a program.                           */
/*                                                                     */
/* A Semanticizer takes a parsed Cartable and evaluates it, producing  */
/* a Semantics value: the meaning of the program. It is not a string   */
/* formatter — it runs the program through a runtime and reports what  */
/* the program denotes.                                                */
/* ------------------------------------------------------------------ */

struct Semantics {
    Atom value {};                  /* the denoted value            */
    std::string rendered {};        /* printable form of the value  */
    std::vector<std::string> errors {};
    bool ok() const { return errors.empty(); }

    explicit operator bool() const { return ok(); }
    std::string str() const { return rendered; }
};

class Semanticizer {
public:
    virtual ~Semanticizer() = default;

    /* Give meaning to the program: evaluate it and return its
     * denotation. */
    virtual Semantics semanticize(const Cartable& cartable) const = 0;
    virtual std::string name() const = 0;
};

/* ------------------------------------------------------------------ */
/* Kernel: a semanticizer adapted to the generic run() pipeline.       */
/*                                                                     */
/* Kernel is the base class; the concrete kernels below are thin       */
/* adapters over their corresponding semanticizers.                    */
/* ------------------------------------------------------------------ */

class Kernel {
public:
    virtual ~Kernel() = default;
    virtual Semantics evaluate(const Cartable& cartable) const = 0;
    virtual std::string name() const = 0;

    /* Legacy string entry point: the rendered denotation. */
    std::string run(const Cartable& cartable) const {
        return evaluate(cartable).rendered;
    }
};

/* ------------------------------------------------------------------ */
/* Tree -> source emission (shared by the runtime backends)            */
/* ------------------------------------------------------------------ */

namespace detail {

/* Emit one program form as scheme/lua-consumable source text. A
 * "program" is the root list; each top-level cell is one form. */
inline std::string emit_forms(const List& root) {
    std::ostringstream out;
    bool first = true;
    for (const auto& cell : root.cells) {
        if (!first) out << '\n';
        out << Serializer::to_string(cell);
        first = false;
    }
    return out.str();
}

/* Translate the program tree into a Lua chunk:
 *   (f a b)      -> f(a, b)
 *   atom         -> literal
 * Top-level forms become statements; the LAST form is returned. */
inline std::string emit_lua_expr(const Cell& cell);
inline std::string emit_lua_expr(const Atom& atom) {
    switch (atom.kind) {
    case NodeKind::Nil:     return "nil";
    case NodeKind::Bool:    return std::get<bool>(atom.value) ? "true" : "false";
    case NodeKind::Integer: return std::to_string(std::get<std::int64_t>(atom.value));
    case NodeKind::Float: {
        std::ostringstream ss;
        ss << std::get<double>(atom.value);
        return ss.str();
    }
    case NodeKind::String:  return "\"" + Serializer::escape(std::get<std::string>(atom.value)) + "\"";
    case NodeKind::Symbol:  return std::get<std::string>(atom.value);
    case NodeKind::List: {
        const auto& list = *std::get<Atom::ListPtr>(atom.value);
        if (list.empty()) return "{}";
        // call form: (f a b) -> f(a, b)
        std::ostringstream out;
        out << emit_lua_expr(list.cells.front());
        out << "(";
        for (std::size_t i = 1; i < list.size(); ++i) {
            if (i > 1) out << ", ";
            out << emit_lua_expr(list.cells[i]);
        }
        out << ")";
        return out.str();
    }
    }
    return "nil";
}
inline std::string emit_lua_expr(const Cell& cell) {
    if (cell.head.is_list()) return emit_lua_expr(cell.head);
    if (cell.tail.empty()) return emit_lua_expr(cell.head);
    // sugar cell (quote etc.): treat as a call
    std::ostringstream out;
    out << emit_lua_expr(cell.head) << "(";
    for (std::size_t i = 0; i < cell.tail.size(); ++i) {
        if (i) out << ", ";
        out << emit_lua_expr(cell.tail[i]);
    }
    out << ")";
    return out.str();
}

inline std::string emit_lua_chunk(const List& root) {
    std::ostringstream out;
    for (std::size_t i = 0; i < root.cells.size(); ++i) {
        const bool last = (i + 1 == root.cells.size());
        if (last) out << "return ";
        out << emit_lua_expr(root.cells[i]);
        if (!last) out << "\n";
    }
    if (root.cells.empty()) out << "return nil";
    return out.str();
}

} // namespace detail

/* ------------------------------------------------------------------ */
/* Lua kernel + semanticizer (QaMRpp runtime)                          */
/* ------------------------------------------------------------------ */

class LuaKernelSemanticizer : public Semanticizer {
public:
    Semantics semanticize(const Cartable& cartable) const override {
        Semantics sem;
#ifndef SEXPRTK_WITH_LUA
        (void)cartable;
        sem.errors.emplace_back(
            "LuaKernelSemanticizer requires SEXPRTK_WITH_LUA: rebuild with "
            "-DSEXPRTK_WITH_LUA=ON and the QaMRpp include path");
        return sem;
#else
        const std::string chunk = detail::emit_lua_chunk(cartable.root);
        try {
            qamrpp::Context ctx;
            auto result = ctx.run(chunk);
            sem.value = to_atom(result);
            sem.rendered = result ? result->to_string() : "nil";
        } catch (const std::exception& e) {
            sem.errors.emplace_back(std::string("lua: ") + e.what());
        }
        return sem;
#endif
    }
    std::string name() const override { return "lua-semanticizer"; }

#ifdef SEXPRTK_WITH_LUA
    /* Convert a QaMRpp value back into an SExprTk atom. */
    static Atom to_atom(const qamrpp::ValuePtr& v) {
        if (!v) return Atom{};
        switch (v->type) {
        case qamrpp::Value::NIL:    return Atom{};
        case qamrpp::Value::BOOL:   return Atom(v->bool_value);
        case qamrpp::Value::INT:    return Atom(static_cast<std::int64_t>(v->int_value));
        case qamrpp::Value::FLOAT:  return Atom(v->float_value);
        case qamrpp::Value::STRING: return Atom(v->string_value, NodeKind::String);
        default:                    return Atom(v->to_string(), NodeKind::Symbol);
        }
    }
#endif
};

class LuaKernel : public Kernel {
public:
    Semantics evaluate(const Cartable& cartable) const override {
        return semanticizer_.semanticize(cartable);
    }
    std::string name() const override { return "lua"; }
private:
    LuaKernelSemanticizer semanticizer_ {};
};

/* ------------------------------------------------------------------ */
/* S7 kernel + semanticizer (Scheme runtime)                           */
/* ------------------------------------------------------------------ */

class S7KernelSemanticizer : public Semanticizer {
public:
    Semantics semanticize(const Cartable& cartable) const override {
        Semantics sem;
#ifndef SEXPRTK_WITH_S7
        (void)cartable;
        sem.errors.emplace_back(
            "S7KernelSemanticizer requires SEXPRTK_WITH_S7: rebuild with "
            "-DSEXPRTK_WITH_S7=ON and the S7 include path/library");
        return sem;
#else
        const std::string forms = detail::emit_forms(cartable.root);
        s7_scheme* sc = s7_init();
        if (!sc) {
            sem.errors.emplace_back("s7: failed to initialize interpreter");
            return sem;
        }
        // The meaning of the program is the value of its last form;
        // wrap the whole document in a begin block and evaluate it.
        const std::string program = "(begin " + forms + ")";
        s7_pointer value = s7_eval_c_string(sc, program.c_str());
        char* text = s7_object_to_c_string(sc, value);
        if (text) {
            sem.rendered = text;
            free(text);
        } else {
            sem.rendered = "<#unspecified>";
        }
        sem.value = to_atom(sc, value);
        return sem;
#endif
    }
    std::string name() const override { return "s7-semanticizer"; }

#ifdef SEXPRTK_WITH_S7
    static Atom to_atom(s7_scheme* sc, s7_pointer v) {
        if (!v) return Atom{};
        if (s7_is_boolean(v)) return Atom(static_cast<bool>(s7_boolean(sc, v)));
        if (s7_is_integer(v)) return Atom(static_cast<std::int64_t>(s7_integer(v)));
        if (s7_is_real(v))    return Atom(static_cast<double>(s7_real(v)));
        if (s7_is_string(v))  return Atom(std::string(s7_string(v)), NodeKind::String);
        if (s7_is_symbol(v))  return Atom(std::string(s7_symbol_name(v)), NodeKind::Symbol);
        if (s7_is_null(sc, v)) return Atom{};
        if (s7_is_pair(v)) {
            auto list = std::make_shared<List>();
            s7_pointer p = v;
            while (s7_is_pair(p)) {
                list->push(Cell(to_atom(sc, s7_car(p))));
                p = s7_cdr(p);
            }
            return Atom(list);
        }
        char* text = s7_object_to_c_string(sc, v);
        std::string s = text ? text : "<#unknown>";
        if (text) free(text);
        return Atom(s, NodeKind::Symbol);
    }
#endif
};

class S7Kernel : public Kernel {
public:
    Semantics evaluate(const Cartable& cartable) const override {
        return semanticizer_.semanticize(cartable);
    }
    std::string name() const override { return "s7"; }
private:
    S7KernelSemanticizer semanticizer_ {};
};

/* ------------------------------------------------------------------ */
/* SExprTk: the top-level driver                                       */
/* ------------------------------------------------------------------ */

class SExprTk {
public:
    explicit SExprTk(std::string name = "SExprTk") : name_(std::move(name)) {}

    Cartable parse(const Source& source, XASEventDispatcher* dispatcher = nullptr) const {
        Parser p(source, dispatcher);
        Cartable c;
        c.root = p.parse_document();
        c.events = std::move(p.events);
        c.errors = std::move(p.errors);
        c.metadata["source"] = source.name;
        return c;
    }

    std::string run(const Source& source) const {
        return parse(source).to_string();
    }

    /* Run the program through a kernel (a semanticizer adapter):
     * returns the rendered denotation. */
    std::string run(const Source& source, const Kernel& kernel) const {
        return kernel.evaluate(parse(source)).rendered;
    }

    /* Semanticize the program directly: returns the full Semantics. */
    Semantics semanticize(const Source& source, const Semanticizer& sem) const {
        return sem.semanticize(parse(source));
    }

    std::string run(const Source& source, const Semanticizer& sem) const {
        return sem.semanticize(parse(source)).rendered;
    }

    /* Run an analysis pass over the parsed program. */
    AnalysisResult analyze(const Source& source, const Analyzer& analyzer) const {
        return analyzer.analyze(parse(source));
    }

    /* Run a transform pass over the parsed program. */
    Cartable transform(const Source& source, const Transformer& transformer) const {
        return transformer.transform(parse(source));
    }

    Package package(const Source& source, const PackageManifest& manifest = {}) const {
        Package pkg;
        pkg.manifest = manifest;
        pkg.cartable = parse(source);
        return pkg;
    }

    std::string name() const { return name_; }

private:
    struct Parser {
        std::string input;
        std::string source_name;
        std::size_t pos {0};
        XASEventDispatcher* dispatcher {nullptr};
        std::vector<XASEvent> events;
        std::vector<std::string> errors;
        std::uint64_t seq {0};
        std::size_t line {1};
        std::size_t column {1};

        Parser(const Source& source, XASEventDispatcher* d)
            : input(source.text), source_name(source.name), dispatcher(d) {}

        void error(std::string msg) {
            XASEvent e;
            e.kind = SEXPRTK_XAS_EVENT_ERROR;
            e.sequence = ++seq;
            e.line = static_cast<std::uint16_t>(line);
            e.column = static_cast<std::uint16_t>(column);
            e.payload = msg;
            e.source = source_name;
            events.push_back(e);
            if (dispatcher) dispatcher->emit(e);

            msg += " at " + source_name + ":" + std::to_string(line) + ":" + std::to_string(column);
            errors.push_back(std::move(msg));
        }

        void emit(sexprtk_xas_event_kind kind, std::string payload = {}) {
            XASEvent e;
            e.kind = kind;
            e.sequence = ++seq;
            e.line = static_cast<std::uint16_t>(line);
            e.column = static_cast<std::uint16_t>(column);
            e.payload = std::move(payload);
            e.source = source_name;
            events.push_back(e);
            if (dispatcher) dispatcher->emit(e);
        }

        void advance() {
            if (pos < input.size() && input[pos] == '\n') { ++line; column = 1; }
            else { ++column; }
            ++pos;
        }

        void skip_ws() {
            while (pos < input.size()) {
                unsigned char ch = static_cast<unsigned char>(input[pos]);
                if (std::isspace(ch)) { advance(); continue; }
                if (input[pos] == ';') {
                    std::string comment;
                    advance(); // consume ';'
                    while (pos < input.size() && input[pos] != '\n') {
                        comment.push_back(input[pos]);
                        advance();
                    }
                    emit(SEXPRTK_XAS_EVENT_COMMENT, comment);
                    continue;
                }
                break;
            }
        }

        bool eof() { skip_ws(); return pos >= input.size(); }

        std::string parse_token() {
            skip_ws();
            if (pos >= input.size()) return {};
            if (input[pos] == '"') {
                advance();
                std::ostringstream out;
                while (pos < input.size()) {
                    char ch = input[pos];
                    if (ch == '"') { advance(); break; }
                    if (ch == '\\' && pos + 1 < input.size()) {
                        advance();
                        char esc = input[pos];
                        advance();
                        switch (esc) {
                        case 'n': out << '\n'; break;
                        case 'r': out << '\r'; break;
                        case 't': out << '\t'; break;
                        case '\\': out << '\\'; break;
                        case '"': out << '"'; break;
                        default:  out << esc; break;
                        }
                    } else {
                        out << ch;
                        advance();
                    }
                }
                return "\"" + out.str() + "\"";
            }
            std::size_t start = pos;
            while (pos < input.size()) {
                char ch = input[pos];
                if (std::isspace(static_cast<unsigned char>(ch)) || ch == '(' || ch == ')' || ch == ';') break;
                advance();
            }
            return input.substr(start, pos - start);
        }

        Atom parse_atom(const std::string& tok) {
            if (tok.empty()) return Atom{};
            if (tok == "nil") return Atom{};
            if (tok == "#t" || tok == "true") return Atom(true);
            if (tok == "#f" || tok == "false") return Atom(false);
            if (tok.front() == '"' && tok.back() == '"' && tok.size() >= 2)
                return Atom(Serializer::unescape(tok.substr(1, tok.size() - 2)), NodeKind::String);
            if (tok.front() == ':') return Atom(std::string(tok), NodeKind::Symbol);

            char* end = nullptr;
            const long long i = std::strtoll(tok.c_str(), &end, 10);
            if (*end == '\0') return Atom(static_cast<std::int64_t>(i));
            char* endf = nullptr;
            const double d = std::strtod(tok.c_str(), &endf);
            if (*endf == '\0' && (tok.find_first_of(".eE") != std::string::npos || tok == "nan" || tok == "inf")) {
                if (tok == "nan") return Atom(std::numeric_limits<double>::quiet_NaN());
                if (tok == "inf") return Atom(std::numeric_limits<double>::infinity());
                return Atom(d);
            }
            return Atom(tok, NodeKind::Symbol);
        }

        Cell parse_cell() {
            skip_ws();
            if (pos >= input.size()) return {};
            if (input[pos] == '(') {
                advance();
                emit(SEXPRTK_XAS_EVENT_LIST_BEGIN, "(");
                Cell c;
                c.head = Atom(std::make_shared<List>());
                auto& list = c.head.as_list();
                while (true) {
                    skip_ws();
                    if (pos >= input.size()) { error("unterminated list"); break; }
                    if (input[pos] == ')') { advance(); break; }
                    list.push(parse_cell());
                }
                emit(SEXPRTK_XAS_EVENT_LIST_END, ")");
                return c;
            }
            if (input[pos] == ')') { error("unexpected ')'"); advance(); return {}; }
            if (input[pos] == '\'' || input[pos] == '`') {
                char quote = input[pos];
                advance();
                emit(SEXPRTK_XAS_EVENT_QUOTE, std::string(1, quote));
                Cell quoted = parse_cell();
                Cell wrapper;
                wrapper.head = Atom(std::string(quote == '\'' ? "quote" : "quasiquote"), NodeKind::Symbol);
                wrapper.tail.push_back(quoted);
                return wrapper;
            }
            const auto tok = parse_token();
            emit(SEXPRTK_XAS_EVENT_ATOM, tok);
            Cell c;
            c.head = parse_atom(tok);
            return c;
        }

        List parse_document() {
            emit(SEXPRTK_XAS_EVENT_DOCUMENT_BEGIN);
            List list;
            while (!eof()) {
                list.push(parse_cell());
            }
            emit(SEXPRTK_XAS_EVENT_DOCUMENT_END);
            return list;
        }
    };

    std::string name_;
};

inline std::string Serializer::to_string(const Atom& atom) {
    switch (atom.kind) {
    case NodeKind::Nil:     return "nil";
    case NodeKind::Bool:    return std::get<bool>(atom.value) ? "#t" : "#f";
    case NodeKind::Integer: return std::to_string(std::get<std::int64_t>(atom.value));
    case NodeKind::Float: {
        std::ostringstream ss;
        ss << std::get<double>(atom.value);
        return ss.str();
    }
    case NodeKind::String:  return "\"" + escape(std::get<std::string>(atom.value)) + "\"";
    case NodeKind::Symbol:  return std::get<std::string>(atom.value);
    case NodeKind::List:    return to_string(*std::get<Atom::ListPtr>(atom.value));
    }
    return "nil";
}

inline std::string Serializer::to_string(const Cell& cell) {
    if (cell.head.kind == NodeKind::List) return to_string(*std::get<Atom::ListPtr>(cell.head.value));
    if (cell.tail.empty()) return to_string(cell.head);
    std::ostringstream out;
    out << "(" << to_string(cell.head);
    for (const auto& sub : cell.tail) out << " " << to_string(sub);
    out << ")";
    return out.str();
}

inline std::string Serializer::to_string(const List& list) {
    std::ostringstream out;
    out << "(";
    for (std::size_t i = 0; i < list.cells.size(); ++i) {
        if (i) out << " ";
        out << to_string(list.cells[i]);
    }
    out << ")";
    return out.str();
}

inline std::string Serializer::to_json(const Atom& atom) {
    switch (atom.kind) {
    case NodeKind::Nil:     return "null";
    case NodeKind::Bool:    return std::get<bool>(atom.value) ? "true" : "false";
    case NodeKind::Integer: return std::to_string(std::get<std::int64_t>(atom.value));
    case NodeKind::Float: {
        std::ostringstream ss;
        ss << std::get<double>(atom.value);
        return ss.str();
    }
    case NodeKind::String:  return "\"" + escape(std::get<std::string>(atom.value)) + "\"";
    case NodeKind::Symbol:  return "\"" + escape(std::get<std::string>(atom.value)) + "\"";
    case NodeKind::List:    return to_json(*std::get<Atom::ListPtr>(atom.value));
    }
    return "null";
}

inline std::string Serializer::to_json(const Cell& cell) {
    if (cell.head.kind == NodeKind::List) return to_json(*std::get<Atom::ListPtr>(cell.head.value));
    if (cell.tail.empty()) return to_json(cell.head);
    std::ostringstream out;
    out << "{";
    out << "\"head\":" << to_json(cell.head);
    out << ",\"tail\":[";
    for (std::size_t i = 0; i < cell.tail.size(); ++i) {
        if (i) out << ",";
        out << to_json(cell.tail[i]);
    }
    out << "]}";
    return out.str();
}

inline std::string Serializer::to_json(const List& list) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < list.cells.size(); ++i) {
        if (i) out << ",";
        out << to_json(list.cells[i]);
    }
    out << "]";
    return out.str();
}

inline std::string Serializer::to_toml(const PackageManifest& manifest) {
    std::ostringstream out;
    out << "name = \"" << escape(manifest.name) << "\"\n";
    out << "version = \"" << escape(manifest.version) << "\"\n";
    out << "entry = \"" << escape(manifest.entry) << "\"\n";
    for (const auto& [k, v] : manifest.fields) {
        out << k << " = \"" << escape(v) << "\"\n";
    }
    return out.str();
}

} // namespace sexprtk
