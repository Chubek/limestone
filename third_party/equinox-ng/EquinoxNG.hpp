#pragma once

/*
    EquiNoxNG.hpp
    -------------
    Header-only e-graph library for C++20.

    Features:
      - Interned ENode storage
      - Union-Find based EClass merging
      - Rebuild / congruence maintenance
      - Pattern matching for rewrites
      - Equality saturation runner
      - Cost-based extraction
      - Optional DSL integration hooks for DSLUtils.hpp

    Design goals:
      - Header-only
      - Reasonably production-grade structure
      - Good defaults
      - Extensible analysis/costing/rewrite pipeline
      - Safe and explicit APIs

    LLM-AGENT HINT:
      Good next improvements:
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

#if __has_include("DSLUtils.hpp")
#  include "DSLUtils.hpp"
#  define EQUINOXNG_HAS_DSLUTILS 1
#else
#  define EQUINOXNG_HAS_DSLUTILS 0
#endif

namespace equinoxng {

// ============================================================
// Utility
// ============================================================

namespace detail {

inline std::size_t hash_combine(std::size_t seed, std::size_t v) noexcept {
    // Standard-ish hash combine
    return seed ^ (v + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2));
}

template <class T>
inline void hash_combine_inplace(std::size_t& seed, T const& v) noexcept {
    seed = hash_combine(seed, std::hash<T>{}(v));
}

template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

} // namespace detail

// ============================================================
// Public scalar IDs
// ============================================================

struct EClassId {
    std::uint32_t value = 0;

    constexpr bool operator==(const EClassId&) const noexcept = default;
    constexpr bool operator!=(const EClassId&) const noexcept = default;
};

struct ENodeId {
    std::uint32_t value = 0;

    constexpr bool operator==(const ENodeId&) const noexcept = default;
    constexpr bool operator!=(const ENodeId&) const noexcept = default;
};

} // namespace equinoxng

template <>
struct std::hash<equinoxng::EClassId> {
    std::size_t operator()(const equinoxng::EClassId& x) const noexcept {
        return std::hash<std::uint32_t>{}(x.value);
    }
};

template <>
struct std::hash<equinoxng::ENodeId> {
    std::size_t operator()(const equinoxng::ENodeId& x) const noexcept {
        return std::hash<std::uint32_t>{}(x.value);
    }
};

namespace equinoxng {

// ============================================================
// Symbol / Literal / Term frontend
// ============================================================

struct Symbol {
    std::string name;

    Symbol() = default;
    Symbol(std::string n) : name(std::move(n)) {}

    bool operator==(Symbol const&) const noexcept = default;
};

} // namespace equinoxng

template <>
struct std::hash<equinoxng::Symbol> {
    std::size_t operator()(equinoxng::Symbol const& s) const noexcept {
        return std::hash<std::string>{}(s.name);
    }
};

namespace equinoxng {

struct Literal {
    using Value = std::variant<std::int64_t, double, bool, std::string>;
    Value value;

    Literal() = default;
    Literal(std::int64_t v) : value(v) {}
    Literal(double v) : value(v) {}
    Literal(bool v) : value(v) {}
    Literal(std::string v) : value(std::move(v)) {}
    Literal(const char* v) : value(std::string(v)) {}

    bool operator==(Literal const&) const noexcept = default;
};

} // namespace equinoxng

template <>
struct std::hash<equinoxng::Literal> {
    std::size_t operator()(equinoxng::Literal const& lit) const noexcept {
        return std::visit([](auto const& v) {
            using T = std::decay_t<decltype(v)>;
            std::size_t seed = 0;
            detail::hash_combine_inplace(seed, static_cast<std::size_t>(lit.value.index()));
            detail::hash_combine_inplace(seed, std::hash<T>{}(v));
            return seed;
        }, lit.value);
    }
};

namespace equinoxng {

struct Term {
    // A simple AST for input/output and pattern construction.
    struct Var { std::string name; };
    struct Node {
        Symbol op;
        std::vector<Term> children;
    };

    using Repr = std::variant<Var, Literal, Node>;
    Repr repr;

    Term() = default;
    explicit Term(Var v) : repr(std::move(v)) {}
    explicit Term(Literal l) : repr(std::move(l)) {}
    explicit Term(Node n) : repr(std::move(n)) {}

    static Term var(std::string name) { return Term{Var{std::move(name)}}; }
    static Term lit(Literal v) { return Term{std::move(v)}; }
    static Term op(std::string name, std::vector<Term> args = {}) {
        return Term{Node{Symbol{std::move(name)}, std::move(args)}};
    }

    bool is_var() const noexcept { return std::holds_alternative<Var>(repr); }
    bool is_lit() const noexcept { return std::holds_alternative<Literal>(repr); }
    bool is_node() const noexcept { return std::holds_alternative<Node>(repr); }

    Var const& as_var() const { return std::get<Var>(repr); }
    Literal const& as_lit() const { return std::get<Literal>(repr); }
    Node const& as_node() const { return std::get<Node>(repr); }
};

inline std::string to_string(Term const& t);

inline std::string literal_to_string(Literal const& lit) {
    return std::visit(detail::overloaded{
        [](std::int64_t v) { return std::to_string(v); },
        [](double v) {
            std::ostringstream oss;
            oss << v;
            return oss.str();
        },
        [](bool v) { return v ? std::string("true") : std::string("false"); },
        [](std::string const& v) { return "\"" + v + "\""; }
    }, lit.value);
}

inline std::string to_string(Term const& t) {
    if (t.is_var()) return "?" + t.as_var().name;
    if (t.is_lit()) return literal_to_string(t.as_lit());

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
// ENode / EClass
// ============================================================

struct ENode {
    // Either an operator application or a literal leaf.
    std::variant<Symbol, Literal> head;
    std::vector<EClassId> children;

    ENode() = default;
    ENode(Symbol op, std::vector<EClassId> ch = {})
        : head(std::move(op)), children(std::move(ch)) {}
    ENode(Literal lit)
        : head(std::move(lit)), children() {}

    bool is_symbol() const noexcept { return std::holds_alternative<Symbol>(head); }
    bool is_literal() const noexcept { return std::holds_alternative<Literal>(head); }

    Symbol const& symbol() const { return std::get<Symbol>(head); }
    Literal const& literal() const { return std::get<Literal>(head); }

    bool operator==(ENode const& other) const noexcept {
        return head == other.head && children == other.children;
    }
};

} // namespace equinoxng

template <>
struct std::hash<equinoxng::ENode> {
    std::size_t operator()(equinoxng::ENode const& n) const noexcept {
        std::size_t seed = 0;
        if (n.is_symbol()) {
            detail::hash_combine_inplace(seed, 1u);
            detail::hash_combine_inplace(seed, n.symbol());
        } else {
            detail::hash_combine_inplace(seed, 2u);
            detail::hash_combine_inplace(seed, n.literal());
        }
        for (auto const& c : n.children) {
            detail::hash_combine_inplace(seed, c.value);
        }
        return seed;
    }
};

namespace equinoxng {

struct EClass {
    EClassId id{};
    std::vector<ENodeId> nodes;
    std::unordered_set<EClassId> parents; // parent eclasses whose nodes reference this class
};

// ============================================================
// Union-find
// ============================================================

class UnionFind {
public:
    EClassId make() {
        EClassId id{static_cast<std::uint32_t>(parent_.size())};
        parent_.push_back(id.value);
        rank_.push_back(0);
        return id;
    }

    EClassId find(EClassId x) {
        auto& p = parent_.at(x.value);
        if (p != x.value) {
            p = find(EClassId{p}).value;
        }
        return EClassId{p};
    }

    EClassId find_const(EClassId x) const {
        while (parent_.at(x.value) != x.value) {
            x = EClassId{parent_.at(x.value)};
        }
        return x;
    }

    EClassId unite(EClassId a, EClassId b) {
        a = find(a);
        b = find(b);
        if (a == b) return a;

        auto ra = rank_.at(a.value);
        auto rb = rank_.at(b.value);
        if (ra < rb) std::swap(a, b);

        parent_[b.value] = a.value;
        if (ra == rb) ++rank_[a.value];
        return a;
    }

    std::size_t size() const noexcept { return parent_.size(); }

private:
    std::vector<std::uint32_t> parent_;
    std::vector<std::uint8_t> rank_;
};

// ============================================================
// Substitution / Pattern
// ============================================================

using Subst = std::unordered_map<std::string, EClassId>;

inline std::string subst_to_string(Subst const& s) {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (auto const& [k, v] : s) {
        if (!first) oss << ", ";
        first = false;
        oss << "?" << k << "->" << v.value;
    }
    oss << "}";
    return oss.str();
}

// ============================================================
// Rewrite rule
// ============================================================

struct RewriteRule {
    std::string name;
    Term lhs;
    Term rhs;

    // Optional guard on a matched substitution.
    std::function<bool(Subst const&)> condition;

    RewriteRule() = default;
    RewriteRule(std::string n, Term l, Term r,
                std::function<bool(Subst const&)> cond = {})
        : name(std::move(n)), lhs(std::move(l)), rhs(std::move(r)),
          condition(std::move(cond)) {}
};

// ============================================================
// Cost model / extraction
// ============================================================

struct AstSizeCost {
    using cost_type = std::size_t;

    cost_type literal_cost(Literal const&) const noexcept { return 1; }
    cost_type symbol_cost(Symbol const&, std::size_t arity) const noexcept {
        return 1 + arity;
    }
};

struct Extracted {
    std::size_t cost = std::numeric_limits<std::size_t>::max();
    Term term = Term::op("invalid");
};

// ============================================================
// Stats / configuration
// ============================================================

struct RunnerStats {
    std::size_t iterations = 0;
    std::size_t rewrites_applied = 0;
    std::size_t enodes_added = 0;
    std::size_t unions_performed = 0;
    bool saturated = false;
};

struct RunnerConfig {
    std::size_t iter_limit = 30;
    std::size_t node_limit = 200000;
    bool rebuild_every_iteration = true;
    bool stop_if_no_change = true;
};

// ============================================================
// Main EGraph
// ============================================================

class EGraph {
public:
    EGraph() = default;

    // ----------------------------
    // Core insertion
    // ----------------------------

    EClassId add(Term const& t) {
        return add_term(t);
    }

    EClassId merge(EClassId a, EClassId b) {
        a = find(a);
        b = find(b);
        if (a == b) return a;

        auto root = uf_.unite(a, b);
        auto other = (root == a) ? b : a;

        auto& rc = classes_.at(root.value);
        auto& oc = classes_.at(other.value);

        rc.nodes.insert(rc.nodes.end(), oc.nodes.begin(), oc.nodes.end());
        rc.parents.insert(oc.parents.begin(), oc.parents.end());

        ++stats_.unions_performed;
        changed_ = true;
        return root;
    }

    EClassId find(EClassId id) {
        return uf_.find(id);
    }

    EClassId find_const(EClassId id) const {
        return uf_.find_const(id);
    }

    std::size_t num_classes() const { return classes_.size(); }
    std::size_t num_enodes() const { return enodes_.size(); }

    RunnerStats const& stats() const noexcept { return stats_; }

    // ----------------------------
    // Rebuild / congruence closure
    // ----------------------------

    void rebuild() {
        // Canonicalize nodes and rehashcons them.
        // Production note: a more optimized implementation would maintain
        // per-class worklists and parent-use indexes by operator.
        bool local_change = true;
        while (local_change) {
            local_change = false;

            std::unordered_map<ENode, ENodeId> new_memo;
            new_memo.reserve(enodes_.size() * 2 + 1);

            for (ENodeId nid{0}; nid.value < enodes_.size(); ++nid.value) {
                auto node = enodes_[nid.value];
                for (auto& c : node.children) {
                    c = find(c);
                }

                auto it = new_memo.find(node);
                if (it == new_memo.end()) {
                    new_memo.emplace(node, nid);
                    enodes_[nid.value] = std::move(node);
                } else {
                    auto existing = it->second;
                    auto owner1 = owner_.at(nid.value);
                    auto owner2 = owner_.at(existing.value);
                    auto r1 = find(owner1);
                    auto r2 = find(owner2);
                    if (r1 != r2) {
                        merge(r1, r2);
                        local_change = true;
                    }
                }
            }

            // Recompute membership lists by representative.
            rebuild_classes_from_owner();
            memo_ = std::move(new_memo);
        }

    }

    // ----------------------------
    // Pattern matching
    // ----------------------------

    std::vector<Subst> match(Term const& pattern, EClassId root) {
        std::vector<Subst> out;
        Subst s;
        match_in_class(pattern, find(root), s, out);
        return out;
    }

    // ----------------------------
    // Rewrite application
    // ----------------------------

    std::size_t apply_rule(RewriteRule const& rule) {
        std::size_t applied = 0;

        // Match rule over all current representative classes.
        std::unordered_set<std::uint32_t> reps_seen;
        for (std::uint32_t i = 0; i < classes_.size(); ++i) {
            EClassId eid{i};
            auto rep = find(eid);
            if (!reps_seen.insert(rep.value).second) continue;

            auto matches = match(rule.lhs, rep);
            for (auto const& subst : matches) {
                if (rule.condition && !rule.condition(subst)) {
                    continue;
                }

                auto lhs_id = instantiate_into_eclass(rule.lhs, subst);
                auto rhs_id = instantiate_into_eclass(rule.rhs, subst);
                auto lrep = find(lhs_id);
                auto rrep = find(rhs_id);
                if (lrep != rrep) {
                    merge(lrep, rrep);
                    ++applied;
                }
            }
        }

        stats_.rewrites_applied += applied;
        return applied;
    }

    template <class Range>
    std::size_t apply_rules(Range const& rules) {
        std::size_t total = 0;
        for (auto const& r : rules) {
            total += apply_rule(r);
        }
        return total;
    }

    // ----------------------------
    // Equality saturation runner
    // ----------------------------

    template <class Range>
    RunnerStats run(Range const& rules, RunnerConfig cfg = {}) {
        stats_ = RunnerStats{};
        changed_ = true;

        for (std::size_t iter = 0; iter < cfg.iter_limit; ++iter) {
            stats_.iterations = iter + 1;
            changed_ = false;

            auto before_nodes = num_enodes();
            auto applied = apply_rules(rules);

            if (cfg.rebuild_every_iteration) {
                rebuild();
            }

            stats_.enodes_added = num_enodes() - before_nodes;

            if (num_enodes() > cfg.node_limit) {
                stats_.saturated = false;
                return stats_;
            }

            if (cfg.stop_if_no_change && applied == 0 && !changed_) {
                stats_.saturated = true;
                return stats_;
            }
        }

        stats_.saturated = false;
        return stats_;
    }

    // ----------------------------
    // Extraction
    // ----------------------------

    template <class CostModel = AstSizeCost>
    Extracted extract(EClassId root, CostModel cost_model = {}) {
        root = find(root);

        using Cost = typename CostModel::cost_type;
        struct Best {
            Cost cost = std::numeric_limits<Cost>::max();
            std::optional<Term> term;
        };

        std::unordered_map<std::uint32_t, Best> best;

        // Chaotic fixed-point DP over e-classes.
        bool changed = true;
        std::size_t rounds = 0;
        while (changed && rounds++ < classes_.size() + enodes_.size() + 8) {
            changed = false;

            std::unordered_set<std::uint32_t> reps_seen;
            for (std::uint32_t i = 0; i < classes_.size(); ++i) {
                EClassId eid{i};
                auto rep = find(eid);
                if (!reps_seen.insert(rep.value).second) continue;

                auto const& ec = classes_.at(rep.value);
                Best cand_best = best[rep.value];

                for (auto nid : ec.nodes) {
                    auto const& n = enodes_.at(nid.value);

                    if (n.is_literal()) {
                        Cost c = cost_model.literal_cost(n.literal());
                        if (!cand_best.term || c < cand_best.cost) {
                            cand_best.cost = c;
                            cand_best.term = Term::lit(n.literal());
                        }
                        continue;
                    }

                    std::vector<Term> args;
                    args.reserve(n.children.size());
                    bool ok = true;
                    Cost total = cost_model.symbol_cost(n.symbol(), n.children.size());

                    for (auto child : n.children) {
                        child = find(child);
                        auto it = best.find(child.value);
                        if (it == best.end() || !it->second.term.has_value()) {
                            ok = false;
                            break;
                        }
                        total += it->second.cost;
                        args.push_back(*it->second.term);
                    }

                    if (ok && (!cand_best.term || total < cand_best.cost)) {
                        cand_best.cost = total;
                        cand_best.term = Term::op(n.symbol().name, std::move(args));
                    }
                }

                auto& slot = best[rep.value];
                if (cand_best.term && (!slot.term || cand_best.cost < slot.cost)) {
                    slot = std::move(cand_best);
                    changed = true;
                }
            }
        }

        auto it = best.find(root.value);
        if (it == best.end() || !it->second.term) {
            return {};
        }

        return Extracted{
            static_cast<std::size_t>(it->second.cost),
            *it->second.term
        };
    }

    // ----------------------------
    // Debug / inspection
    // ----------------------------

    std::string dump() const {
        std::ostringstream oss;
        std::unordered_set<std::uint32_t> reps_seen;

        for (std::uint32_t i = 0; i < classes_.size(); ++i) {
            EClassId eid{i};
            auto rep = find_const(eid);
            if (!reps_seen.insert(rep.value).second) continue;

            auto const& ec = classes_.at(rep.value);
            oss << "EClass " << rep.value << ":\n";
            for (auto nid : ec.nodes) {
                oss << "  " << nid.value << ": " << enode_to_string(enodes_[nid.value]) << "\n";
            }
        }
        return oss.str();
    }

private:
    // ----------------------------
    // Storage
    // ----------------------------

    UnionFind uf_;
    std::vector<EClass> classes_;
    std::vector<ENode> enodes_;
    std::vector<EClassId> owner_;              // owner eclass of each enode
    std::unordered_map<ENode, ENodeId> memo_;  // canonical enode -> enode id
    RunnerStats stats_{};
    bool changed_ = false;

    // ----------------------------
    // Allocation helpers
    // ----------------------------

    EClassId fresh_class() {
        auto id = uf_.make();
        if (id.value != classes_.size()) {
            throw std::logic_error("EClass allocation out of sync");
        }
        classes_.push_back(EClass{id, {}, {}});
        return id;
    }

    ENodeId fresh_enode(ENode n, EClassId owner) {
        ENodeId nid{static_cast<std::uint32_t>(enodes_.size())};
        enodes_.push_back(std::move(n));
        owner_.push_back(owner);
        classes_.at(owner.value).nodes.push_back(nid);
        return nid;
    }

    // ----------------------------
    // Canonicalized add
    // ----------------------------

    EClassId add_enode(ENode n) {
        for (auto& c : n.children) {
            c = find(c);
        }

        auto it = memo_.find(n);
        if (it != memo_.end()) {
            return find(owner_.at(it->second.value));
        }

        auto eid = fresh_class();
        auto nid = fresh_enode(std::move(n), eid);
        memo_.emplace(enodes_.at(nid.value), nid);

        // Update parent links.
        for (auto child : enodes_.at(nid.value).children) {
            child = find(child);
            classes_.at(child.value).parents.insert(eid);
        }

        changed_ = true;
        return eid;
    }

    EClassId add_term(Term const& t) {
        if (t.is_var()) {
            throw std::invalid_argument("cannot add a pattern variable as a concrete term");
        }

        if (t.is_lit()) {
            return add_enode(ENode{t.as_lit()});
        }

        auto const& n = t.as_node();
        std::vector<EClassId> child_ids;
        child_ids.reserve(n.children.size());
        for (auto const& c : n.children) {
            child_ids.push_back(add_term(c));
        }
        return add_enode(ENode{n.op, std::move(child_ids)});
    }

    // ----------------------------
    // Matching
    // ----------------------------

    void match_in_class(Term const& pat, EClassId eid, Subst subst, std::vector<Subst>& out) {
        eid = find(eid);

        if (pat.is_var()) {
            auto const& v = pat.as_var().name;
            auto it = subst.find(v);
            if (it == subst.end()) {
                subst.emplace(v, eid);
                out.push_back(std::move(subst));
            } else if (find(it->second) == eid) {
                out.push_back(std::move(subst));
            }
            return;
        }

        auto const& ec = classes_.at(eid.value);

        for (auto nid : ec.nodes) {
            auto const& n = enodes_.at(nid.value);

            if (pat.is_lit()) {
                if (n.is_literal() && n.literal() == pat.as_lit()) {
                    out.push_back(subst);
                }
                continue;
            }

            auto const& pn = pat.as_node();
            if (!n.is_symbol()) continue;
            if (n.symbol() != pn.op) continue;
            if (n.children.size() != pn.children.size()) continue;

            std::vector<Subst> frontier{subst};
            for (std::size_t i = 0; i < pn.children.size(); ++i) {
                std::vector<Subst> next;
                for (auto const& s : frontier) {
                    match_in_class(pn.children[i], find(n.children[i]), s, next);
                }
                frontier = std::move(next);
                if (frontier.empty()) break;
            }

            out.insert(out.end(),
                       std::make_move_iterator(frontier.begin()),
                       std::make_move_iterator(frontier.end()));
        }
    }

    // ----------------------------
    // Instantiation
    // ----------------------------

    EClassId instantiate_into_eclass(Term const& t, Subst const& subst) {
        if (t.is_var()) {
            auto it = subst.find(t.as_var().name);
            if (it == subst.end()) {
                throw std::invalid_argument("unbound variable in instantiate");
            }
            return find(it->second);
        }
        if (t.is_lit()) {
            return add_enode(ENode{t.as_lit()});
        }

        auto const& n = t.as_node();
        std::vector<EClassId> child_ids;
        child_ids.reserve(n.children.size());
        for (auto const& c : n.children) {
            child_ids.push_back(instantiate_into_eclass(c, subst));
        }
        return add_enode(ENode{n.op, std::move(child_ids)});
    }

    // ----------------------------
    // Maintenance
    // ----------------------------

    void rebuild_classes_from_owner() {
        std::vector<std::vector<ENodeId>> grouped(classes_.size());
        for (ENodeId nid{0}; nid.value < owner_.size(); ++nid.value) {
            owner_[nid.value] = find(owner_[nid.value]);
            grouped[owner_[nid.value].value].push_back(nid);
        }

        for (std::uint32_t i = 0; i < classes_.size(); ++i) {
            classes_[i].id = EClassId{i};
            classes_[i].nodes = std::move(grouped[i]);
            classes_[i].parents.clear();
        }

        for (ENodeId nid{0}; nid.value < enodes_.size(); ++nid.value) {
            auto const& node = enodes_[nid.value];
            auto own = owner_[nid.value];
            for (auto child : node.children) {
                child = find(child);
                classes_[child.value].parents.insert(own);
            }
        }
    }

    std::string enode_to_string(ENode const& n) const {
        if (n.is_literal()) return literal_to_string(n.literal());

        std::ostringstream oss;
        oss << "(" << n.symbol().name;
        for (auto c : n.children) {
            oss << " @" << find_const(c).value;
        }
        oss << ")";
        return oss.str();
    }
};

// ============================================================
// DSL helpers
// ============================================================

namespace dsl_api {

// Small embedded frontend for nicer term construction.
struct expr {
    Term t;

    expr() = default;
    explicit expr(Term v) : t(std::move(v)) {}

    operator Term const&() const noexcept { return t; }
};

inline expr var(std::string name) {
    return expr{Term::var(std::move(name))};
}

inline expr lit(std::int64_t v) { return expr{Term::lit(Literal{v})}; }
inline expr lit(double v)       { return expr{Term::lit(Literal{v})}; }
inline expr lit(bool v)         { return expr{Term::lit(Literal{v})}; }
inline expr lit(std::string v)  { return expr{Term::lit(Literal{std::move(v)})}; }
inline expr lit(const char* v)  { return expr{Term::lit(Literal{v})}; }

inline expr call(std::string op, std::initializer_list<expr> xs = {}) {
    std::vector<Term> args;
    args.reserve(xs.size());
    for (auto const& x : xs) args.push_back(x.t);
    return expr{Term::op(std::move(op), std::move(args))};
}

inline RewriteRule rule(std::string name, expr lhs, expr rhs,
                        std::function<bool(Subst const&)> cond = {}) {
    return RewriteRule{std::move(name), lhs.t, rhs.t, std::move(cond)};
}

// Arithmetic sugar
inline expr add(expr a, expr b) { return call("add", {a, b}); }
inline expr mul(expr a, expr b) { return call("mul", {a, b}); }
inline expr sub(expr a, expr b) { return call("sub", {a, b}); }
inline expr div(expr a, expr b) { return call("div", {a, b}); }
inline expr neg(expr a)         { return call("neg", {a}); }

} // namespace dsl_api

// ============================================================
// Optional integration with DSLUtils.hpp
// ============================================================

#if EQUINOXNG_HAS_DSLUTILS

/*
    Based on retrieved summary from DSLUtils.hpp:
      - primary namespace is dsl
      - DSL<Derived,...> exists
      - ASTNode exists
      - rewrite helpers exist: dsl::rule<...>, dsl::rewrite_set(...)
      - AST constructors: dsl::node<"tag">(...), dsl::leaf<"tag">(...)
    Cited from retrieval summary of DSLUtils.hpp:
      namespace/core API summary lines 365, 430, 973, 1370, ~114-136, 1504.

    Since we only have a summary and not the exact full signatures,
    this integration layer is intentionally conservative and optional.
*/

namespace dsl_bridge {

/*
      Improve this adapter after retrieving exact signatures from DSLUtils.hpp:
        - Add exact conversion from dsl::ASTNode fields to equinoxng::Term
        - Add exact conversion from equinoxng::Term back to dsl::ASTNode
        - Build rewrite rules directly from dsl::rule<"name"> metadata
        - Add compile-time string tag adapters if DSLUtils uses fixed_string NTTP
*/

template <typename ASTNodeLike>
concept HasDump = requires(ASTNodeLike const& x) {
    { x.dump() } -> std::convertible_to<std::string>;
};

// Fallback adapter from "dump()" S-expression text could be implemented later.
// For now, provide placeholder extension points.

template <class ASTNodeLike>
inline Term from_dsl_ast(ASTNodeLike const&) {
    static_assert(sizeof(ASTNodeLike) == 0,
        "from_dsl_ast requires exact DSLUtils ASTNode structure. "
        "Retrieve exact ASTNode definition/signatures from DSLUtils.hpp and implement adapter.");
    return Term{};
}

template <class DSLRewriteLike>
inline RewriteRule from_dsl_rewrite(DSLRewriteLike const&) {
    static_assert(sizeof(DSLRewriteLike) == 0,
        "from_dsl_rewrite requires exact DSLUtils Rewrite/rule signature. "
        "Retrieve exact rule definition from DSLUtils.hpp and implement adapter.");
    return {};
}

} // namespace dsl_bridge

#endif // EQUINOXNG_HAS_DSLUTILS

// ============================================================
// Convenience library of classic rewrites
// ============================================================

namespace rules {

inline std::vector<RewriteRule> arithmetic_ring_basic() {
    using namespace dsl_api;

    auto x = var("x");
    auto y = var("y");
    auto z = var("z");

    return {
        rule("add-comm",  add(x, y), add(y, x)),
        rule("add-assoc", add(add(x, y), z), add(x, add(y, z))),
        rule("mul-comm",  mul(x, y), mul(y, x)),
        rule("mul-assoc", mul(mul(x, y), z), mul(x, mul(y, z))),
        rule("distrib",   mul(x, add(y, z)), add(mul(x, y), mul(x, z))),
        rule("add-zero",  add(x, lit(0)), x),
        rule("mul-one",   mul(x, lit(1)), x),
        rule("mul-zero",  mul(x, lit(0)), lit(0)),
        rule("sub-self",  sub(x, x), lit(0)),
        rule("double-neg", neg(neg(x)), x)
    };
}

} // namespace rules

// ============================================================
// Example helper
// ============================================================

inline std::ostream& operator<<(std::ostream& os, Term const& t) {
    return os << to_string(t);
}

} // namespace equinoxng
