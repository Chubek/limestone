#pragma once

/*
    RdvsgNG.hpp
    ----------
    Header-only RDVSG (Reduced-Ordered Directed Acyclic Graph) library
    with:
      - explicit node/edge graph representation
      - ordered child edges
      - optional translation to DSLUtils ASTs
      - optional translation to EquiNoxNG terms (adapter points)
      - simple rewrite rule container for RDVSG term-level transforms

    Notes:
      * "Reduced-Ordered" here means:
          - nodes may be shared
          - outgoing edges carry child positions
          - equivalent node reuse can be enabled with hash-consing hooks later
      * This implementation is intentionally conservative and self-contained.
      * If DSLUtils.hpp is available, this header exposes a bridge to dsl::ASTNode.
      * If EquiNoxNG.hpp is available, adapter stubs are provided where integration can be completed
        against the exact EquiNoxNG API in your environment.
*/

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#if __has_include("EquiNoxNG.hpp")
#  include "EquiNoxNG.hpp"
#  define RDVSG_HAS_EQUINOXNG 1
#else
#  define RDVSG_HAS_EQUINOXNG 0
#endif

#if __has_include("DSLUtils.hpp")
#  include "DSLUtils.hpp"
#  define RDVSG_HAS_DSLUTILS 1
#else
#  define RDVSG_HAS_DSLUTILS 0
#endif

namespace rdvsg {

// ============================================================
// IDs
// ============================================================

struct RdvsgNodeId {
    std::size_t value = 0;
    constexpr bool operator==(RdvsgNodeId const&) const noexcept = default;
    constexpr bool operator!=(RdvsgNodeId const&) const noexcept = default;
};

struct RdvsgEdgeId {
    std::size_t value = 0;
    constexpr bool operator==(RdvsgEdgeId const&) const noexcept = default;
    constexpr bool operator!=(RdvsgEdgeId const&) const noexcept = default;
};

// ============================================================
// Core value types
// ============================================================

struct RdvsgSymbol {
    std::string name;

    RdvsgSymbol() = default;
    RdvsgSymbol(std::string n) : name(std::move(n)) {}

    bool operator==(RdvsgSymbol const&) const noexcept = default;
};

struct RdvsgLiteral {
    using Value = std::variant<std::int64_t, double, bool, std::string>;
    Value value;

    RdvsgLiteral() = default;
    RdvsgLiteral(std::int64_t v) : value(v) {}
    RdvsgLiteral(double v) : value(v) {}
    RdvsgLiteral(bool v) : value(v) {}
    RdvsgLiteral(std::string v) : value(std::move(v)) {}
    RdvsgLiteral(char const* v) : value(std::string(v)) {}

    bool operator==(RdvsgLiteral const&) const noexcept = default;
};

enum class RdvsgNodeKind {
    Operator,
    Literal,
    Variable
};

// ============================================================
// Forward declarations
// ============================================================

struct RdvsgNode;
struct RdvsgEdge;
struct RdvsgTerm;

template <typename T>
struct RdvsgHasher;

// ============================================================
// Hash helpers
// ============================================================

namespace detail {

template <class T, class = void>
struct has_std_hash : std::false_type {};

template <class T>
struct has_std_hash<T, std::void_t<decltype(std::hash<T>{}(std::declval<T const&>()))>> : std::true_type {};

template <class T>
inline std::size_t hash_value(T const& v) noexcept {
    if constexpr (has_std_hash<T>::value) {
        return std::hash<T>{}(v);
    } else {
        return RdvsgHasher<T>{}(v);
    }
}

template <class T>
inline void hash_combine_inplace(std::size_t& seed, T const& v) noexcept {
    std::size_t h = hash_value(v);
    seed ^= h + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
}

} // namespace detail

} // namespace rdvsg

namespace std {
template <>
struct hash<rdvsg::RdvsgNodeId> {
    std::size_t operator()(rdvsg::RdvsgNodeId const& id) const noexcept {
        return std::hash<std::size_t>{}(id.value);
    }
};
template <>
struct hash<rdvsg::RdvsgEdgeId> {
    std::size_t operator()(rdvsg::RdvsgEdgeId const& id) const noexcept {
        return std::hash<std::size_t>{}(id.value);
    }
};
} // namespace std

namespace rdvsg {

// ============================================================
// Stringification
// ============================================================

inline std::string rdvsg_literal_to_string(RdvsgLiteral const& lit) {
    return std::visit([](auto const& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return "\"" + v + "\"";
        } else if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        } else {
            return std::to_string(v);
        }
    }, lit.value);
}

// ============================================================
// Term representation
// ============================================================

struct RdvsgTerm {
    struct Var {
        std::string name;
        bool operator==(Var const&) const noexcept = default;
    };

    struct Node {
        RdvsgSymbol op;
        std::vector<RdvsgTerm> children;
        bool operator==(Node const&) const noexcept = default;
    };

    using Repr = std::variant<Var, RdvsgLiteral, Node>;
    Repr repr;

    RdvsgTerm() = default;
    explicit RdvsgTerm(Var v) : repr(std::move(v)) {}
    explicit RdvsgTerm(RdvsgLiteral l) : repr(std::move(l)) {}
    explicit RdvsgTerm(Node n) : repr(std::move(n)) {}

    static RdvsgTerm var(std::string name) {
        return RdvsgTerm{Var{std::move(name)}};
    }

    static RdvsgTerm lit(RdvsgLiteral v) {
        return RdvsgTerm{std::move(v)};
    }

    static RdvsgTerm op(std::string name, std::vector<RdvsgTerm> args = {}) {
        return RdvsgTerm{Node{RdvsgSymbol{std::move(name)}, std::move(args)}};
    }

    bool is_var() const noexcept { return std::holds_alternative<Var>(repr); }
    bool is_lit() const noexcept { return std::holds_alternative<RdvsgLiteral>(repr); }
    bool is_node() const noexcept { return std::holds_alternative<Node>(repr); }

    Var const& as_var() const { return std::get<Var>(repr); }
    RdvsgLiteral const& as_lit() const { return std::get<RdvsgLiteral>(repr); }
    Node const& as_node() const { return std::get<Node>(repr); }
};

inline std::string to_string(RdvsgTerm const& t) {
    if (t.is_var()) return "?" + t.as_var().name;
    if (t.is_lit()) return rdvsg_literal_to_string(t.as_lit());

    auto const& n = t.as_node();
    std::ostringstream oss;
    oss << "(" << n.op.name;
    for (auto const& c : n.children) {
        oss << " " << to_string(c);
    }
    oss << ")";
    return oss.str();
}

// ============================================================
// Graph representation
// ============================================================

struct RdvsgNode {
    RdvsgNodeKind kind = RdvsgNodeKind::Operator;
    RdvsgSymbol symbol;
    std::vector<RdvsgEdgeId> incoming_edges;
    std::vector<RdvsgEdgeId> outgoing_edges;
    std::optional<RdvsgLiteral> literal;
    std::optional<std::string> variable_name;

    bool operator==(RdvsgNode const&) const noexcept = default;
};

struct RdvsgEdge {
    RdvsgNodeId source;
    RdvsgNodeId target;
    std::size_t position = 0;

    bool operator==(RdvsgEdge const&) const noexcept = default;
};

// ============================================================
// Hashers
// ============================================================

template <typename T>
struct RdvsgHasher {
    std::size_t operator()(T const& val) const noexcept {
        return std::hash<T>{}(val);
    }
};

template <>
struct RdvsgHasher<RdvsgSymbol> {
    std::size_t operator()(RdvsgSymbol const& s) const noexcept {
        return std::hash<std::string>{}(s.name);
    }
};

template <>
struct RdvsgHasher<RdvsgLiteral> {
    std::size_t operator()(RdvsgLiteral const& lit) const noexcept {
        return std::visit([](auto const& v) -> std::size_t {
            using T = std::decay_t<decltype(v)>;
            std::size_t seed = 0;
            detail::hash_combine_inplace(seed, static_cast<std::size_t>(lit.value.index()));
            detail::hash_combine_inplace(seed, std::hash<T>{}(v));
            return seed;
        }, lit.value);
    }
};

template <>
struct RdvsgHasher<RdvsgTerm> {
    std::size_t operator()(RdvsgTerm const& t) const noexcept {
        std::size_t seed = 0;
        if (t.is_var()) {
            detail::hash_combine_inplace(seed, 1u);
            detail::hash_combine_inplace(seed, t.as_var().name);
        } else if (t.is_lit()) {
            detail::hash_combine_inplace(seed, 2u);
            detail::hash_combine_inplace(seed, t.as_lit());
        } else {
            detail::hash_combine_inplace(seed, 3u);
            detail::hash_combine_inplace(seed, t.as_node().op);
            for (auto const& child : t.as_node().children) {
                detail::hash_combine_inplace(seed, child);
            }
        }
        return seed;
    }
};

template <>
struct RdvsgHasher<RdvsgNode> {
    std::size_t operator()(RdvsgNode const& n) const noexcept {
        std::size_t seed = 0;
        detail::hash_combine_inplace(seed, static_cast<int>(n.kind));
        detail::hash_combine_inplace(seed, n.symbol);
        if (n.literal) detail::hash_combine_inplace(seed, *n.literal);
        if (n.variable_name) detail::hash_combine_inplace(seed, *n.variable_name);
        for (auto e : n.incoming_edges) detail::hash_combine_inplace(seed, e.value);
        for (auto e : n.outgoing_edges) detail::hash_combine_inplace(seed, e.value);
        return seed;
    }
};

template <>
struct RdvsgHasher<RdvsgEdge> {
    std::size_t operator()(RdvsgEdge const& e) const noexcept {
        std::size_t seed = 0;
        detail::hash_combine_inplace(seed, e.source.value);
        detail::hash_combine_inplace(seed, e.target.value);
        detail::hash_combine_inplace(seed, e.position);
        return seed;
    }
};

// ============================================================
// Rewrite support
// ============================================================

struct RewriteRule {
    std::string name;
    std::function<bool(RdvsgTerm const&)> predicate;
    std::function<RdvsgTerm(RdvsgTerm const&)> transform;
};

class RewriteSet {
public:
    RewriteSet() = default;

    RewriteSet& add(RewriteRule rule) {
        rules_.push_back(std::move(rule));
        return *this;
    }

    std::vector<RewriteRule> const& rules() const noexcept { return rules_; }

    bool empty() const noexcept { return rules_.empty(); }

private:
    std::vector<RewriteRule> rules_;
};

inline RdvsgTerm apply_rewrite_once(RdvsgTerm const& term, RewriteSet const& set, bool* changed = nullptr) {
    for (auto const& r : set.rules()) {
        if (r.predicate && r.predicate(term)) {
            if (changed) *changed = true;
            return r.transform ? r.transform(term) : term;
        }
    }

    if (term.is_node()) {
        auto const& n = term.as_node();
        bool any_changed = false;
        std::vector<RdvsgTerm> new_children;
        new_children.reserve(n.children.size());

        for (auto const& c : n.children) {
            bool child_changed = false;
            new_children.push_back(apply_rewrite_once(c, set, &child_changed));
            any_changed = any_changed || child_changed;
        }

        if (any_changed) {
            if (changed) *changed = true;
            return RdvsgTerm::op(n.op.name, std::move(new_children));
        }
    }

    if (changed) *changed = false;
    return term;
}

inline RdvsgTerm apply_rewrite_fixpoint(RdvsgTerm term, RewriteSet const& set, std::size_t max_iterations = 64) {
    for (std::size_t i = 0; i < max_iterations; ++i) {
        bool changed = false;
        RdvsgTerm next = apply_rewrite_once(term, set, &changed);
        if (!changed) return next;
        term = std::move(next);
    }
    return term;
}

// ============================================================
// Graph manager
// ============================================================

class RdvsgGraph {
public:
    RdvsgGraph() = default;

    void clear() {
        nodes_.clear();
        edges_.clear();
        root_.reset();
        next_node_id_ = 0;
        next_edge_id_ = 0;
    }

    bool empty() const noexcept { return nodes_.empty(); }

    // --------------------------------------------------------
    // Node creation
    // --------------------------------------------------------

    RdvsgNodeId add_operator(std::string symbol) {
        RdvsgNodeId id{next_node_id_++};
        nodes_.emplace(id.value, RdvsgNode{
            RdvsgNodeKind::Operator,
            RdvsgSymbol{std::move(symbol)},
            {},
            {},
            std::nullopt,
            std::nullopt
        });
        return id;
    }

    RdvsgNodeId add_literal(RdvsgLiteral lit) {
        RdvsgNodeId id{next_node_id_++};
        nodes_.emplace(id.value, RdvsgNode{
            RdvsgNodeKind::Literal,
            RdvsgSymbol{"lit"},
            {},
            {},
            std::move(lit),
            std::nullopt
        });
        return id;
    }

    RdvsgNodeId add_variable(std::string name) {
        RdvsgNodeId id{next_node_id_++};
        nodes_.emplace(id.value, RdvsgNode{
            RdvsgNodeKind::Variable,
            RdvsgSymbol{"var"},
            {},
            {},
            std::nullopt,
            std::move(name)
        });
        return id;
    }

    // Legacy-ish generic constructor.
    RdvsgNodeId add_node(RdvsgSymbol sym, std::optional<RdvsgLiteral> lit = std::nullopt) {
        if (lit) return add_literal(*lit);
        return add_operator(std::move(sym.name));
    }

    // --------------------------------------------------------
    // Edge creation
    // --------------------------------------------------------

    RdvsgEdgeId add_edge(RdvsgNodeId from, RdvsgNodeId to, std::size_t position) {
        auto* src = try_get_node(from);
        auto* dst = try_get_node(to);
        if (!src || !dst) {
            throw std::runtime_error("Cannot add edge: source or target node does not exist.");
        }

        if (src->kind != RdvsgNodeKind::Operator) {
            throw std::runtime_error("Cannot add outgoing child edge from non-operator node.");
        }

        for (auto eid : src->outgoing_edges) {
            auto const& e = edges_.at(eid.value);
            if (e.position == position) {
                throw std::runtime_error("Cannot add edge: duplicate child position on source node.");
            }
        }

        RdvsgEdgeId id{next_edge_id_++};
        edges_.emplace(id.value, RdvsgEdge{from, to, position});

        src->outgoing_edges.push_back(id);
        dst->incoming_edges.push_back(id);
        return id;
    }

    // Appends child at next available position.
    RdvsgEdgeId append_child(RdvsgNodeId parent, RdvsgNodeId child) {
        auto* src = try_get_node(parent);
        if (!src) throw std::runtime_error("append_child: parent does not exist.");
        std::size_t pos = src->outgoing_edges.size();
        return add_edge(parent, child, pos);
    }

    // --------------------------------------------------------
    // Root management
    // --------------------------------------------------------

    void set_root(RdvsgNodeId id) {
        ensure_node_exists(id);
        root_ = id;
    }

    std::optional<RdvsgNodeId> root() const noexcept { return root_; }

    // --------------------------------------------------------
    // Accessors
    // --------------------------------------------------------

    bool has_node(RdvsgNodeId id) const noexcept {
        return nodes_.find(id.value) != nodes_.end();
    }

    bool has_edge(RdvsgEdgeId id) const noexcept {
        return edges_.find(id.value) != edges_.end();
    }

    RdvsgNode const& node(RdvsgNodeId id) const {
        auto it = nodes_.find(id.value);
        if (it == nodes_.end()) throw std::out_of_range("RdvsgNodeId not found.");
        return it->second;
    }

    RdvsgNode& node(RdvsgNodeId id) {
        auto it = nodes_.find(id.value);
        if (it == nodes_.end()) throw std::out_of_range("RdvsgNodeId not found.");
        return it->second;
    }

    RdvsgEdge const& edge(RdvsgEdgeId id) const {
        auto it = edges_.find(id.value);
        if (it == edges_.end()) throw std::out_of_range("RdvsgEdgeId not found.");
        return it->second;
    }

    RdvsgEdge& edge(RdvsgEdgeId id) {
        auto it = edges_.find(id.value);
        if (it == edges_.end()) throw std::out_of_range("RdvsgEdgeId not found.");
        return it->second;
    }

    std::size_t node_count() const noexcept { return nodes_.size(); }
    std::size_t edge_count() const noexcept { return edges_.size(); }

    std::vector<RdvsgNodeId> nodes() const {
        std::vector<RdvsgNodeId> out;
        out.reserve(nodes_.size());
        for (auto const& [k, _] : nodes_) out.push_back(RdvsgNodeId{k});
        std::sort(out.begin(), out.end(), [](auto a, auto b) { return a.value < b.value; });
        return out;
    }

    std::vector<RdvsgEdgeId> edges() const {
        std::vector<RdvsgEdgeId> out;
        out.reserve(edges_.size());
        for (auto const& [k, _] : edges_) out.push_back(RdvsgEdgeId{k});
        std::sort(out.begin(), out.end(), [](auto a, auto b) { return a.value < b.value; });
        return out;
    }

    std::vector<RdvsgNodeId> ordered_children(RdvsgNodeId id) const {
        auto const& n = node(id);
        std::vector<std::pair<std::size_t, RdvsgNodeId>> pairs;
        pairs.reserve(n.outgoing_edges.size());
        for (auto eid : n.outgoing_edges) {
            auto const& e = edge(eid);
            pairs.emplace_back(e.position, e.target);
        }
        std::sort(pairs.begin(), pairs.end(),
                  [](auto const& a, auto const& b) { return a.first < b.first; });

        std::vector<RdvsgNodeId> out;
        out.reserve(pairs.size());
        for (auto const& [_, cid] : pairs) out.push_back(cid);
        return out;
    }

    // --------------------------------------------------------
    // Validation
    // --------------------------------------------------------

    bool is_acyclic() const {
        enum class Mark : unsigned char { None, Temp, Perm };
        std::unordered_map<std::size_t, Mark> marks;

        std::function<bool(RdvsgNodeId)> dfs = [&](RdvsgNodeId id) -> bool {
            Mark& m = marks[id.value];
            if (m == Mark::Temp) return false;
            if (m == Mark::Perm) return true;

            m = Mark::Temp;
            for (auto child : ordered_children(id)) {
                if (!dfs(child)) return false;
            }
            m = Mark::Perm;
            return true;
        };

        for (auto const& [k, _] : nodes_) {
            if (!dfs(RdvsgNodeId{k})) return false;
        }
        return true;
    }

    bool is_well_formed() const {
        for (auto const& [_, n] : nodes_) {
            if (n.kind == RdvsgNodeKind::Literal) {
                if (!n.literal) return false;
                if (!n.outgoing_edges.empty()) return false;
            }
            if (n.kind == RdvsgNodeKind::Variable) {
                if (!n.variable_name) return false;
                if (!n.outgoing_edges.empty()) return false;
            }
            if (n.kind == RdvsgNodeKind::Operator) {
                if (n.literal || n.variable_name) return false;

                std::unordered_set<std::size_t> seen_positions;
                for (auto eid : n.outgoing_edges) {
                    auto it = edges_.find(eid.value);
                    if (it == edges_.end()) return false;
                    if (!seen_positions.insert(it->second.position).second) return false;
                    if (it->second.source.value == it->second.target.value) return false;
                }
            }
        }
        return is_acyclic();
    }

    // --------------------------------------------------------
    // Graph -> term
    // --------------------------------------------------------

    RdvsgTerm to_term(RdvsgNodeId id) const {
        ensure_node_exists(id);
        std::unordered_map<std::size_t, RdvsgTerm> memo;
        std::unordered_set<std::size_t> active;
        return to_term_impl(id, memo, active);
    }

    RdvsgTerm to_term() const {
        if (!root_) throw std::runtime_error("to_term(): graph has no root.");
        return to_term(*root_);
    }

    std::string dump_term(RdvsgNodeId id) const {
        return to_string(to_term(id));
    }

    std::string dump_term() const {
        return to_string(to_term());
    }

    // --------------------------------------------------------
    // Term -> graph
    // --------------------------------------------------------

    RdvsgNodeId import_term(RdvsgTerm const& term) {
        std::unordered_map<RdvsgTerm, RdvsgNodeId, RdvsgHasher<RdvsgTerm>> memo;
        RdvsgNodeId id = import_term_impl(term, memo);
        root_ = id;
        return id;
    }

    // --------------------------------------------------------
    // Reduction / deduplication
    // --------------------------------------------------------

    struct StructuralKey {
        RdvsgNodeKind kind = RdvsgNodeKind::Operator;
        std::string symbol_or_name;
        std::optional<RdvsgLiteral> literal;
        std::vector<RdvsgNodeId> children;

        bool operator==(StructuralKey const&) const noexcept = default;
    };

    struct StructuralKeyHasher {
        std::size_t operator()(StructuralKey const& k) const noexcept {
            std::size_t seed = 0;
            detail::hash_combine_inplace(seed, static_cast<int>(k.kind));
            detail::hash_combine_inplace(seed, k.symbol_or_name);
            if (k.literal) detail::hash_combine_inplace(seed, *k.literal);
            for (auto c : k.children) detail::hash_combine_inplace(seed, c.value);
            return seed;
        }
    };

    // Rebuilds a graph from the current root using structural hash-consing.
    // Sharing is induced for structurally equal subterms.
    void reduce_from_root() {
        if (!root_) return;
        RdvsgTerm t = to_term(*root_);
        clear();
        import_term_reduced(t);
    }

    RdvsgNodeId import_term_reduced(RdvsgTerm const& term) {
        std::unordered_map<StructuralKey, RdvsgNodeId, StructuralKeyHasher> cons;
        RdvsgNodeId id = import_term_reduced_impl(term, cons);
        root_ = id;
        return id;
    }

#if RDVSG_HAS_DSLUTILS
    // --------------------------------------------------------
    // RDVSG -> dsl::ASTNode bridge
    //
    // Encoding:
    //   variable -> leaf<"var">(name)
    //   literal  -> leaf<"lit">(value)
    //   operator -> node<"apply">(leaf<"op">(symbol), child1, child2, ...)
    // --------------------------------------------------------

    auto to_ast(RdvsgNodeId id) const {
        return to_ast_impl(to_term(id));
    }

    auto to_ast() const {
        return to_ast(to_root_or_throw());
    }
#endif

#if RDVSG_HAS_EQUINOXNG
    // --------------------------------------------------------
    // Adapter point for EquiNoxNG integration
    //
    // Because exact EquiNoxNG APIs vary, this provides a term-shaped export
    // hook instead of guessing symbols/types beyond presence of header.
    // --------------------------------------------------------

    RdvsgTerm to_equinox_term_like(RdvsgNodeId id) const {
        return to_term(id);
    }

    RdvsgTerm to_equinox_term_like() const {
        return to_term();
    }
#endif

private:
    std::unordered_map<std::size_t, RdvsgNode> nodes_;
    std::unordered_map<std::size_t, RdvsgEdge> edges_;
    std::optional<RdvsgNodeId> root_;
    std::size_t next_node_id_ = 0;
    std::size_t next_edge_id_ = 0;

    void ensure_node_exists(RdvsgNodeId id) const {
        if (!has_node(id)) throw std::out_of_range("RdvsgNodeId not found.");
    }

    RdvsgNodeId to_root_or_throw() const {
        if (!root_) throw std::runtime_error("graph has no root.");
        return *root_;
    }

    RdvsgNode* try_get_node(RdvsgNodeId id) {
        auto it = nodes_.find(id.value);
        return it == nodes_.end() ? nullptr : &it->second;
    }

    RdvsgTerm to_term_impl(
        RdvsgNodeId id,
        std::unordered_map<std::size_t, RdvsgTerm>& memo,
        std::unordered_set<std::size_t>& active
    ) const {
        if (auto it = memo.find(id.value); it != memo.end()) {
            return it->second;
        }

        if (!active.insert(id.value).second) {
            throw std::runtime_error("Cycle detected while converting RDVSG to term.");
        }

        auto const& n = node(id);
        RdvsgTerm result;

        switch (n.kind) {
            case RdvsgNodeKind::Literal:
                if (!n.literal) throw std::runtime_error("Malformed literal node.");
                result = RdvsgTerm::lit(*n.literal);
                break;

            case RdvsgNodeKind::Variable:
                if (!n.variable_name) throw std::runtime_error("Malformed variable node.");
                result = RdvsgTerm::var(*n.variable_name);
                break;

            case RdvsgNodeKind::Operator: {
                auto kids = ordered_children(id);
                std::vector<RdvsgTerm> args;
                args.reserve(kids.size());
                for (auto child : kids) {
                    args.push_back(to_term_impl(child, memo, active));
                }
                result = RdvsgTerm::op(n.symbol.name, std::move(args));
                break;
            }
        }

        active.erase(id.value);
        memo.emplace(id.value, result);
        return result;
    }

    RdvsgNodeId import_term_impl(
        RdvsgTerm const& term,
        std::unordered_map<RdvsgTerm, RdvsgNodeId, RdvsgHasher<RdvsgTerm>>& memo
    ) {
        if (auto it = memo.find(term); it != memo.end()) {
            return it->second;
        }

        RdvsgNodeId id{};
        if (term.is_var()) {
            id = add_variable(term.as_var().name);
        } else if (term.is_lit()) {
            id = add_literal(term.as_lit());
        } else {
            auto const& n = term.as_node();
            id = add_operator(n.op.name);
            for (std::size_t i = 0; i < n.children.size(); ++i) {
                RdvsgNodeId child = import_term_impl(n.children[i], memo);
                add_edge(id, child, i);
            }
        }

        memo.emplace(term, id);
        return id;
    }

    RdvsgNodeId import_term_reduced_impl(
        RdvsgTerm const& term,
        std::unordered_map<StructuralKey, RdvsgNodeId, StructuralKeyHasher>& cons
    ) {
        if (term.is_var()) {
            StructuralKey k;
            k.kind = RdvsgNodeKind::Variable;
            k.symbol_or_name = term.as_var().name;

            if (auto it = cons.find(k); it != cons.end()) return it->second;
            RdvsgNodeId id = add_variable(term.as_var().name);
            cons.emplace(std::move(k), id);
            return id;
        }

        if (term.is_lit()) {
            StructuralKey k;
            k.kind = RdvsgNodeKind::Literal;
            k.literal = term.as_lit();

            if (auto it = cons.find(k); it != cons.end()) return it->second;
            RdvsgNodeId id = add_literal(term.as_lit());
            cons.emplace(std::move(k), id);
            return id;
        }

        auto const& n = term.as_node();
        std::vector<RdvsgNodeId> child_ids;
        child_ids.reserve(n.children.size());
        for (auto const& c : n.children) {
            child_ids.push_back(import_term_reduced_impl(c, cons));
        }

        StructuralKey k;
        k.kind = RdvsgNodeKind::Operator;
        k.symbol_or_name = n.op.name;
        k.children = child_ids;

        if (auto it = cons.find(k); it != cons.end()) return it->second;

        RdvsgNodeId id = add_operator(n.op.name);
        for (std::size_t i = 0; i < child_ids.size(); ++i) {
            add_edge(id, child_ids[i], i);
        }
        cons.emplace(std::move(k), id);
        return id;
    }

#if RDVSG_HAS_DSLUTILS
    template <class T>
    static auto make_lit_leaf(T const& value) {
        return dsl::leaf<"lit">(value);
    }

    static auto to_ast_impl(RdvsgTerm const& t) {
        if (t.is_var()) {
            return dsl::leaf<"var">(t.as_var().name);
        }

        if (t.is_lit()) {
            return std::visit([](auto const& v) {
                return dsl::leaf<"lit">(v);
            }, t.as_lit().value);
        }

        auto const& n = t.as_node();
        auto op_leaf = dsl::leaf<"op">(n.op.name);

        // Build node<"apply">(op, child1, child2, ...)
        return ast_apply_builder(n, op_leaf, std::make_index_sequence<0>{});
    }

    template <class OpLeaf>
    static auto ast_apply_builder(RdvsgTerm::Node const& n, OpLeaf const& op_leaf, std::index_sequence<>) {
        switch (n.children.size()) {
            case 0:
                return dsl::node<"apply">(op_leaf);
            case 1: {
                auto c0 = to_ast_impl(n.children[0]);
                return dsl::node<"apply">(op_leaf, c0);
            }
            case 2: {
                auto c0 = to_ast_impl(n.children[0]);
                auto c1 = to_ast_impl(n.children[1]);
                return dsl::node<"apply">(op_leaf, c0, c1);
            }
            case 3: {
                auto c0 = to_ast_impl(n.children[0]);
                auto c1 = to_ast_impl(n.children[1]);
                auto c2 = to_ast_impl(n.children[2]);
                return dsl::node<"apply">(op_leaf, c0, c1, c2);
            }
            case 4: {
                auto c0 = to_ast_impl(n.children[0]);
                auto c1 = to_ast_impl(n.children[1]);
                auto c2 = to_ast_impl(n.children[2]);
                auto c3 = to_ast_impl(n.children[3]);
                return dsl::node<"apply">(op_leaf, c0, c1, c2, c3);
            }
            default: {
                // Portable fallback for unknown ASTNode variadic/mutable interfaces:
                // encode n-ary application as left-associated binary apply chains.
                auto acc = dsl::node<"apply">(op_leaf);
                for (auto const& child : n.children) {
                    acc = dsl::node<"apply">(acc, to_ast_impl(child));
                }
                return acc;
            }
        }
    }
#endif
};

// ============================================================
// Convenience builders
// ============================================================

inline RdvsgTerm var(std::string name) {
    return RdvsgTerm::var(std::move(name));
}

inline RdvsgTerm lit(RdvsgLiteral v) {
    return RdvsgTerm::lit(std::move(v));
}

inline RdvsgTerm op(std::string name, std::vector<RdvsgTerm> args = {}) {
    return RdvsgTerm::op(std::move(name), std::move(args));
}

inline RewriteRule rule(
    std::string name,
    std::function<bool(RdvsgTerm const&)> predicate,
    std::function<RdvsgTerm(RdvsgTerm const&)> transform
) {
    return RewriteRule{std::move(name), std::move(predicate), std::move(transform)};
}

// ============================================================
// Stream support
// ============================================================

inline std::ostream& operator<<(std::ostream& os, RdvsgTerm const& t) {
    return os << to_string(t);
}

} // namespace rdvsg
