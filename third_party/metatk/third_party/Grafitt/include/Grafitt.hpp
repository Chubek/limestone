#pragma once

// grafitt.hpp
// Header-only graph library inspired by OCamlGraph, with:
// - immutable/persistent and mutable/imperative graphs
// - OCamlGraph-like traversal/fold/builder style APIs
// - Queryfitt query DSL scaffolding
// - graph rewriting
// - GBIN serialization hooks for SerdeTk
//
// Requires C++20.

#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ios>
#include <istream>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <ostream>
#include <queue>
#include <ranges>
#include <set>
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

// MetaTk DSLtk provides the DSL mixin framework (dsl::DSL, dsl::pattern,
// dsl::FixedString, ...) and the PEG machinery used by the Queryfitt text
// parser. It replaced the old matcheroni/DSLUtils dependencies.
//
// The qualified "MetaTk/DSLtk/DSLtk.hpp" path is preferred because it is
// unambiguous: some vendored trees (e.g. SerdeTk's bundle) ship a bare
// "DSLtk.hpp" that merely forwards to the older IPCtk/DSLUtils.hpp, which
// lacks the PEG API (dsl::PEGDefinition, dsl::create_peg_definition, ...).
// Preferring the namespaced path guarantees we get the real DSLtk with PEG
// support whenever MetaTk is vendored, falling back to a bare "DSLtk.hpp"
// only for standalone system installs.
#if __has_include("MetaTk/DSLtk/DSLtk.hpp")
#  include "MetaTk/DSLtk/DSLtk.hpp"
#  define GRAFITT_HAS_DSLTK 1
#elif __has_include("DSLtk.hpp")
#  include "DSLtk.hpp"
#  define GRAFITT_HAS_DSLTK 1
#else
#  define GRAFITT_HAS_DSLTK 0
#endif

// Backwards-compatible alias: GRAFITT_HAS_DSLUTILS now means "DSLtk is
// available", since DSLtk is the successor of DSLUtils.
#define GRAFITT_HAS_DSLUTILS GRAFITT_HAS_DSLTK

// SerdeTk capability flag. We detect its presence without pulling it in:
// Grafitt's GBIN codec is self-contained (see namespace gbin below), and
// SerdeTk.hpp transitively includes a bundled "DSLtk.hpp" stub that can
// shadow the real MetaTk DSLtk and break the PEG machinery above. The flag
// is therefore reserved for future SerdeTk-backed schema hooks and is not
// transitively included here.
#if __has_include("SerdeTk.hpp")
#  define GRAFITT_HAS_SERDETK 1
#else
#  define GRAFITT_HAS_SERDETK 0
#endif

namespace grafitt {

// ============================================================
// Core utility concepts and traits
// ============================================================

template<class T>
concept Hashable = requires(T v) {
    { std::hash<T>{}(v) } -> std::convertible_to<std::size_t>;
};

template<class T>
concept EqualityComparable = requires(T a, T b) {
    { a == b } -> std::convertible_to<bool>;
};

template<class V>
concept VertexLike = Hashable<V> && EqualityComparable<V>;

template<class E>
concept EdgeLabelLike = std::default_initializable<E>;

struct unit final {
    friend constexpr bool operator==(unit, unit) = default;
};

template<class T>
struct identity_key {
    using type = T;
    constexpr const T& operator()(const T& v) const noexcept { return v; }
};

template<class T>
inline std::string to_string_fallback(const T& value) {
    if constexpr (requires(std::ostream& os) { os << value; }) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    } else {
        return "<unprintable>";
    }
}

// ============================================================
// Exceptions
// ============================================================

struct grafitt_error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct parse_error : grafitt_error {
    using grafitt_error::grafitt_error;
};

struct serialization_error : grafitt_error {
    using grafitt_error::grafitt_error;
};

struct rewrite_error : grafitt_error {
    using grafitt_error::grafitt_error;
};

struct query_error : grafitt_error {
    using grafitt_error::grafitt_error;
};

// ============================================================
// Edge model
// ============================================================

template<class Vertex, class EdgeLabel = unit>
struct edge {
    Vertex src {};
    Vertex dst {};
    EdgeLabel label {};

    friend bool operator==(const edge&, const edge&) = default;
};

template<class Vertex, class EdgeLabel>
struct edge_hash {
    std::size_t operator()(const edge<Vertex, EdgeLabel>& e) const noexcept {
        std::size_t h1 = std::hash<Vertex>{}(e.src);
        std::size_t h2 = std::hash<Vertex>{}(e.dst);
        std::size_t h3 = 0;
        if constexpr (Hashable<EdgeLabel>) {
            h3 = std::hash<EdgeLabel>{}(e.label);
        }
        return h1 ^ (h2 << 1) ^ (h3 << 7);
    }
};

// ============================================================
// Graph traits and direction
// ============================================================

enum class direction {
    directed,
    undirected
};

template<class Vertex, class EdgeLabel = unit>
using edge_set = std::unordered_set<edge<Vertex, EdgeLabel>, edge_hash<Vertex, EdgeLabel>>;

// ============================================================
// Internal graph storage
// ============================================================

template<VertexLike Vertex, class EdgeLabel = unit>
class adjacency_storage {
public:
    using vertex_type = Vertex;
    using edge_label_type = EdgeLabel;
    using edge_type = edge<Vertex, EdgeLabel>;

private:
    direction dir_ { direction::directed };
    std::unordered_set<Vertex> vertices_;
    edge_set<Vertex, EdgeLabel> edges_;
    std::unordered_map<Vertex, std::vector<edge_type>> out_;
    std::unordered_map<Vertex, std::vector<edge_type>> in_;

public:
    adjacency_storage() = default;
    explicit adjacency_storage(direction dir) : dir_(dir) {}

    [[nodiscard]] direction dir() const noexcept { return dir_; }

    [[nodiscard]] bool mem_vertex(const Vertex& v) const {
        return vertices_.contains(v);
    }

    [[nodiscard]] bool mem_edge(const Vertex& s, const Vertex& d) const {
        if (auto it = out_.find(s); it != out_.end()) {
            for (const auto& e : it->second) {
                if (e.dst == d) return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool mem_edge_e(const edge_type& e) const {
        return edges_.contains(e);
    }

    void add_vertex(const Vertex& v) {
        vertices_.insert(v);
        out_.try_emplace(v);
        in_.try_emplace(v);
    }

    void add_edge(const edge_type& e) {
        add_vertex(e.src);
        add_vertex(e.dst);

        if (edges_.contains(e)) return;

        edges_.insert(e);
        out_[e.src].push_back(e);
        in_[e.dst].push_back(e);

        if (dir_ == direction::undirected && !(e.src == e.dst)) {
            edge_type rev { e.dst, e.src, e.label };
            if (!edges_.contains(rev)) {
                edges_.insert(rev);
                out_[e.dst].push_back(rev);
                in_[e.src].push_back(rev);
            }
        }
    }

    void remove_edge(const edge_type& e) {
        auto erase_from = [](auto& vec, const edge_type& x) {
            vec.erase(std::remove(vec.begin(), vec.end(), x), vec.end());
        };

        if (edges_.erase(e)) {
            erase_from(out_[e.src], e);
            erase_from(in_[e.dst], e);
        }

        if (dir_ == direction::undirected && !(e.src == e.dst)) {
            edge_type rev { e.dst, e.src, e.label };
            if (edges_.erase(rev)) {
                erase_from(out_[rev.src], rev);
                erase_from(in_[rev.dst], rev);
            }
        }
    }

    void remove_vertex(const Vertex& v) {
        if (!vertices_.contains(v)) return;

        auto outgoing = out_[v];
        auto incoming = in_[v];

        for (const auto& e : outgoing) remove_edge(e);
        for (const auto& e : incoming) remove_edge(e);

        out_.erase(v);
        in_.erase(v);
        vertices_.erase(v);
    }

    [[nodiscard]] const std::unordered_set<Vertex>& vertices() const noexcept { return vertices_; }
    [[nodiscard]] const edge_set<Vertex, EdgeLabel>& edges() const noexcept { return edges_; }

    [[nodiscard]] std::vector<edge_type> out_edges(const Vertex& v) const {
        if (auto it = out_.find(v); it != out_.end()) return it->second;
        return {};
    }

    [[nodiscard]] std::vector<edge_type> in_edges(const Vertex& v) const {
        if (auto it = in_.find(v); it != in_.end()) return it->second;
        return {};
    }

    [[nodiscard]] std::vector<edge_type> all_edges(const Vertex& s, const Vertex& d) const {
        std::vector<edge_type> r;
        if (auto it = out_.find(s); it != out_.end()) {
            for (const auto& e : it->second) {
                if (e.dst == d) r.push_back(e);
            }
        }
        return r;
    }

    [[nodiscard]] std::size_t nb_vertex() const noexcept { return vertices_.size(); }
    [[nodiscard]] std::size_t nb_edges() const noexcept { return edges_.size(); }
};

// ============================================================
// Common graph API base
// ============================================================

template<VertexLike Vertex, class EdgeLabel = unit>
class graph_view_base {
public:
    using vertex_type = Vertex;
    using edge_label_type = EdgeLabel;
    using edge_type = edge<Vertex, EdgeLabel>;
    using storage_type = adjacency_storage<Vertex, EdgeLabel>;

protected:
    std::shared_ptr<const storage_type> storage_;

    explicit graph_view_base(std::shared_ptr<const storage_type> s)
        : storage_(std::move(s)) {}

public:
    graph_view_base() : storage_(std::make_shared<storage_type>()) {}

    [[nodiscard]] direction dir() const noexcept { return storage_->dir(); }
    [[nodiscard]] bool is_directed() const noexcept { return dir() == direction::directed; }

    [[nodiscard]] bool mem_vertex(const Vertex& v) const { return storage_->mem_vertex(v); }
    [[nodiscard]] bool mem_edge(const Vertex& s, const Vertex& d) const { return storage_->mem_edge(s, d); }
    [[nodiscard]] bool mem_edge_e(const edge_type& e) const { return storage_->mem_edge_e(e); }

    [[nodiscard]] std::size_t nb_vertex() const noexcept { return storage_->nb_vertex(); }
    [[nodiscard]] std::size_t nb_edges() const noexcept { return storage_->nb_edges(); }

    template<class F>
    void iter_vertex(F&& f) const {
        for (const auto& v : storage_->vertices()) std::invoke(f, v);
    }

    template<class F, class Acc>
    [[nodiscard]] Acc fold_vertex(F&& f, Acc init) const {
        for (const auto& v : storage_->vertices()) {
            init = std::invoke(f, v, std::move(init));
        }
        return init;
    }

    template<class F>
    void iter_edges(F&& f) const {
        for (const auto& e : storage_->edges()) std::invoke(f, e.src, e.dst);
    }

    template<class F>
    void iter_edges_e(F&& f) const {
        for (const auto& e : storage_->edges()) std::invoke(f, e);
    }

    template<class F, class Acc>
    [[nodiscard]] Acc fold_edges(F&& f, Acc init) const {
        for (const auto& e : storage_->edges()) {
            init = std::invoke(f, e.src, e.dst, std::move(init));
        }
        return init;
    }

    template<class F, class Acc>
    [[nodiscard]] Acc fold_edges_e(F&& f, Acc init) const {
        for (const auto& e : storage_->edges()) {
            init = std::invoke(f, e, std::move(init));
        }
        return init;
    }

    template<class F>
    void iter_succ(const Vertex& v, F&& f) const {
        for (const auto& e : storage_->out_edges(v)) std::invoke(f, e.dst);
    }

    template<class F>
    void iter_pred(const Vertex& v, F&& f) const {
        for (const auto& e : storage_->in_edges(v)) std::invoke(f, e.src);
    }

    template<class F>
    void iter_succ_e(const Vertex& v, F&& f) const {
        for (const auto& e : storage_->out_edges(v)) std::invoke(f, e);
    }

    template<class F>
    void iter_pred_e(const Vertex& v, F&& f) const {
        for (const auto& e : storage_->in_edges(v)) std::invoke(f, e);
    }

    [[nodiscard]] std::vector<edge_type> succ_e(const Vertex& v) const {
        return storage_->out_edges(v);
    }

    [[nodiscard]] std::vector<edge_type> pred_e(const Vertex& v) const {
        return storage_->in_edges(v);
    }

    [[nodiscard]] std::vector<Vertex> succ(const Vertex& v) const {
        std::vector<Vertex> r;
        for (const auto& e : storage_->out_edges(v)) r.push_back(e.dst);
        return r;
    }

    [[nodiscard]] std::vector<Vertex> pred(const Vertex& v) const {
        std::vector<Vertex> r;
        for (const auto& e : storage_->in_edges(v)) r.push_back(e.src);
        return r;
    }

    [[nodiscard]] std::vector<edge_type> find_all_edges(const Vertex& s, const Vertex& d) const {
        return storage_->all_edges(s, d);
    }

    [[nodiscard]] const storage_type& storage() const noexcept { return *storage_; }
};

// ============================================================
// Persistent / immutable graph
// Similar to OCamlGraph Sig.P spirit
// ============================================================

template<VertexLike Vertex, class EdgeLabel = unit>
class persistent_graph final : public graph_view_base<Vertex, EdgeLabel> {
public:
    using base = graph_view_base<Vertex, EdgeLabel>;
    using typename base::edge_type;
    using typename base::storage_type;

    persistent_graph()
        : base(std::make_shared<storage_type>()) {}

    explicit persistent_graph(direction dir)
        : base(std::make_shared<storage_type>(dir)) {}

private:
    explicit persistent_graph(std::shared_ptr<const storage_type> s)
        : base(std::move(s)) {}

public:
    [[nodiscard]] persistent_graph add_vertex(const Vertex& v) const {
        auto copy = std::make_shared<storage_type>(this->storage());
        copy->add_vertex(v);
        return persistent_graph(copy);
    }

    [[nodiscard]] persistent_graph add_edge(const Vertex& src, const Vertex& dst, const EdgeLabel& label = {}) const {
        auto copy = std::make_shared<storage_type>(this->storage());
        copy->add_edge(edge_type{src, dst, label});
        return persistent_graph(copy);
    }

    [[nodiscard]] persistent_graph add_edge_e(const edge_type& e) const {
        auto copy = std::make_shared<storage_type>(this->storage());
        copy->add_edge(e);
        return persistent_graph(copy);
    }

    [[nodiscard]] persistent_graph remove_vertex(const Vertex& v) const {
        auto copy = std::make_shared<storage_type>(this->storage());
        copy->remove_vertex(v);
        return persistent_graph(copy);
    }

    [[nodiscard]] persistent_graph remove_edge(const Vertex& src, const Vertex& dst, const EdgeLabel& label = {}) const {
        auto copy = std::make_shared<storage_type>(this->storage());
        copy->remove_edge(edge_type{src, dst, label});
        return persistent_graph(copy);
    }

    [[nodiscard]] persistent_graph remove_edge_e(const edge_type& e) const {
        auto copy = std::make_shared<storage_type>(this->storage());
        copy->remove_edge(e);
        return persistent_graph(copy);
    }
};

// ============================================================
// Imperative / mutable graph
// Similar to OCamlGraph Sig.I spirit
// ============================================================

template<VertexLike Vertex, class EdgeLabel = unit>
class imperative_graph final {
public:
    using vertex_type = Vertex;
    using edge_label_type = EdgeLabel;
    using edge_type = edge<Vertex, EdgeLabel>;
    using storage_type = adjacency_storage<Vertex, EdgeLabel>;

private:
    storage_type storage_;

public:
    imperative_graph() = default;
    explicit imperative_graph(direction dir) : storage_(dir) {}

    [[nodiscard]] direction dir() const noexcept { return storage_.dir(); }
    [[nodiscard]] bool is_directed() const noexcept { return dir() == direction::directed; }

    void add_vertex(const Vertex& v) { storage_.add_vertex(v); }
    void add_edge(const Vertex& src, const Vertex& dst, const EdgeLabel& label = {}) {
        storage_.add_edge(edge_type{src, dst, label});
    }
    void add_edge_e(const edge_type& e) { storage_.add_edge(e); }

    void remove_vertex(const Vertex& v) { storage_.remove_vertex(v); }
    void remove_edge(const Vertex& src, const Vertex& dst, const EdgeLabel& label = {}) {
        storage_.remove_edge(edge_type{src, dst, label});
    }
    void remove_edge_e(const edge_type& e) { storage_.remove_edge(e); }

    [[nodiscard]] bool mem_vertex(const Vertex& v) const { return storage_.mem_vertex(v); }
    [[nodiscard]] bool mem_edge(const Vertex& s, const Vertex& d) const { return storage_.mem_edge(s, d); }
    [[nodiscard]] bool mem_edge_e(const edge_type& e) const { return storage_.mem_edge_e(e); }

    [[nodiscard]] std::size_t nb_vertex() const noexcept { return storage_.nb_vertex(); }
    [[nodiscard]] std::size_t nb_edges() const noexcept { return storage_.nb_edges(); }

    template<class F>
    void iter_vertex(F&& f) const {
        for (const auto& v : storage_.vertices()) std::invoke(f, v);
    }

    template<class F, class Acc>
    [[nodiscard]] Acc fold_vertex(F&& f, Acc init) const {
        for (const auto& v : storage_.vertices()) {
            init = std::invoke(f, v, std::move(init));
        }
        return init;
    }

    template<class F>
    void iter_edges(F&& f) const {
        for (const auto& e : storage_.edges()) std::invoke(f, e.src, e.dst);
    }

    template<class F>
    void iter_edges_e(F&& f) const {
        for (const auto& e : storage_.edges()) std::invoke(f, e);
    }

    template<class F, class Acc>
    [[nodiscard]] Acc fold_edges(F&& f, Acc init) const {
        for (const auto& e : storage_.edges()) {
            init = std::invoke(f, e.src, e.dst, std::move(init));
        }
        return init;
    }

    template<class F, class Acc>
    [[nodiscard]] Acc fold_edges_e(F&& f, Acc init) const {
        for (const auto& e : storage_.edges()) {
            init = std::invoke(f, e, std::move(init));
        }
        return init;
    }

    template<class F>
    void iter_succ(const Vertex& v, F&& f) const {
        for (const auto& e : storage_.out_edges(v)) std::invoke(f, e.dst);
    }

    template<class F>
    void iter_pred(const Vertex& v, F&& f) const {
        for (const auto& e : storage_.in_edges(v)) std::invoke(f, e.src);
    }

    template<class F>
    void iter_succ_e(const Vertex& v, F&& f) const {
        for (const auto& e : storage_.out_edges(v)) std::invoke(f, e);
    }

    template<class F>
    void iter_pred_e(const Vertex& v, F&& f) const {
        for (const auto& e : storage_.in_edges(v)) std::invoke(f, e);
    }

    [[nodiscard]] std::vector<edge_type> succ_e(const Vertex& v) const { return storage_.out_edges(v); }
    [[nodiscard]] std::vector<edge_type> pred_e(const Vertex& v) const { return storage_.in_edges(v); }

    [[nodiscard]] std::vector<Vertex> succ(const Vertex& v) const {
        std::vector<Vertex> r;
        for (const auto& e : storage_.out_edges(v)) r.push_back(e.dst);
        return r;
    }

    [[nodiscard]] std::vector<Vertex> pred(const Vertex& v) const {
        std::vector<Vertex> r;
        for (const auto& e : storage_.in_edges(v)) r.push_back(e.src);
        return r;
    }

    [[nodiscard]] std::vector<edge_type> find_all_edges(const Vertex& s, const Vertex& d) const {
        return storage_.all_edges(s, d);
    }

    [[nodiscard]] persistent_graph<Vertex, EdgeLabel> freeze() const {
        auto copy = std::make_shared<typename persistent_graph<Vertex, EdgeLabel>::storage_type>(storage_);
        return persistent_graph<Vertex, EdgeLabel>(copy);
    }
};

// ============================================================
// Builders
// Mirrors the spirit of Graph.Builder common construction API
// ============================================================

namespace builder {

template<class Graph>
class imperative_builder {
public:
    using graph_type = Graph;
    using vertex_type = typename graph_type::vertex_type;
    using edge_type = typename graph_type::edge_type;
    using edge_label_type = typename graph_type::edge_label_type;

private:
    graph_type g_;

public:
    imperative_builder() = default;
    explicit imperative_builder(direction dir) : g_(dir) {}

    imperative_builder& vertex(const vertex_type& v) {
        g_.add_vertex(v);
        return *this;
    }

    imperative_builder& edge(const vertex_type& s, const vertex_type& d, const edge_label_type& l = {}) {
        g_.add_edge(s, d, l);
        return *this;
    }

    imperative_builder& edge_e(const edge_type& e) {
        g_.add_edge_e(e);
        return *this;
    }

    [[nodiscard]] graph_type build() const {
        return g_;
    }
};

template<class Graph>
class persistent_builder {
public:
    using graph_type = Graph;
    using vertex_type = typename graph_type::vertex_type;
    using edge_type = typename graph_type::edge_type;
    using edge_label_type = typename graph_type::edge_label_type;

private:
    graph_type g_;

public:
    persistent_builder() = default;
    explicit persistent_builder(direction dir) : g_(dir) {}

    persistent_builder& vertex(const vertex_type& v) {
        g_ = g_.add_vertex(v);
        return *this;
    }

    persistent_builder& edge(const vertex_type& s, const vertex_type& d, const edge_label_type& l = {}) {
        g_ = g_.add_edge(s, d, l);
        return *this;
    }

    persistent_builder& edge_e(const edge_type& e) {
        g_ = g_.add_edge_e(e);
        return *this;
    }

    [[nodiscard]] graph_type build() const {
        return g_;
    }
};

} // namespace builder

// ============================================================
// Algorithms
// ============================================================

namespace algo {

/**
 * @brief Status of a topological-sort computation.
 *
 * A topological order is only defined for directed acyclic graphs.
 * This status reports whether ordering succeeded, failed due to a detected
 * cycle, or was rejected because the graph is undirected.
 */
enum class topological_sort_status {
    ok,
    not_directed,
    has_cycle
};

/**
 * @brief Rich result of topological sorting.
 *
 * @tparam Vertex Vertex type of the graph.
 *
 * @invariant `status == topological_sort_status::ok` implies `order` contains
 * all vertices exactly once.
 * @invariant `status == topological_sort_status::has_cycle` implies
 * `cycle_witness` is non-empty and starts/ends at the same vertex.
 */
template<class Vertex>
struct topological_sort_result {
    topological_sort_status status { topological_sort_status::ok };
    std::vector<Vertex> order;
    std::vector<Vertex> cycle_witness;

    /**
     * @brief Returns true iff a full topological order was produced.
     */
    [[nodiscard]] bool has_order() const noexcept {
        return status == topological_sort_status::ok;
    }

    /**
     * @brief Returns true iff a directed cycle witness is available.
     */
    [[nodiscard]] bool has_cycle() const noexcept {
        return status == topological_sort_status::has_cycle;
    }
};

namespace detail {

template<class Graph>
[[nodiscard]] std::vector<typename Graph::vertex_type>
extract_cycle_witness_from_residual(
    const Graph& g,
    const std::unordered_set<typename Graph::vertex_type>& residual
) {
    using V = typename Graph::vertex_type;
    std::unordered_map<V, int> color;
    std::unordered_map<V, V> parent;
    std::vector<V> cycle;

    for (const auto& v : residual) color[v] = 0;

    std::function<bool(const V&)> dfs = [&](const V& v) -> bool {
        color[v] = 1;
        for (const auto& n : g.succ(v)) {
            if (!residual.contains(n)) continue;
            if (color[n] == 0) {
                parent[n] = v;
                if (dfs(n)) return true;
                continue;
            }
            if (color[n] == 1) {
                cycle.push_back(n);
                V cur = v;
                while (!(cur == n)) {
                    cycle.push_back(cur);
                    cur = parent.at(cur);
                }
                cycle.push_back(n);
                std::reverse(cycle.begin(), cycle.end());
                return true;
            }
        }
        color[v] = 2;
        return false;
    };

    for (const auto& [v, c] : color) {
        if (c == 0 && dfs(v)) break;
    }
    return cycle;
}

} // namespace detail

template<class Graph, class Pred>
[[nodiscard]] std::optional<typename Graph::vertex_type>
find_vertex_if(const Graph& g, Pred&& pred) {
    std::optional<typename Graph::vertex_type> out;
    g.iter_vertex([&](const auto& v) {
        if (!out && std::invoke(pred, v)) out = v;
    });
    return out;
}

template<class Graph, class LabelOf>
[[nodiscard]] std::optional<typename Graph::vertex_type>
find_vertex_by_label(const Graph& g, const std::string& wanted, LabelOf&& label_of) {
    return find_vertex_if(g, [&](const auto& v) {
        return std::invoke(label_of, v) == wanted;
    });
}

template<class Graph>
[[nodiscard]] bool reachable(
    const Graph& g,
    const typename Graph::vertex_type& src,
    const typename Graph::vertex_type& dst
) {
    using V = typename Graph::vertex_type;
    if (!g.mem_vertex(src) || !g.mem_vertex(dst)) return false;
    if (src == dst) return true;

    std::queue<V> q;
    std::unordered_set<V> seen;
    q.push(src);
    seen.insert(src);

    while (!q.empty()) {
        auto v = q.front();
        q.pop();
        for (const auto& n : g.succ(v)) {
            if (seen.contains(n)) continue;
            if (n == dst) return true;
            seen.insert(n);
            q.push(n);
        }
    }
    return false;
}

template<class Graph>
[[nodiscard]] std::optional<std::vector<typename Graph::vertex_type>>
shortest_path(
    const Graph& g,
    const typename Graph::vertex_type& src,
    const typename Graph::vertex_type& dst
) {
    using V = typename Graph::vertex_type;
    if (!g.mem_vertex(src) || !g.mem_vertex(dst)) return std::nullopt;

    std::queue<V> q;
    std::unordered_map<V, V> parent;
    std::unordered_set<V> seen;

    q.push(src);
    seen.insert(src);

    bool found = false;
    while (!q.empty() && !found) {
        V cur = q.front();
        q.pop();
        for (const auto& n : g.succ(cur)) {
            if (seen.contains(n)) continue;
            seen.insert(n);
            parent[n] = cur;
            if (n == dst) {
                found = true;
                break;
            }
            q.push(n);
        }
    }

    if (!found && src != dst) return std::nullopt;

    std::vector<V> path;
    V cur = dst;
    path.push_back(cur);
    while (!(cur == src)) {
        cur = parent.at(cur);
        path.push_back(cur);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

template<class Graph>
[[nodiscard]] std::vector<typename Graph::vertex_type>
bfs_order(const Graph& g, const typename Graph::vertex_type& root) {
    using V = typename Graph::vertex_type;
    std::vector<V> out;
    if (!g.mem_vertex(root)) return out;

    std::queue<V> q;
    std::unordered_set<V> seen;
    q.push(root);
    seen.insert(root);

    while (!q.empty()) {
        auto v = q.front();
        q.pop();
        out.push_back(v);
        for (const auto& n : g.succ(v)) {
            if (seen.insert(n).second) q.push(n);
        }
    }
    return out;
}

/**
 * @brief Computes a topological order for directed graphs.
 *
 * Uses Kahn's algorithm and reports cycle failures explicitly.
 *
 * @tparam Graph Graph type exposing the Grafitt graph interface.
 * @param g Input graph.
 * @return A `topological_sort_result` containing either a complete order
 *         (`status == ok`) or a cycle witness (`status == has_cycle`), or
 *         `status == not_directed` for undirected graphs.
 *
 * @pre The graph type models Grafitt's standard graph API.
 * @post On success, the order contains each vertex exactly once.
 * @complexity O(|V| + |E|) expected.
 * @exception No Grafitt-specific exceptions are thrown by this routine.
 */
template<class Graph>
[[nodiscard]] topological_sort_result<typename Graph::vertex_type>
topological_sort(const Graph& g) {
    using V = typename Graph::vertex_type;
    topological_sort_result<V> out;

    if (!g.is_directed()) {
        out.status = topological_sort_status::not_directed;
        return out;
    }

    std::vector<V> verts;
    g.iter_vertex([&](const auto& v) { verts.push_back(v); });
    out.order.reserve(verts.size());

    std::unordered_map<V, std::size_t> indegree;
    indegree.reserve(verts.size());
    for (const auto& v : verts) indegree.emplace(v, 0);
    for (const auto& v : verts) {
        for (const auto& n : g.succ(v)) {
            if (auto it = indegree.find(n); it != indegree.end()) ++(it->second);
        }
    }

    std::queue<V> ready;
    for (const auto& [v, deg] : indegree) {
        if (deg == 0) ready.push(v);
    }

    while (!ready.empty()) {
        V v = ready.front();
        ready.pop();
        out.order.push_back(v);
        for (const auto& n : g.succ(v)) {
            auto it = indegree.find(n);
            if (it == indegree.end()) continue;
            if (it->second == 0) continue;
            --(it->second);
            if (it->second == 0) ready.push(n);
        }
    }

    if (out.order.size() == verts.size()) {
        out.status = topological_sort_status::ok;
        return out;
    }

    out.status = topological_sort_status::has_cycle;
    std::unordered_set<V> residual(verts.begin(), verts.end());
    for (const auto& v : out.order) residual.erase(v);
    out.cycle_witness = detail::extract_cycle_witness_from_residual(g, residual);
    return out;
}

/**
 * @brief Computes a stable topological order using caller-provided ordering.
 *
 * Stability means that when multiple zero-indegree vertices are available,
 * the smallest one under `Compare` is chosen first.
 *
 * @tparam Graph Graph type exposing the Grafitt graph interface.
 * @tparam Compare Strict weak ordering over vertices.
 * @param g Input graph.
 * @param comp Vertex comparator used to break ties.
 * @return Same contract as `topological_sort`.
 *
 * @complexity O((|V| + |E|) log |V|) expected.
 */
template<class Graph, class Compare>
[[nodiscard]] topological_sort_result<typename Graph::vertex_type>
topological_sort_stable(const Graph& g, Compare comp) {
    using V = typename Graph::vertex_type;
    topological_sort_result<V> out;

    if (!g.is_directed()) {
        out.status = topological_sort_status::not_directed;
        return out;
    }

    std::vector<V> verts;
    g.iter_vertex([&](const auto& v) { verts.push_back(v); });
    out.order.reserve(verts.size());

    std::unordered_map<V, std::size_t> indegree;
    indegree.reserve(verts.size());
    for (const auto& v : verts) indegree.emplace(v, 0);
    for (const auto& v : verts) {
        for (const auto& n : g.succ(v)) {
            if (auto it = indegree.find(n); it != indegree.end()) ++(it->second);
        }
    }

    std::multiset<V, Compare> ready(comp);
    for (const auto& [v, deg] : indegree) {
        if (deg == 0) ready.insert(v);
    }

    while (!ready.empty()) {
        auto it = ready.begin();
        V v = *it;
        ready.erase(it);
        out.order.push_back(v);
        for (const auto& n : g.succ(v)) {
            auto nd = indegree.find(n);
            if (nd == indegree.end()) continue;
            if (nd->second == 0) continue;
            --(nd->second);
            if (nd->second == 0) ready.insert(n);
        }
    }

    if (out.order.size() == verts.size()) {
        out.status = topological_sort_status::ok;
        return out;
    }

    out.status = topological_sort_status::has_cycle;
    std::unordered_set<V> residual(verts.begin(), verts.end());
    for (const auto& v : out.order) residual.erase(v);
    out.cycle_witness = detail::extract_cycle_witness_from_residual(g, residual);
    return out;
}

/**
 * @brief Convenience stable topological sort using `std::less<>`.
 *
 * @tparam Graph Graph type exposing the Grafitt graph interface.
 * @param g Input graph.
 * @return Same contract as `topological_sort_stable`.
 */
template<class Graph>
[[nodiscard]] topological_sort_result<typename Graph::vertex_type>
topological_sort_stable(const Graph& g) {
    return topological_sort_stable(g, std::less<>{});
}

/**
 * @brief Returns true iff the directed graph is acyclic.
 *
 * Undirected graphs return false because DAG semantics do not apply.
 */
template<class Graph>
[[nodiscard]] bool is_dag(const Graph& g) {
    const auto topo = topological_sort(g);
    return topo.status == topological_sort_status::ok;
}

/**
 * @brief Extracts one directed cycle witness, if any.
 *
 * @return A cycle whose first and last vertices are equal, or `std::nullopt`
 *         if no cycle witness is available.
 */
template<class Graph>
[[nodiscard]] std::optional<std::vector<typename Graph::vertex_type>>
cycle_witness(const Graph& g) {
    const auto topo = topological_sort(g);
    if (topo.status != topological_sort_status::has_cycle) return std::nullopt;
    return topo.cycle_witness;
}

/**
 * @brief Result object for weakly connected component decomposition.
 *
 * Weak connectivity is computed on the underlying undirected view of the graph.
 * For undirected graphs, this is equivalent to standard connected components.
 *
 * @tparam Vertex Vertex type.
 *
 * @invariant `component_of` maps each discovered vertex to exactly one
 * valid index in `components`.
 */
template<class Vertex>
struct weakly_connected_components_result {
    std::vector<std::vector<Vertex>> components;
    std::unordered_map<Vertex, std::size_t> component_of;

    /**
     * @brief Returns number of weakly connected components.
     */
    [[nodiscard]] std::size_t component_count() const noexcept {
        return components.size();
    }

    /**
     * @brief Returns true iff @p v was assigned to a component.
     */
    [[nodiscard]] bool contains_vertex(const Vertex& v) const {
        return component_of.contains(v);
    }

    /**
     * @brief Returns component id of @p v if present.
     */
    [[nodiscard]] std::optional<std::size_t> component_id_of(const Vertex& v) const {
        if (auto it = component_of.find(v); it != component_of.end()) return it->second;
        return std::nullopt;
    }

    /**
     * @brief Returns pointer to component-vertex list for @p id.
     */
    [[nodiscard]] const std::vector<Vertex>* component_vertices(std::size_t id) const noexcept {
        if (id >= components.size()) return nullptr;
        return &components[id];
    }
};

/**
 * @brief Result object for strongly connected component decomposition.
 *
 * @tparam Vertex Vertex type.
 *
 * @invariant `component_of` maps each discovered vertex to exactly one
 * valid index in `components`.
 */
template<class Vertex>
struct strongly_connected_components_result {
    std::vector<std::vector<Vertex>> components;
    std::unordered_map<Vertex, std::size_t> component_of;

    /**
     * @brief Returns number of strongly connected components.
     */
    [[nodiscard]] std::size_t component_count() const noexcept {
        return components.size();
    }

    /**
     * @brief Returns true iff @p v was assigned to a component.
     */
    [[nodiscard]] bool contains_vertex(const Vertex& v) const {
        return component_of.contains(v);
    }

    /**
     * @brief Returns component id of @p v if present.
     */
    [[nodiscard]] std::optional<std::size_t> component_id_of(const Vertex& v) const {
        if (auto it = component_of.find(v); it != component_of.end()) return it->second;
        return std::nullopt;
    }

    /**
     * @brief Returns pointer to component-vertex list for @p id.
     */
    [[nodiscard]] const std::vector<Vertex>* component_vertices(std::size_t id) const noexcept {
        if (id >= components.size()) return nullptr;
        return &components[id];
    }
};

/**
 * @brief Witness relating one original edge to one condensation edge.
 *
 * @tparam Vertex Vertex type from the original graph.
 */
template<class Vertex>
struct condensation_edge_witness {
    std::size_t src_component { 0 };
    std::size_t dst_component { 0 };
    Vertex src_vertex {};
    Vertex dst_vertex {};
};

/**
 * @brief Result object describing SCC condensation DAG and provenance.
 *
 * @tparam Vertex Vertex type of the original graph.
 *
 * `dag` has one vertex per SCC (component id in `[0, component_count())`)
 * and one directed edge for each inter-component reachability edge induced by
 * the original graph.
 */
template<class Vertex>
struct condensation_graph_result {
    using condensation_graph_type = imperative_graph<std::size_t>;

    condensation_graph_type dag { direction::directed };
    std::vector<std::vector<Vertex>> components;
    std::unordered_map<Vertex, std::size_t> component_of;
    std::vector<condensation_edge_witness<Vertex>> edge_witnesses;

    /**
     * @brief Returns number of SCC nodes in the condensation DAG.
     */
    [[nodiscard]] std::size_t component_count() const noexcept {
        return components.size();
    }

    /**
     * @brief Returns true iff there is at least one SCC in the result.
     */
    [[nodiscard]] bool non_empty() const noexcept {
        return !components.empty();
    }
};

/**
 * @brief Validation status for condensation graph coherence checks.
 */
enum class condensation_validation_status {
    ok,
    missing_component_vertex,
    empty_component,
    duplicate_vertex_across_components,
    component_mapping_missing,
    component_mapping_mismatch,
    graph_vertex_uncovered,
    graph_vertex_component_out_of_bounds,
    missing_condensation_edge,
    witness_component_out_of_bounds,
    witness_component_mismatch,
    witness_original_edge_missing,
    condensation_not_dag
};

/**
 * @brief Validation status for decomposition/cover results.
 */
enum class cover_validation_status {
    ok,
    empty_piece,
    duplicate_vertex_assignment,
    uncovered_vertex,
    invalid_component_assignment,
    invalid_piece_edge,
    piece_not_acyclic,
    invalid_parent_relation,
    invalid_tree_edge_count
};

/**
 * @brief Shared metrics for cover/decomposition outputs.
 */
struct cover_quality_metrics {
    std::size_t piece_count = 0;
    std::size_t covered_vertices = 0;
    std::size_t overlap_vertices = 0;
    std::size_t max_piece_vertices = 0;
    std::size_t total_witness_edges = 0;
};

/**
 * @brief Objective selector for decomposition/cover construction.
 */
enum class cover_objective {
    minimize_piece_count,
    balance_piece_sizes,
    maximize_locality
};

/**
 * @brief Strategy selector for condensation DAG cover construction.
 */
enum class condensation_cover_strategy {
    weak_component_partition,
    topo_chunked
};

/**
 * @brief Strategy selector for BFS-forest cover construction.
 */
enum class bfs_forest_cover_strategy {
    weak_component_roots,
    degree_desc_root_priority
};

/**
 * @brief Options for condensation DAG cover construction.
 */
struct condensation_dag_cover_options {
    condensation_cover_strategy strategy { condensation_cover_strategy::weak_component_partition };
    cover_objective objective { cover_objective::minimize_piece_count };
    std::size_t max_piece_components = 0;
};

/**
 * @brief Options for BFS-forest cover construction.
 */
struct bfs_forest_cover_options {
    bfs_forest_cover_strategy strategy { bfs_forest_cover_strategy::weak_component_roots };
    cover_objective objective { cover_objective::minimize_piece_count };
    std::size_t max_piece_vertices = 0;
};

/**
 * @brief Scored view over a cover candidate.
 */
template<class Cover>
struct cover_scored {
    Cover cover;
    double score = 0.0;
    cover_objective objective { cover_objective::minimize_piece_count };
};

/**
 * @brief One DAG piece in a condensation-based DAG cover.
 *
 * @tparam Vertex Original graph vertex type.
 */
template<class Vertex>
struct condensation_dag_cover_piece {
    std::size_t piece_id { 0 };
    imperative_graph<std::size_t> dag_piece { direction::directed };
    std::vector<std::size_t> component_ids;
    std::vector<Vertex> original_vertices;
    std::vector<condensation_edge_witness<Vertex>> edge_witnesses;
};

/**
 * @brief Result of covering condensation DAG by DAG pieces.
 *
 * @tparam Vertex Original graph vertex type.
 */
template<class Vertex>
struct condensation_dag_cover_result {
    condensation_graph_result<Vertex> condensation;
    std::vector<condensation_dag_cover_piece<Vertex>> pieces;
    std::unordered_map<std::size_t, std::size_t> component_to_piece;
    cover_quality_metrics metrics;
};

/**
 * @brief Validation result for condensation DAG covers.
 */
template<class Vertex>
struct condensation_dag_cover_validation_result {
    cover_validation_status status { cover_validation_status::ok };
    std::optional<std::size_t> offending_piece;
    std::optional<std::size_t> offending_component;
    std::optional<std::pair<std::size_t, std::size_t>> offending_component_edge;

    [[nodiscard]] bool ok() const noexcept { return status == cover_validation_status::ok; }
};

/**
 * @brief Piece in a BFS-forest graph cover.
 *
 * @tparam Vertex Vertex type.
 * @tparam EdgeLabel Edge label type.
 */
template<class Vertex, class EdgeLabel>
struct bfs_forest_cover_piece {
    std::size_t piece_id { 0 };
    Vertex root {};
    imperative_graph<Vertex, EdgeLabel> forest_piece { direction::directed };
    std::vector<Vertex> vertices;
    std::vector<edge<Vertex, EdgeLabel>> tree_edges;
    std::unordered_map<Vertex, std::optional<Vertex>> parent_of;
};

/**
 * @brief Result of a BFS-forest cover/decomposition.
 *
 * @tparam Vertex Vertex type.
 * @tparam EdgeLabel Edge label type.
 */
template<class Vertex, class EdgeLabel>
struct bfs_forest_cover_result {
    std::vector<bfs_forest_cover_piece<Vertex, EdgeLabel>> pieces;
    std::unordered_map<Vertex, std::size_t> vertex_to_piece;
    cover_quality_metrics metrics;
};

/**
 * @brief Validation result for BFS-forest covers.
 */
template<class Vertex>
struct bfs_forest_cover_validation_result {
    cover_validation_status status { cover_validation_status::ok };
    std::optional<std::size_t> offending_piece;
    std::optional<Vertex> offending_vertex;
    std::optional<std::pair<Vertex, Vertex>> offending_parent_link;

    [[nodiscard]] bool ok() const noexcept { return status == cover_validation_status::ok; }
};

/**
 * @brief Detailed result of condensation graph validation.
 *
 * @tparam Vertex Vertex type of original graph.
 */
template<class Vertex>
struct condensation_validation_result {
    condensation_validation_status status { condensation_validation_status::ok };
    std::optional<Vertex> offending_vertex;
    std::optional<std::size_t> offending_component;
    std::optional<std::pair<std::size_t, std::size_t>> offending_component_edge;
    std::optional<condensation_edge_witness<Vertex>> offending_witness;
    std::vector<std::size_t> condensation_cycle_witness;

    /**
     * @brief Returns true iff validation succeeded.
     */
    [[nodiscard]] bool ok() const noexcept {
        return status == condensation_validation_status::ok;
    }
};

/**
 * @brief Computes weakly connected components using BFS on undirected view.
 *
 * For directed graphs, this ignores edge orientation by traversing both
 * successors and predecessors.
 *
 * @tparam Graph Graph type exposing Grafitt graph API.
 * @param g Input graph.
 * @return Component decomposition with vertex-to-component mapping.
 *
 * @complexity O(|V| + |E|) expected.
 */
template<class Graph>
[[nodiscard]] weakly_connected_components_result<typename Graph::vertex_type>
weakly_connected_components(const Graph& g) {
    using V = typename Graph::vertex_type;
    weakly_connected_components_result<V> out;

    std::unordered_set<V> seen;
    std::vector<V> verts;
    verts.reserve(g.nb_vertex());
    g.iter_vertex([&](const auto& v) { verts.push_back(v); });

    for (const auto& root : verts) {
        if (seen.contains(root)) continue;
        const std::size_t cid = out.components.size();
        out.components.emplace_back();

        std::queue<V> q;
        q.push(root);
        seen.insert(root);

        while (!q.empty()) {
            V cur = q.front();
            q.pop();
            out.component_of[cur] = cid;
            out.components.back().push_back(cur);

            for (const auto& n : g.succ(cur)) {
                if (seen.insert(n).second) q.push(n);
            }
            for (const auto& p : g.pred(cur)) {
                if (seen.insert(p).second) q.push(p);
            }
        }
    }

    return out;
}

/**
 * @brief Computes strongly connected components via Tarjan's algorithm.
 *
 * For undirected graphs (modeled with symmetric adjacency in Grafitt),
 * this produces connected components.
 *
 * @tparam Graph Graph type exposing Grafitt graph API.
 * @param g Input graph.
 * @return SCC decomposition with vertex-to-component mapping.
 *
 * @complexity O(|V| + |E|) expected.
 */
template<class Graph>
[[nodiscard]] strongly_connected_components_result<typename Graph::vertex_type>
strongly_connected_components(const Graph& g) {
    using V = typename Graph::vertex_type;
    strongly_connected_components_result<V> out;

    std::unordered_map<V, std::size_t> index_of;
    std::unordered_map<V, std::size_t> low_of;
    std::unordered_set<V> on_stack;
    std::vector<V> stack;
    std::size_t next_index = 0;

    std::function<void(const V&)> dfs = [&](const V& v) {
        index_of[v] = next_index;
        low_of[v] = next_index;
        ++next_index;

        stack.push_back(v);
        on_stack.insert(v);

        for (const auto& n : g.succ(v)) {
            if (!index_of.contains(n)) {
                dfs(n);
                low_of[v] = std::min(low_of[v], low_of[n]);
            } else if (on_stack.contains(n)) {
                low_of[v] = std::min(low_of[v], index_of[n]);
            }
        }

        if (low_of[v] == index_of[v]) {
            const std::size_t cid = out.components.size();
            out.components.emplace_back();
            while (true) {
                V w = stack.back();
                stack.pop_back();
                on_stack.erase(w);
                out.component_of[w] = cid;
                out.components.back().push_back(w);
                if (w == v) break;
            }
        }
    };

    g.iter_vertex([&](const auto& v) {
        if (!index_of.contains(v)) dfs(v);
    });

    return out;
}

namespace detail {

struct component_edge_key {
    std::size_t src { 0 };
    std::size_t dst { 0 };

    friend bool operator==(const component_edge_key&, const component_edge_key&) = default;
};

struct component_edge_key_hash {
    std::size_t operator()(const component_edge_key& key) const noexcept {
        return key.src ^ (key.dst << 1);
    }
};

} // namespace detail

/**
 * @brief Builds SCC condensation DAG from precomputed SCC decomposition.
 *
 * @tparam Graph Graph type exposing Grafitt graph API.
 * @param g Original graph.
 * @param scc SCC decomposition of @p g.
 * @return Condensation DAG with component provenance and edge witnesses.
 *
 * @pre `scc` corresponds to `g` and maps each graph vertex to an SCC id.
 * @post `result.dag` is directed and acyclic when inputs are coherent.
 * @complexity O(|V| + |E|) expected.
 */
template<class Graph>
[[nodiscard]] condensation_graph_result<typename Graph::vertex_type>
condensation_graph_from_scc(
    const Graph& g,
    const strongly_connected_components_result<typename Graph::vertex_type>& scc
) {
    using V = typename Graph::vertex_type;
    condensation_graph_result<V> out;

    out.components = scc.components;
    out.component_of = scc.component_of;

    for (std::size_t cid = 0; cid < out.components.size(); ++cid) {
        out.dag.add_vertex(cid);
    }

    std::unordered_set<detail::component_edge_key, detail::component_edge_key_hash> seen_edges;
    g.iter_edges_e([&](const auto& e) {
        auto src_it = out.component_of.find(e.src);
        auto dst_it = out.component_of.find(e.dst);
        if (src_it == out.component_of.end() || dst_it == out.component_of.end()) return;

        const std::size_t src_c = src_it->second;
        const std::size_t dst_c = dst_it->second;
        if (src_c == dst_c) return;

        detail::component_edge_key key { src_c, dst_c };
        if (!seen_edges.insert(key).second) return;

        out.dag.add_edge(src_c, dst_c);
        out.edge_witnesses.push_back(condensation_edge_witness<V> {
            .src_component = src_c,
            .dst_component = dst_c,
            .src_vertex = e.src,
            .dst_vertex = e.dst
        });
    });

    return out;
}

/**
 * @brief Computes SCC decomposition and its condensation DAG.
 *
 * @tparam Graph Graph type exposing Grafitt graph API.
 * @param g Input graph.
 * @return Condensation DAG and SCC provenance.
 *
 * @complexity O(|V| + |E|) expected.
 */
template<class Graph>
[[nodiscard]] condensation_graph_result<typename Graph::vertex_type>
condensation_graph(const Graph& g) {
    return condensation_graph_from_scc(g, strongly_connected_components(g));
}

/**
 * @brief Validates structural consistency of a condensation graph result.
 *
 * Checks vertex coverage, component id bounds, edge lifting correctness, and
 * acyclicity of the condensation DAG.
 *
 * @tparam Graph Original graph type.
 * @param g Original graph used to derive the condensation.
 * @param cond Candidate condensation result.
 * @return True iff the result is internally coherent and consistent with @p g.
 *
 * @complexity O(|V| + |E|) expected.
 */
template<class Graph>
[[nodiscard]] condensation_validation_result<typename Graph::vertex_type> validate_condensation_graph_detailed(
    const Graph& g,
    const condensation_graph_result<typename Graph::vertex_type>& cond
) {
    using V = typename Graph::vertex_type;
    condensation_validation_result<V> out;

    const std::size_t n = cond.components.size();
    for (std::size_t cid = 0; cid < n; ++cid) {
        if (!cond.dag.mem_vertex(cid)) {
            out.status = condensation_validation_status::missing_component_vertex;
            out.offending_component = cid;
            return out;
        }
        if (cond.components[cid].empty()) {
            out.status = condensation_validation_status::empty_component;
            out.offending_component = cid;
            return out;
        }
    }

    std::unordered_set<V> all_component_vertices;
    for (std::size_t cid = 0; cid < n; ++cid) {
        for (const auto& v : cond.components[cid]) {
            if (!all_component_vertices.insert(v).second) {
                out.status = condensation_validation_status::duplicate_vertex_across_components;
                out.offending_vertex = v;
                out.offending_component = cid;
                return out;
            }
            auto it = cond.component_of.find(v);
            if (it == cond.component_of.end()) {
                out.status = condensation_validation_status::component_mapping_missing;
                out.offending_vertex = v;
                out.offending_component = cid;
                return out;
            }
            if (it->second != cid) {
                out.status = condensation_validation_status::component_mapping_mismatch;
                out.offending_vertex = v;
                out.offending_component = cid;
                return out;
            }
        }
    }

    bool vertex_coverage_ok = true;
    g.iter_vertex([&](const auto& v) {
        if (!vertex_coverage_ok) return;
        auto it = cond.component_of.find(v);
        if (it == cond.component_of.end() || it->second >= n) {
            out.offending_vertex = v;
            if (it == cond.component_of.end()) {
                out.status = condensation_validation_status::graph_vertex_uncovered;
            } else {
                out.status = condensation_validation_status::graph_vertex_component_out_of_bounds;
                out.offending_component = it->second;
            }
            vertex_coverage_ok = false;
            return;
        }
        if (!all_component_vertices.contains(v)) {
            out.status = condensation_validation_status::graph_vertex_uncovered;
            out.offending_vertex = v;
            out.offending_component = it->second;
            vertex_coverage_ok = false;
            return;
        }
    });
    if (!vertex_coverage_ok) return out;

    bool lifted_edges_ok = true;
    g.iter_edges_e([&](const auto& e) {
        if (!lifted_edges_ok) return;
        auto src_it = cond.component_of.find(e.src);
        auto dst_it = cond.component_of.find(e.dst);
        if (src_it == cond.component_of.end() || dst_it == cond.component_of.end()) {
            lifted_edges_ok = false;
            out.status = condensation_validation_status::graph_vertex_uncovered;
            out.offending_vertex = (src_it == cond.component_of.end()) ? e.src : e.dst;
            return;
        }
        const std::size_t src_c = src_it->second;
        const std::size_t dst_c = dst_it->second;
        if (src_c == dst_c) return;
        if (!cond.dag.mem_edge(src_c, dst_c)) {
            lifted_edges_ok = false;
            out.status = condensation_validation_status::missing_condensation_edge;
            out.offending_component_edge = std::pair<std::size_t, std::size_t>{src_c, dst_c};
        }
    });
    if (!lifted_edges_ok) return out;

    for (const auto& witness : cond.edge_witnesses) {
        if (witness.src_component >= n || witness.dst_component >= n) {
            out.status = condensation_validation_status::witness_component_out_of_bounds;
            out.offending_witness = witness;
            return out;
        }
        if (!cond.dag.mem_edge(witness.src_component, witness.dst_component)) {
            out.status = condensation_validation_status::missing_condensation_edge;
            out.offending_witness = witness;
            out.offending_component_edge =
                std::pair<std::size_t, std::size_t>{witness.src_component, witness.dst_component};
            return out;
        }

        auto src_it = cond.component_of.find(witness.src_vertex);
        auto dst_it = cond.component_of.find(witness.dst_vertex);
        if (src_it == cond.component_of.end() || dst_it == cond.component_of.end()) {
            out.status = condensation_validation_status::component_mapping_missing;
            out.offending_witness = witness;
            out.offending_vertex = (src_it == cond.component_of.end()) ? witness.src_vertex : witness.dst_vertex;
            return out;
        }
        if (src_it->second != witness.src_component || dst_it->second != witness.dst_component) {
            out.status = condensation_validation_status::witness_component_mismatch;
            out.offending_witness = witness;
            return out;
        }
        if (!g.mem_edge(witness.src_vertex, witness.dst_vertex)) {
            out.status = condensation_validation_status::witness_original_edge_missing;
            out.offending_witness = witness;
            return out;
        }
    }

    const auto topo = topological_sort(cond.dag);
    if (topo.status != topological_sort_status::ok) {
        out.status = condensation_validation_status::condensation_not_dag;
        out.condensation_cycle_witness = std::move(topo.cycle_witness);
        return out;
    }

    out.status = condensation_validation_status::ok;
    return out;
}

/**
 * @brief Validates structural consistency of a condensation graph result.
 *
 * Convenience boolean wrapper around `validate_condensation_graph_detailed`.
 *
 * @tparam Graph Original graph type.
 * @param g Original graph used to derive the condensation.
 * @param cond Candidate condensation result.
 * @return True iff the result is internally coherent and consistent with @p g.
 */
template<class Graph>
[[nodiscard]] bool validate_condensation_graph(
    const Graph& g,
    const condensation_graph_result<typename Graph::vertex_type>& cond
) {
    return validate_condensation_graph_detailed(g, cond).ok();
}

/**
 * @brief Builds a DAG cover from SCC-condensation weak components.
 *
 * Each piece is an induced sub-DAG of the condensation graph; provenance back
 * to original vertices/edges is preserved via component ids and witnesses.
 */
template<class Graph>
[[nodiscard]] condensation_dag_cover_result<typename Graph::vertex_type>
condensation_dag_cover(
    const Graph& g,
    condensation_dag_cover_options options = {}
) {
    using V = typename Graph::vertex_type;
    condensation_dag_cover_result<V> out;
    out.condensation = condensation_graph(g);

    std::vector<std::vector<std::size_t>> component_groups;
    if (options.strategy == condensation_cover_strategy::weak_component_partition) {
        const auto weak = weakly_connected_components(out.condensation.dag);
        component_groups = weak.components;
    } else {
        auto topo = topological_sort(out.condensation.dag);
        if (!topo.has_order()) return out;
        std::size_t chunk = options.max_piece_components;
        if (chunk == 0) {
            if (options.objective == cover_objective::balance_piece_sizes) {
                chunk = std::max<std::size_t>(1, static_cast<std::size_t>(std::sqrt(static_cast<double>(std::max<std::size_t>(1, topo.order.size())))));
            } else if (options.objective == cover_objective::maximize_locality) {
                chunk = 2;
            } else {
                chunk = topo.order.size();
            }
        }
        for (std::size_t i = 0; i < topo.order.size(); i += chunk) {
            const auto end = std::min(topo.order.size(), i + chunk);
            component_groups.emplace_back(topo.order.begin() + static_cast<std::ptrdiff_t>(i),
                                          topo.order.begin() + static_cast<std::ptrdiff_t>(end));
        }
    }

    out.pieces.reserve(component_groups.size());
    for (std::size_t pid = 0; pid < component_groups.size(); ++pid) {
        condensation_dag_cover_piece<V> piece;
        piece.piece_id = pid;
        piece.component_ids = component_groups[pid];
        std::sort(piece.component_ids.begin(), piece.component_ids.end());

        std::unordered_set<std::size_t> comp_set(piece.component_ids.begin(), piece.component_ids.end());
        for (const auto cid : piece.component_ids) {
            piece.dag_piece.add_vertex(cid);
            out.component_to_piece[cid] = pid;
            if (cid < out.condensation.components.size()) {
                for (const auto& ov : out.condensation.components[cid]) {
                    piece.original_vertices.push_back(ov);
                }
            }
        }

        out.condensation.dag.iter_edges_e([&](const auto& e) {
            if (comp_set.contains(e.src) && comp_set.contains(e.dst)) {
                piece.dag_piece.add_edge(e.src, e.dst);
            }
        });

        for (const auto& witness : out.condensation.edge_witnesses) {
            if (comp_set.contains(witness.src_component) && comp_set.contains(witness.dst_component)) {
                piece.edge_witnesses.push_back(witness);
            }
        }
        out.metrics.total_witness_edges += piece.edge_witnesses.size();
        out.metrics.max_piece_vertices = std::max(out.metrics.max_piece_vertices, piece.original_vertices.size());
        out.pieces.push_back(std::move(piece));
    }

    out.metrics.piece_count = out.pieces.size();
    out.metrics.covered_vertices = out.condensation.component_count();
    out.metrics.overlap_vertices = 0;
    return out;
}

/**
 * @brief Validates coherence of a condensation DAG cover result.
 */
template<class Graph>
[[nodiscard]] condensation_dag_cover_validation_result<typename Graph::vertex_type>
validate_condensation_dag_cover(
    const Graph& g,
    const condensation_dag_cover_result<typename Graph::vertex_type>& cover
) {
    using V = typename Graph::vertex_type;
    condensation_dag_cover_validation_result<V> out;

    const auto cond_check = validate_condensation_graph_detailed(g, cover.condensation);
    if (!cond_check.ok()) {
        out.status = cover_validation_status::invalid_component_assignment;
        return out;
    }

    std::unordered_set<std::size_t> seen_components;
    for (const auto& piece : cover.pieces) {
        if (piece.component_ids.empty()) {
            out.status = cover_validation_status::empty_piece;
            out.offending_piece = piece.piece_id;
            return out;
        }
        for (const auto cid : piece.component_ids) {
            if (cid >= cover.condensation.component_count()) {
                out.status = cover_validation_status::invalid_component_assignment;
                out.offending_piece = piece.piece_id;
                out.offending_component = cid;
                return out;
            }
            if (!seen_components.insert(cid).second) {
                out.status = cover_validation_status::duplicate_vertex_assignment;
                out.offending_piece = piece.piece_id;
                out.offending_component = cid;
                return out;
            }
            auto it = cover.component_to_piece.find(cid);
            if (it == cover.component_to_piece.end() || it->second != piece.piece_id) {
                out.status = cover_validation_status::invalid_component_assignment;
                out.offending_piece = piece.piece_id;
                out.offending_component = cid;
                return out;
            }
        }
        piece.dag_piece.iter_edges_e([&](const auto& e) {
            if (out.status != cover_validation_status::ok) return;
            if (!cover.condensation.dag.mem_edge(e.src, e.dst)) {
                out.status = cover_validation_status::invalid_piece_edge;
                out.offending_piece = piece.piece_id;
                out.offending_component_edge = std::pair<std::size_t, std::size_t>{e.src, e.dst};
            }
        });
        if (out.status != cover_validation_status::ok) return out;

        const auto topo = topological_sort(piece.dag_piece);
        if (!topo.has_order()) {
            out.status = cover_validation_status::piece_not_acyclic;
            out.offending_piece = piece.piece_id;
            return out;
        }
    }

    for (std::size_t cid = 0; cid < cover.condensation.component_count(); ++cid) {
        if (!seen_components.contains(cid)) {
            out.status = cover_validation_status::uncovered_vertex;
            out.offending_component = cid;
            return out;
        }
    }

    return out;
}

/**
 * @brief Builds a weakly-connected BFS forest cover with parent provenance.
 */
template<class Graph>
[[nodiscard]] bfs_forest_cover_result<typename Graph::vertex_type, typename Graph::edge_label_type>
bfs_forest_cover(
    const Graph& g,
    bfs_forest_cover_options options = {}
) {
    using V = typename Graph::vertex_type;
    using E = typename Graph::edge_label_type;

    bfs_forest_cover_result<V, E> out;
    std::unordered_set<V> seen;
    std::vector<V> verts;
    verts.reserve(g.nb_vertex());
    g.iter_vertex([&](const auto& v) { verts.push_back(v); });
    if (options.strategy == bfs_forest_cover_strategy::degree_desc_root_priority ||
        options.objective == cover_objective::maximize_locality) {
        std::sort(verts.begin(), verts.end(), [&](const V& a, const V& b) {
            const auto da = g.succ(a).size() + g.pred(a).size();
            const auto db = g.succ(b).size() + g.pred(b).size();
            if (da != db) return da > db;
            return grafitt::to_string_fallback(a) < grafitt::to_string_fallback(b);
        });
    } else {
        std::sort(verts.begin(), verts.end(), [](const V& a, const V& b) {
            return grafitt::to_string_fallback(a) < grafitt::to_string_fallback(b);
        });
    }

    std::size_t pid = 0;
    for (const auto& root : verts) {
        if (seen.contains(root)) continue;

        bfs_forest_cover_piece<V, E> piece;
        piece.piece_id = pid;
        piece.root = root;
        piece.forest_piece.add_vertex(root);
        piece.parent_of[root] = std::nullopt;
        // Commit vertices to the piece at *discovery* time (not pop time) and
        // refuse to expand once the piece is full. The previous loop committed
        // on pop, so the neighbors discovered just before hitting
        // max_piece_vertices were left half-attached: present in
        // parent_of/tree_edges/forest_piece but absent from
        // vertices/vertex_to_piece. That orphaned them (never covered, never
        // re-rooted) and broke the tree-edge-count invariant
        // (tree_edges != vertices.size() - 1), failing validation.
        piece.vertices.push_back(root);
        out.vertex_to_piece[root] = pid;
        seen.insert(root);
        bool piece_full = (options.max_piece_vertices > 0 &&
                           piece.vertices.size() >= options.max_piece_vertices);

        std::queue<V> q;
        if (!piece_full) q.push(root);

        while (!q.empty() && !piece_full) {
            const V cur = q.front();
            q.pop();

            auto try_enqueue = [&](const V& next, const V& from) {
                if (piece_full) return;
                if (seen.contains(next)) return;
                if (options.max_piece_vertices > 0 &&
                    piece.vertices.size() >= options.max_piece_vertices) {
                    piece_full = true;
                    return;
                }
                seen.insert(next);
                piece.parent_of[next] = from;
                piece.forest_piece.add_vertex(next);
                piece.vertices.push_back(next);
                out.vertex_to_piece[next] = pid;

                std::optional<edge<V, E>> witness;
                const auto forward = g.find_all_edges(from, next);
                if (!forward.empty()) {
                    witness = edge<V, E>{forward.front().src, forward.front().dst, forward.front().label};
                } else {
                    const auto reverse = g.find_all_edges(next, from);
                    if (!reverse.empty()) witness = edge<V, E>{reverse.front().src, reverse.front().dst, reverse.front().label};
                }
                if (witness) {
                    piece.tree_edges.push_back(*witness);
                    piece.forest_piece.add_edge_e(*witness);
                }
                q.push(next);
            };

            for (const auto& n : g.succ(cur)) try_enqueue(n, cur);
            for (const auto& p : g.pred(cur)) try_enqueue(p, cur);
        }

        out.metrics.max_piece_vertices = std::max(out.metrics.max_piece_vertices, piece.vertices.size());
        out.metrics.total_witness_edges += piece.tree_edges.size();
        out.pieces.push_back(std::move(piece));
        ++pid;
    }

    out.metrics.piece_count = out.pieces.size();
    out.metrics.covered_vertices = out.vertex_to_piece.size();
    out.metrics.overlap_vertices = 0;
    return out;
}

/**
 * @brief Scores cover metrics under a chosen objective.
 */
[[nodiscard]] inline double score_cover_metrics(
    const cover_quality_metrics& metrics,
    cover_objective objective
) {
    if (metrics.piece_count == 0) return 0.0;
    const double piece_count = static_cast<double>(metrics.piece_count);
    const double max_piece = static_cast<double>(metrics.max_piece_vertices);
    const double covered = static_cast<double>(metrics.covered_vertices);
    const double witnesses = static_cast<double>(metrics.total_witness_edges);

    switch (objective) {
        case cover_objective::minimize_piece_count:
            return covered - piece_count * 10.0 - static_cast<double>(metrics.overlap_vertices) * 5.0;
        case cover_objective::balance_piece_sizes:
            return covered - max_piece - piece_count;
        case cover_objective::maximize_locality:
            return witnesses + covered - piece_count;
    }
    return covered - piece_count;
}

template<class Vertex>
[[nodiscard]] cover_scored<condensation_dag_cover_result<Vertex>>
score_cover(
    condensation_dag_cover_result<Vertex> cover,
    cover_objective objective
) {
    cover_scored<condensation_dag_cover_result<Vertex>> out;
    out.objective = objective;
    out.score = score_cover_metrics(cover.metrics, objective);
    out.cover = std::move(cover);
    return out;
}

template<class Vertex, class EdgeLabel>
[[nodiscard]] cover_scored<bfs_forest_cover_result<Vertex, EdgeLabel>>
score_cover(
    bfs_forest_cover_result<Vertex, EdgeLabel> cover,
    cover_objective objective
) {
    cover_scored<bfs_forest_cover_result<Vertex, EdgeLabel>> out;
    out.objective = objective;
    out.score = score_cover_metrics(cover.metrics, objective);
    out.cover = std::move(cover);
    return out;
}

/**
 * @brief Validates BFS-forest cover structural correctness.
 */
template<class Graph>
[[nodiscard]] bfs_forest_cover_validation_result<typename Graph::vertex_type>
validate_bfs_forest_cover(
    const Graph& g,
    const bfs_forest_cover_result<typename Graph::vertex_type, typename Graph::edge_label_type>& cover
) {
    using V = typename Graph::vertex_type;
    bfs_forest_cover_validation_result<V> out;

    std::unordered_set<V> covered;
    for (const auto& piece : cover.pieces) {
        if (piece.vertices.empty()) {
            out.status = cover_validation_status::empty_piece;
            out.offending_piece = piece.piece_id;
            return out;
        }
        const auto expected_edges = piece.vertices.size() - 1;
        if (piece.tree_edges.size() != expected_edges) {
            out.status = cover_validation_status::invalid_tree_edge_count;
            out.offending_piece = piece.piece_id;
            return out;
        }
        for (const auto& v : piece.vertices) {
            if (!covered.insert(v).second) {
                out.status = cover_validation_status::duplicate_vertex_assignment;
                out.offending_piece = piece.piece_id;
                out.offending_vertex = v;
                return out;
            }
            auto it = cover.vertex_to_piece.find(v);
            if (it == cover.vertex_to_piece.end() || it->second != piece.piece_id) {
                out.status = cover_validation_status::invalid_component_assignment;
                out.offending_piece = piece.piece_id;
                out.offending_vertex = v;
                return out;
            }
            auto pit = piece.parent_of.find(v);
            if (pit == piece.parent_of.end()) {
                out.status = cover_validation_status::invalid_parent_relation;
                out.offending_piece = piece.piece_id;
                out.offending_vertex = v;
                return out;
            }
            if (v == piece.root) {
                if (pit->second.has_value()) {
                    out.status = cover_validation_status::invalid_parent_relation;
                    out.offending_piece = piece.piece_id;
                    out.offending_vertex = v;
                    return out;
                }
            } else if (!pit->second.has_value()) {
                out.status = cover_validation_status::invalid_parent_relation;
                out.offending_piece = piece.piece_id;
                out.offending_vertex = v;
                return out;
            }
        }
        for (const auto& e : piece.tree_edges) {
            if (!g.mem_edge(e.src, e.dst)) {
                out.status = cover_validation_status::invalid_piece_edge;
                out.offending_piece = piece.piece_id;
                out.offending_parent_link = std::pair<V, V>{e.src, e.dst};
                return out;
            }
        }
    }

    g.iter_vertex([&](const auto& v) {
        if (out.status != cover_validation_status::ok) return;
        if (!covered.contains(v)) {
            out.status = cover_validation_status::uncovered_vertex;
            out.offending_vertex = v;
        }
    });
    return out;
}

/**
 * @brief Status of Dijkstra single-source shortest-path execution.
 */
enum class dijkstra_status {
    ok,
    source_not_found,
    negative_edge
};

/**
 * @brief Status of Bellman-Ford single-source shortest-path execution.
 */
enum class bellman_ford_status {
    ok,
    source_not_found,
    negative_cycle
};

/**
 * @brief Strategy selector for unified weighted shortest-path APIs.
 *
 * - `auto_select`: choose Dijkstra unless negative edges are present,
 *   otherwise Bellman-Ford.
 * - `dijkstra`: force Dijkstra (fails on negative edges).
 * - `bellman_ford`: force Bellman-Ford.
 */
enum class weighted_shortest_path_strategy {
    auto_select,
    dijkstra,
    bellman_ford
};


/**
 * @brief Witness edge for Dijkstra input validation failures.
 *
 * @tparam Vertex Vertex type.
 * @tparam Distance Weight/distance scalar type.
 */
template<class Vertex, class Distance>
struct weighted_edge_witness {
    Vertex src {};
    Vertex dst {};
    Distance weight {};
};

/**
 * @brief Result object for weighted single-source shortest paths.
 *
 * Stores distances and predecessor links from one source and can reconstruct
 * concrete shortest paths to reachable targets.
 *
 * @tparam Vertex Vertex type.
 * @tparam Distance Weight/distance scalar type.
 */
template<class Vertex, class Distance>
struct dijkstra_result {
    dijkstra_status status { dijkstra_status::ok };
    std::optional<Vertex> source;
    std::unordered_map<Vertex, Distance> distance;
    std::unordered_map<Vertex, Vertex> predecessor;
    std::optional<weighted_edge_witness<Vertex, Distance>> negative_edge_witness;

    /**
     * @brief Returns true iff shortest paths were computed successfully.
     */
    [[nodiscard]] bool ok() const noexcept {
        return status == dijkstra_status::ok;
    }

    /**
     * @brief Returns true iff a path from source to @p target exists.
     */
    [[nodiscard]] bool reachable(const Vertex& target) const {
        return distance.contains(target);
    }

    /**
     * @brief Returns the shortest distance to @p target if reachable.
     */
    [[nodiscard]] std::optional<Distance> distance_to(const Vertex& target) const {
        if (auto it = distance.find(target); it != distance.end()) return it->second;
        return std::nullopt;
    }

    /**
     * @brief Reconstructs one shortest path from source to @p target.
     *
     * @return Vertex sequence from source to target, or `std::nullopt` when
     *         no source exists, no route exists, or predecessor links are
     *         inconsistent.
     */
    [[nodiscard]] std::optional<std::vector<Vertex>> path_to(const Vertex& target) const {
        if (!source) return std::nullopt;
        if (!distance.contains(target)) return std::nullopt;

        std::vector<Vertex> path;
        Vertex cur = target;
        path.push_back(cur);
        while (!(cur == *source)) {
            auto it = predecessor.find(cur);
            if (it == predecessor.end()) return std::nullopt;
            cur = it->second;
            path.push_back(cur);
        }
        std::reverse(path.begin(), path.end());
        return path;
    }
};

/**
 * @brief Result object for Bellman-Ford single-source shortest paths.
 *
 * Supports negative edge weights and reports one reachable negative cycle
 * witness when detected.
 *
 * @tparam Vertex Vertex type.
 * @tparam Distance Weight/distance scalar type.
 */
template<class Vertex, class Distance>
struct bellman_ford_result {
    bellman_ford_status status { bellman_ford_status::ok };
    std::optional<Vertex> source;
    std::unordered_map<Vertex, Distance> distance;
    std::unordered_map<Vertex, Vertex> predecessor;
    std::vector<Vertex> negative_cycle_witness;

    /**
     * @brief Returns true iff shortest paths were computed successfully.
     */
    [[nodiscard]] bool ok() const noexcept {
        return status == bellman_ford_status::ok;
    }

    /**
     * @brief Returns true iff a path from source to @p target exists.
     */
    [[nodiscard]] bool reachable(const Vertex& target) const {
        return distance.contains(target);
    }

    /**
     * @brief Returns the shortest distance to @p target if reachable.
     */
    [[nodiscard]] std::optional<Distance> distance_to(const Vertex& target) const {
        if (auto it = distance.find(target); it != distance.end()) return it->second;
        return std::nullopt;
    }

    /**
     * @brief Reconstructs one shortest path from source to @p target.
     *
     * @return Vertex sequence from source to target, or `std::nullopt` when
     *         no source exists, no route exists, or predecessor links are
     *         inconsistent.
     */
    [[nodiscard]] std::optional<std::vector<Vertex>> path_to(const Vertex& target) const {
        if (!source) return std::nullopt;
        if (!distance.contains(target)) return std::nullopt;

        std::vector<Vertex> path;
        Vertex cur = target;
        path.push_back(cur);
        while (!(cur == *source)) {
            auto it = predecessor.find(cur);
            if (it == predecessor.end()) return std::nullopt;
            cur = it->second;
            path.push_back(cur);
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    /**
     * @brief Returns true iff a negative cycle witness was extracted.
     */
    [[nodiscard]] bool has_negative_cycle() const noexcept {
        return status == bellman_ford_status::negative_cycle && !negative_cycle_witness.empty();
    }
};

/**
 * @brief Unified weighted shortest-path result wrapper.
 *
 * Provides a stable caller-facing shape while preserving algorithm-specific
 * diagnostics from either Dijkstra or Bellman-Ford.
 *
 * @tparam Vertex Vertex type.
 * @tparam Distance Weight/distance scalar type.
 */
template<class Vertex, class Distance>
struct weighted_shortest_path_result {
    using dijkstra_type = dijkstra_result<Vertex, Distance>;
    using bellman_ford_type = bellman_ford_result<Vertex, Distance>;
    using algorithm_result_type = std::variant<dijkstra_type, bellman_ford_type>;

    weighted_shortest_path_strategy strategy_used { weighted_shortest_path_strategy::auto_select };
    algorithm_result_type algorithm_result;

    /**
     * @brief Returns true iff the selected algorithm completed successfully.
     */
    [[nodiscard]] bool ok() const {
        return std::visit([](const auto& r) { return r.ok(); }, algorithm_result);
    }

    /**
     * @brief Returns true iff a route from source to @p target exists.
     */
    [[nodiscard]] bool reachable(const Vertex& target) const {
        return std::visit([&](const auto& r) { return r.reachable(target); }, algorithm_result);
    }

    /**
     * @brief Returns shortest distance to @p target when reachable.
     */
    [[nodiscard]] std::optional<Distance> distance_to(const Vertex& target) const {
        return std::visit([&](const auto& r) { return r.distance_to(target); }, algorithm_result);
    }

    /**
     * @brief Reconstructs one shortest path to @p target if available.
     */
    [[nodiscard]] std::optional<std::vector<Vertex>> path_to(const Vertex& target) const {
        return std::visit([&](const auto& r) { return r.path_to(target); }, algorithm_result);
    }

    /**
     * @brief Returns Dijkstra view when strategy used Dijkstra.
     */
    [[nodiscard]] const dijkstra_type* as_dijkstra() const noexcept {
        return std::get_if<dijkstra_type>(&algorithm_result);
    }

    /**
     * @brief Returns Bellman-Ford view when strategy used Bellman-Ford.
     */
    [[nodiscard]] const bellman_ford_type* as_bellman_ford() const noexcept {
        return std::get_if<bellman_ford_type>(&algorithm_result);
    }
};

/**
 * @brief Computes weighted single-source shortest paths using Dijkstra.
 *
 * The provided weight function must produce non-negative values. If a negative
 * edge is observed, execution stops and the result status is `negative_edge`
 * with a witness edge.
 *
 * @tparam Graph Graph type exposing the Grafitt graph interface.
 * @tparam WeightOf Callable mapping `Graph::edge_type` to a numeric-like
 * scalar supporting `<` and `+`.
 * @tparam Distance Distance scalar type (deduced from `WeightOf` by default).
 *
 * @param g Input graph (directed or undirected).
 * @param source Source vertex.
 * @param weight_of Edge weight function.
 * @param zero Additive zero value for the distance domain.
 *
 * @return `dijkstra_result` containing distances and predecessor links.
 *
 * @pre `source` should exist in the graph; otherwise `source_not_found`.
 * @pre Every traversed edge must have non-negative weight for correctness.
 * @post On success, `distance[v]` is the shortest known distance from source.
 * @complexity O((|V| + |E|) log |V|) expected.
 */
template<class Graph, class WeightOf,
    class Distance = std::decay_t<std::invoke_result_t<WeightOf, const typename Graph::edge_type&>>>
[[nodiscard]] dijkstra_result<typename Graph::vertex_type, Distance>
dijkstra_shortest_paths(
    const Graph& g,
    const typename Graph::vertex_type& source,
    WeightOf&& weight_of,
    Distance zero = Distance{}
) {
    using V = typename Graph::vertex_type;

    dijkstra_result<V, Distance> out;
    out.source = source;

    if (!g.mem_vertex(source)) {
        out.status = dijkstra_status::source_not_found;
        out.source.reset();
        return out;
    }

    struct queue_node {
        Distance dist;
        V vertex;
    };
    struct queue_cmp {
        bool operator()(const queue_node& a, const queue_node& b) const {
            return b.dist < a.dist;
        }
    };

    std::priority_queue<queue_node, std::vector<queue_node>, queue_cmp> pq;
    out.distance[source] = zero;
    pq.push(queue_node{zero, source});

    while (!pq.empty()) {
        const auto cur = pq.top();
        pq.pop();

        auto best_it = out.distance.find(cur.vertex);
        if (best_it == out.distance.end()) continue;
        if (best_it->second < cur.dist) continue;

        for (const auto& e : g.succ_e(cur.vertex)) {
            const Distance w = static_cast<Distance>(std::invoke(weight_of, e));
            if (w < zero) {
                out.status = dijkstra_status::negative_edge;
                out.negative_edge_witness = weighted_edge_witness<V, Distance>{e.src, e.dst, w};
                return out;
            }

            const Distance nd = static_cast<Distance>(cur.dist + w);
            auto it = out.distance.find(e.dst);
            if (it == out.distance.end() || nd < it->second) {
                out.distance[e.dst] = nd;
                out.predecessor[e.dst] = cur.vertex;
                pq.push(queue_node{nd, e.dst});
            }
        }
    }

    out.status = dijkstra_status::ok;
    return out;
}

/**
 * @brief Computes one weighted shortest path between source and destination.
 *
 * @return `std::nullopt` if source/destination is missing, no route exists,
 *         or Dijkstra rejects a negative edge.
 */
template<class Graph, class WeightOf,
    class Distance = std::decay_t<std::invoke_result_t<WeightOf, const typename Graph::edge_type&>>>
[[nodiscard]] std::optional<std::vector<typename Graph::vertex_type>>
dijkstra_shortest_path(
    const Graph& g,
    const typename Graph::vertex_type& source,
    const typename Graph::vertex_type& destination,
    WeightOf&& weight_of,
    Distance zero = Distance{}
) {
    if (!g.mem_vertex(destination)) return std::nullopt;
    auto result = dijkstra_shortest_paths(g, source, std::forward<WeightOf>(weight_of), zero);
    if (result.status != dijkstra_status::ok) return std::nullopt;
    return result.path_to(destination);
}

/**
 * @brief Computes weighted single-source shortest paths using Bellman-Ford.
 *
 * Supports negative edges. If a reachable negative cycle exists, returns
 * `status == negative_cycle` and provides a concrete cycle witness whose
 * first and last vertices are equal.
 *
 * @tparam Graph Graph type exposing the Grafitt graph interface.
 * @tparam WeightOf Callable mapping `Graph::edge_type` to a numeric-like
 * scalar supporting `<` and `+`.
 * @tparam Distance Distance scalar type (deduced from `WeightOf` by default).
 *
 * @param g Input graph (directed or undirected).
 * @param source Source vertex.
 * @param weight_of Edge weight function.
 * @param zero Additive zero value for the distance domain.
 *
 * @return `bellman_ford_result` containing shortest-path metadata or a
 *         negative-cycle witness.
 *
 * @complexity O(|V| * |E|) expected.
 */
template<class Graph, class WeightOf,
    class Distance = std::decay_t<std::invoke_result_t<WeightOf, const typename Graph::edge_type&>>>
[[nodiscard]] bellman_ford_result<typename Graph::vertex_type, Distance>
bellman_ford_shortest_paths(
    const Graph& g,
    const typename Graph::vertex_type& source,
    WeightOf&& weight_of,
    Distance zero = Distance{}
) {
    using V = typename Graph::vertex_type;
    using E = typename Graph::edge_type;

    bellman_ford_result<V, Distance> out;
    out.source = source;

    if (!g.mem_vertex(source)) {
        out.status = bellman_ford_status::source_not_found;
        out.source.reset();
        return out;
    }

    std::vector<V> verts;
    verts.reserve(g.nb_vertex());
    g.iter_vertex([&](const auto& v) { verts.push_back(v); });
    std::vector<E> es;
    es.reserve(g.nb_edges());
    g.iter_edges_e([&](const auto& e) { es.push_back(e); });

    out.distance[source] = zero;

    for (std::size_t i = 1; i < verts.size(); ++i) {
        bool changed = false;
        for (const auto& e : es) {
            auto src_it = out.distance.find(e.src);
            if (src_it == out.distance.end()) continue;

            const Distance w = static_cast<Distance>(std::invoke(weight_of, e));
            const Distance cand = static_cast<Distance>(src_it->second + w);
            auto dst_it = out.distance.find(e.dst);
            if (dst_it == out.distance.end() || cand < dst_it->second) {
                out.distance[e.dst] = cand;
                out.predecessor[e.dst] = e.src;
                changed = true;
            }
        }
        if (!changed) break;
    }

    std::optional<V> relaxed_vertex;
    for (const auto& e : es) {
        auto src_it = out.distance.find(e.src);
        if (src_it == out.distance.end()) continue;

        const Distance w = static_cast<Distance>(std::invoke(weight_of, e));
        const Distance cand = static_cast<Distance>(src_it->second + w);
        auto dst_it = out.distance.find(e.dst);
        if (dst_it == out.distance.end() || cand < dst_it->second) {
            out.predecessor[e.dst] = e.src;
            relaxed_vertex = e.dst;
            break;
        }
    }

    if (!relaxed_vertex) {
        out.status = bellman_ford_status::ok;
        return out;
    }

    V inside = *relaxed_vertex;
    for (std::size_t i = 0; i < verts.size(); ++i) {
        auto it = out.predecessor.find(inside);
        if (it == out.predecessor.end()) break;
        inside = it->second;
    }

    std::vector<V> cycle;
    cycle.push_back(inside);
    V cur = inside;
    std::size_t guard = 0;
    do {
        auto it = out.predecessor.find(cur);
        if (it == out.predecessor.end()) {
            cycle.clear();
            break;
        }
        cur = it->second;
        cycle.push_back(cur);
        ++guard;
        if (guard > verts.size() + 1) {
            cycle.clear();
            break;
        }
    } while (!(cur == inside));

    if (!cycle.empty()) std::reverse(cycle.begin(), cycle.end());
    out.negative_cycle_witness = std::move(cycle);
    out.status = bellman_ford_status::negative_cycle;
    return out;
}

/**
 * @brief Computes one Bellman-Ford shortest path from source to destination.
 *
 * @return `std::nullopt` if source/destination is missing, no route exists, or
 *         a reachable negative cycle is detected.
 */
template<class Graph, class WeightOf,
    class Distance = std::decay_t<std::invoke_result_t<WeightOf, const typename Graph::edge_type&>>>
[[nodiscard]] std::optional<std::vector<typename Graph::vertex_type>>
bellman_ford_shortest_path(
    const Graph& g,
    const typename Graph::vertex_type& source,
    const typename Graph::vertex_type& destination,
    WeightOf&& weight_of,
    Distance zero = Distance{}
) {
    if (!g.mem_vertex(destination)) return std::nullopt;
    auto result = bellman_ford_shortest_paths(g, source, std::forward<WeightOf>(weight_of), zero);
    if (result.status != bellman_ford_status::ok) return std::nullopt;
    return result.path_to(destination);
}

/**
 * @brief Unified weighted shortest-path API with selectable strategy.
 *
 * When `strategy == auto_select`, this scans edges once:
 * - if any edge has negative weight, runs Bellman-Ford
 * - otherwise runs Dijkstra
 *
 * @tparam Graph Graph type exposing the Grafitt graph interface.
 * @tparam WeightOf Callable mapping `Graph::edge_type` to a numeric-like
 * scalar supporting `<` and `+`.
 * @tparam Distance Distance scalar type (deduced from `WeightOf` by default).
 */
template<class Graph, class WeightOf,
    class Distance = std::decay_t<std::invoke_result_t<WeightOf, const typename Graph::edge_type&>>>
[[nodiscard]] weighted_shortest_path_result<typename Graph::vertex_type, Distance>
weighted_shortest_paths(
    const Graph& g,
    const typename Graph::vertex_type& source,
    WeightOf&& weight_of,
    weighted_shortest_path_strategy strategy = weighted_shortest_path_strategy::auto_select,
    Distance zero = Distance{}
) {
    using V = typename Graph::vertex_type;
    using Result = weighted_shortest_path_result<V, Distance>;
    using W = std::remove_reference_t<WeightOf>;

    auto w = std::forward<WeightOf>(weight_of);

    auto run_dijkstra = [&]() -> Result {
        auto r = dijkstra_shortest_paths(g, source, w, zero);
        Result out;
        out.strategy_used = weighted_shortest_path_strategy::dijkstra;
        out.algorithm_result = std::move(r);
        return out;
    };

    auto run_bellman = [&]() -> Result {
        auto r = bellman_ford_shortest_paths(g, source, w, zero);
        Result out;
        out.strategy_used = weighted_shortest_path_strategy::bellman_ford;
        out.algorithm_result = std::move(r);
        return out;
    };

    if (strategy == weighted_shortest_path_strategy::dijkstra) return run_dijkstra();
    if (strategy == weighted_shortest_path_strategy::bellman_ford) return run_bellman();

    bool has_negative = false;
    g.iter_edges_e([&](const auto& e) {
        if (has_negative) return;
        if (static_cast<Distance>(std::invoke(w, e)) < zero) has_negative = true;
    });
    if (has_negative) return run_bellman();
    return run_dijkstra();
}

/**
 * @brief Unified weighted source-to-destination shortest path helper.
 *
 * Returns `std::nullopt` if route is unavailable or the selected strategy
 * reports failure (for example, negative cycle in Bellman-Ford).
 */
template<class Graph, class WeightOf,
    class Distance = std::decay_t<std::invoke_result_t<WeightOf, const typename Graph::edge_type&>>>
[[nodiscard]] std::optional<std::vector<typename Graph::vertex_type>>
weighted_shortest_path(
    const Graph& g,
    const typename Graph::vertex_type& source,
    const typename Graph::vertex_type& destination,
    WeightOf&& weight_of,
    weighted_shortest_path_strategy strategy = weighted_shortest_path_strategy::auto_select,
    Distance zero = Distance{}
) {
    if (!g.mem_vertex(destination)) return std::nullopt;
    auto result = weighted_shortest_paths(g, source, std::forward<WeightOf>(weight_of), strategy, zero);
    if (!result.ok()) return std::nullopt;
    return result.path_to(destination);
}

/**
 * @brief Status of incremental Dijkstra cache state.
 */
enum class incremental_dijkstra_cache_status {
    ok,
    source_not_found,
    negative_edge,
    stale_requires_rebuild
};

/**
 * @brief Status returned by incremental Dijkstra update operations.
 */
enum class incremental_dijkstra_update_status {
    ok,
    source_unset,
    touched_vertex_not_found,
    touched_vertex_unreachable,
    edge_not_found,
    negative_edge,
    cache_stale_requires_rebuild
};

/**
 * @brief Reusable single-source shortest-path cache for incremental updates.
 *
 * Supports efficient decrease-only refresh workflows (edge insertion or
 * weight decrease) under Dijkstra assumptions (non-negative weights).
 *
 * @tparam Vertex Vertex type.
 * @tparam Distance Distance scalar type.
 */
template<class Vertex, class Distance>
struct incremental_dijkstra_cache {
    incremental_dijkstra_cache_status status { incremental_dijkstra_cache_status::ok };
    std::optional<Vertex> source;
    Distance zero {};
    std::unordered_map<Vertex, Distance> distance;
    std::unordered_map<Vertex, Vertex> predecessor;
    std::optional<weighted_edge_witness<Vertex, Distance>> negative_edge_witness;

    /**
     * @brief Returns true iff cache is valid for incremental decrease updates.
     */
    [[nodiscard]] bool ok() const noexcept {
        return status == incremental_dijkstra_cache_status::ok;
    }

    /**
     * @brief Returns true iff cache is marked stale and requires rebuild.
     */
    [[nodiscard]] bool stale() const noexcept {
        return status == incremental_dijkstra_cache_status::stale_requires_rebuild;
    }

    /**
     * @brief Returns true iff @p v is currently reachable from source.
     */
    [[nodiscard]] bool reachable(const Vertex& v) const {
        return distance.contains(v);
    }

    /**
     * @brief Returns shortest distance to @p v if currently known.
     */
    [[nodiscard]] std::optional<Distance> distance_to(const Vertex& v) const {
        if (auto it = distance.find(v); it != distance.end()) return it->second;
        return std::nullopt;
    }

    /**
     * @brief Reconstructs one shortest path to @p target if available.
     */
    [[nodiscard]] std::optional<std::vector<Vertex>> path_to(const Vertex& target) const {
        if (!source) return std::nullopt;
        if (!distance.contains(target)) return std::nullopt;

        std::vector<Vertex> path;
        Vertex cur = target;
        path.push_back(cur);
        while (!(cur == *source)) {
            auto it = predecessor.find(cur);
            if (it == predecessor.end()) return std::nullopt;
            cur = it->second;
            path.push_back(cur);
        }
        std::reverse(path.begin(), path.end());
        return path;
    }
};

/**
 * @brief Rich result for one incremental Dijkstra update step.
 *
 * @tparam Vertex Vertex type.
 * @tparam Distance Distance scalar type.
 */
template<class Vertex, class Distance>
struct incremental_dijkstra_update_result {
    incremental_dijkstra_update_status status { incremental_dijkstra_update_status::ok };
    std::size_t improved_vertices { 0 };
    std::size_t relaxed_edges { 0 };
    std::optional<weighted_edge_witness<Vertex, Distance>> negative_edge_witness;

    /**
     * @brief Returns true iff the update completed successfully.
     */
    [[nodiscard]] bool ok() const noexcept {
        return status == incremental_dijkstra_update_status::ok;
    }
};

namespace detail {

template<class Graph, class WeightOf, class Distance>
void incremental_dijkstra_propagate(
    const Graph& g,
    WeightOf&& weight_of,
    incremental_dijkstra_cache<typename Graph::vertex_type, Distance>& cache,
    std::priority_queue<
        std::pair<Distance, typename Graph::vertex_type>,
        std::vector<std::pair<Distance, typename Graph::vertex_type>>,
        std::greater<>
    >& pq,
    incremental_dijkstra_update_result<typename Graph::vertex_type, Distance>& update
) {
    using V = typename Graph::vertex_type;
    while (!pq.empty()) {
        const auto [cur_dist, cur] = pq.top();
        pq.pop();

        auto best_it = cache.distance.find(cur);
        if (best_it == cache.distance.end()) continue;
        if (best_it->second < cur_dist) continue;

        for (const auto& e : g.succ_e(cur)) {
            ++update.relaxed_edges;
            const Distance w = static_cast<Distance>(std::invoke(weight_of, e));
            if (w < cache.zero) {
                update.status = incremental_dijkstra_update_status::negative_edge;
                update.negative_edge_witness = weighted_edge_witness<V, Distance>{e.src, e.dst, w};
                cache.status = incremental_dijkstra_cache_status::negative_edge;
                cache.negative_edge_witness = update.negative_edge_witness;
                return;
            }

            const Distance candidate = static_cast<Distance>(cur_dist + w);
            auto dst_it = cache.distance.find(e.dst);
            if (dst_it == cache.distance.end() || candidate < dst_it->second) {
                const bool existed = (dst_it != cache.distance.end());
                const Distance old_value = existed ? dst_it->second : cache.zero;
                cache.distance[e.dst] = candidate;
                cache.predecessor[e.dst] = cur;
                pq.push({candidate, e.dst});
                if (!existed || candidate < old_value) ++update.improved_vertices;
            }
        }
    }
}

} // namespace detail

/**
 * @brief Builds incremental Dijkstra cache from scratch.
 *
 * @tparam Graph Graph type exposing Grafitt graph API.
 * @tparam WeightOf Callable mapping edge to weight.
 * @tparam Distance Distance scalar type.
 */
template<class Graph, class WeightOf,
    class Distance = std::decay_t<std::invoke_result_t<WeightOf, const typename Graph::edge_type&>>>
[[nodiscard]] incremental_dijkstra_cache<typename Graph::vertex_type, Distance>
incremental_dijkstra_build(
    const Graph& g,
    const typename Graph::vertex_type& source,
    WeightOf&& weight_of,
    Distance zero = Distance{}
) {
    using V = typename Graph::vertex_type;
    incremental_dijkstra_cache<V, Distance> cache;
    cache.source = source;
    cache.zero = zero;

    auto base = dijkstra_shortest_paths(g, source, std::forward<WeightOf>(weight_of), zero);
    cache.distance = std::move(base.distance);
    cache.predecessor = std::move(base.predecessor);
    cache.negative_edge_witness = base.negative_edge_witness;

    if (base.status == dijkstra_status::ok) {
        cache.status = incremental_dijkstra_cache_status::ok;
    } else if (base.status == dijkstra_status::source_not_found) {
        cache.status = incremental_dijkstra_cache_status::source_not_found;
        cache.source.reset();
    } else {
        cache.status = incremental_dijkstra_cache_status::negative_edge;
    }
    return cache;
}

/**
 * @brief Rebuilds incremental cache from graph state and existing source.
 *
 * @tparam Graph Graph type exposing Grafitt graph API.
 * @tparam WeightOf Callable mapping edge to weight.
 * @tparam Distance Distance scalar type.
 * @param g Input graph.
 * @param weight_of Edge weight function.
 * @param cache Cache object to refresh.
 * @return Cache status after rebuild.
 */
template<class Graph, class WeightOf, class Distance>
[[nodiscard]] incremental_dijkstra_cache_status incremental_dijkstra_rebuild(
    const Graph& g,
    WeightOf&& weight_of,
    incremental_dijkstra_cache<typename Graph::vertex_type, Distance>& cache
) {
    if (!cache.source) {
        cache.status = incremental_dijkstra_cache_status::source_not_found;
        return cache.status;
    }
    auto rebuilt = incremental_dijkstra_build(g, *cache.source, std::forward<WeightOf>(weight_of), cache.zero);
    cache = std::move(rebuilt);
    return cache.status;
}

/**
 * @brief Marks cache stale after unsupported updates (deletion/increase).
 *
 * This explicitly records that decrease-only incremental assumptions are
 * invalidated and a full rebuild is required.
 */
template<class Vertex, class Distance>
void incremental_dijkstra_mark_stale(
    incremental_dijkstra_cache<Vertex, Distance>& cache
) {
    if (cache.status == incremental_dijkstra_cache_status::ok) {
        cache.status = incremental_dijkstra_cache_status::stale_requires_rebuild;
    }
}

/**
 * @brief Performs decrease-friendly incremental update from one touched edge.
 *
 * Intended for edge insertion or edge-weight decrease of `src -> dst` under
 * non-negative weights. If this relaxation improves `dst`, the improvement is
 * propagated with a Dijkstra frontier.
 *
 * @tparam Graph Graph type exposing Grafitt graph API.
 * @tparam WeightOf Callable mapping edge to weight.
 * @tparam Distance Distance scalar type.
 *
 * @pre Cache must be built for same source and graph vertex set.
 * @pre Graph updates since last cache state should be monotone decreases only.
 * @post On success, cache distances are no larger than before.
 */
template<class Graph, class WeightOf, class Distance>
[[nodiscard]] incremental_dijkstra_update_result<typename Graph::vertex_type, Distance>
incremental_dijkstra_relax_edge(
    const Graph& g,
    const typename Graph::vertex_type& src,
    const typename Graph::vertex_type& dst,
    WeightOf&& weight_of,
    incremental_dijkstra_cache<typename Graph::vertex_type, Distance>& cache
) {
    using V = typename Graph::vertex_type;
    using edge_pair = std::pair<V, V>;
    incremental_dijkstra_update_result<V, Distance> out;

    if (!cache.source) {
        out.status = incremental_dijkstra_update_status::source_unset;
        return out;
    }
    if (cache.status == incremental_dijkstra_cache_status::stale_requires_rebuild) {
        out.status = incremental_dijkstra_update_status::cache_stale_requires_rebuild;
        return out;
    }
    if (!g.mem_vertex(src) || !g.mem_vertex(dst)) {
        out.status = incremental_dijkstra_update_status::touched_vertex_not_found;
        return out;
    }
    if (!cache.distance.contains(src)) {
        out.status = incremental_dijkstra_update_status::touched_vertex_unreachable;
        return out;
    }

    auto relax_pair = [&](const edge_pair& p,
                          std::priority_queue<
                              std::pair<Distance, V>,
                              std::vector<std::pair<Distance, V>>,
                              std::greater<>
                          >& pq) -> bool {
        const auto edges_sd = g.find_all_edges(p.first, p.second);
        if (edges_sd.empty()) return true;

        std::optional<Distance> best_w;
        for (const auto& e : edges_sd) {
            const Distance w = static_cast<Distance>(std::invoke(weight_of, e));
            if (w < cache.zero) {
                out.status = incremental_dijkstra_update_status::negative_edge;
                out.negative_edge_witness = weighted_edge_witness<V, Distance>{e.src, e.dst, w};
                cache.status = incremental_dijkstra_cache_status::negative_edge;
                cache.negative_edge_witness = out.negative_edge_witness;
                return false;
            }
            if (!best_w || w < *best_w) best_w = w;
        }
        if (!best_w) return true;

        const auto src_it = cache.distance.find(p.first);
        if (src_it == cache.distance.end()) return true;

        const Distance candidate = static_cast<Distance>(src_it->second + *best_w);
        auto dst_it = cache.distance.find(p.second);
        if (dst_it == cache.distance.end() || candidate < dst_it->second) {
            cache.distance[p.second] = candidate;
            cache.predecessor[p.second] = p.first;
            pq.push({candidate, p.second});
            ++out.improved_vertices;
        }
        return true;
    };

    std::priority_queue<
        std::pair<Distance, V>,
        std::vector<std::pair<Distance, V>>,
        std::greater<>
    > pq;

    if (!relax_pair({src, dst}, pq)) return out;
    if (!g.is_directed() && !(src == dst)) {
        if (!relax_pair({dst, src}, pq)) return out;
    }

    if (pq.empty()) {
        out.status = g.mem_edge(src, dst) ? incremental_dijkstra_update_status::ok
                                          : incremental_dijkstra_update_status::edge_not_found;
        return out;
    }

    detail::incremental_dijkstra_propagate(g, std::forward<WeightOf>(weight_of), cache, pq, out);
    return out;
}

/**
 * @brief Performs decrease-friendly incremental update from one touched vertex.
 *
 * Intended for workflows where outgoing edges from `touched` were inserted or
 * had decreased weights, while global assumptions remain Dijkstra-valid.
 */
template<class Graph, class WeightOf, class Distance>
[[nodiscard]] incremental_dijkstra_update_result<typename Graph::vertex_type, Distance>
incremental_dijkstra_relax_from_vertex(
    const Graph& g,
    const typename Graph::vertex_type& touched,
    WeightOf&& weight_of,
    incremental_dijkstra_cache<typename Graph::vertex_type, Distance>& cache
) {
    using V = typename Graph::vertex_type;
    incremental_dijkstra_update_result<V, Distance> out;

    if (!cache.source) {
        out.status = incremental_dijkstra_update_status::source_unset;
        return out;
    }
    if (cache.status == incremental_dijkstra_cache_status::stale_requires_rebuild) {
        out.status = incremental_dijkstra_update_status::cache_stale_requires_rebuild;
        return out;
    }
    if (!g.mem_vertex(touched)) {
        out.status = incremental_dijkstra_update_status::touched_vertex_not_found;
        return out;
    }
    auto start_it = cache.distance.find(touched);
    if (start_it == cache.distance.end()) {
        out.status = incremental_dijkstra_update_status::touched_vertex_unreachable;
        return out;
    }

    std::priority_queue<
        std::pair<Distance, V>,
        std::vector<std::pair<Distance, V>>,
        std::greater<>
    > pq;
    pq.push({start_it->second, touched});
    detail::incremental_dijkstra_propagate(g, std::forward<WeightOf>(weight_of), cache, pq, out);
    return out;
}

/**
 * @brief Status of graph isomorphism checks.
 */
enum class isomorphism_status {
    isomorphic,
    not_isomorphic,
    size_mismatch,
    directed_mismatch,
    degree_profile_mismatch,
    edge_label_profile_mismatch
};

/**
 * @brief Status of (sub)graph matching routines.
 */
enum class subgraph_match_status {
    match_found,
    no_match,
    directed_mismatch,
    pattern_larger_than_target,
    search_budget_exhausted
};

/**
 * @brief Search mode for subgraph matching.
 *
 * `exact` performs exhaustive backtracking subject only to logical pruning.
 * `heuristic_constrained` allows early termination via explicit search limits.
 */
enum class subgraph_match_search_mode {
    exact,
    heuristic_constrained
};

/**
 * @brief One matched pattern edge paired with one supporting target edge.
 *
 * @tparam PatternVertex Pattern-graph vertex type.
 * @tparam TargetVertex Target-graph vertex type.
 * @tparam EdgeLabel Edge-label type.
 */
template<class PatternVertex, class TargetVertex, class EdgeLabel>
struct subgraph_edge_witness {
    edge<PatternVertex, EdgeLabel> pattern_edge;
    edge<TargetVertex, EdgeLabel> target_edge;
};

/**
 * @brief Binding/witness payload for one successful subgraph match.
 *
 * @tparam PatternVertex Pattern-graph vertex type.
 * @tparam TargetVertex Target-graph vertex type.
 * @tparam EdgeLabel Edge-label type.
 */
template<class PatternVertex, class TargetVertex, class EdgeLabel>
struct subgraph_match_witness {
    std::unordered_map<PatternVertex, TargetVertex> vertex_bindings;
    std::unordered_map<TargetVertex, PatternVertex> reverse_vertex_bindings;
    std::vector<subgraph_edge_witness<PatternVertex, TargetVertex, EdgeLabel>> edge_witnesses;

    /**
     * @brief Converts vertex bindings to rewrite-friendly string metadata.
     *
     * @tparam PatternNameOf Callable producing a string-like key from pattern vertex.
     * @tparam TargetNameOf Callable producing a string-like value from target vertex.
     * @param pattern_name_of Naming callback for pattern-side vertices.
     * @param target_name_of Naming callback for target-side vertices.
     * @return String map suitable for rewrite/query metadata plumbing.
     */
    template<class PatternNameOf, class TargetNameOf>
    [[nodiscard]] std::map<std::string, std::string> to_named_bindings(
        PatternNameOf&& pattern_name_of,
        TargetNameOf&& target_name_of
    ) const {
        std::map<std::string, std::string> out;
        for (const auto& [pv, tv] : vertex_bindings) {
            out.emplace(
                grafitt::to_string_fallback(std::invoke(pattern_name_of, pv)),
                grafitt::to_string_fallback(std::invoke(target_name_of, tv))
            );
        }
        return out;
    }
};

/**
 * @brief Matching options for exact and constrained-heuristic subgraph search.
 *
 * @tparam PatternVertex Pattern-graph vertex type.
 * @tparam TargetVertex Target-graph vertex type.
 * @tparam EdgeLabel Edge-label type.
 *
 * @note Directedness must still match between pattern and target.
 */
template<class PatternVertex, class TargetVertex, class EdgeLabel>
struct subgraph_match_options {
    /**
     * @brief Match edge labels when true, otherwise use unlabeled structure.
     */
    bool match_edge_labels = true;

    /**
     * @brief Require induced matching among matched vertices when true.
     *
     * If enabled, non-edges in the pattern must remain non-edges between the
     * corresponding mapped target vertices (direction-aware for directed graphs).
     */
    bool induced = false;

    /**
     * @brief Search mode (exact exhaustive vs constrained heuristic).
     */
    subgraph_match_search_mode search_mode { subgraph_match_search_mode::exact };

    /**
     * @brief Vertex compatibility predicate used to filter candidate mappings.
     */
    std::function<bool(const PatternVertex&, const TargetVertex&)> vertex_compatible =
        [](const PatternVertex&, const TargetVertex&) { return true; };

    /**
     * @brief Edge-label equivalence predicate.
     */
    std::function<bool(const EdgeLabel&, const EdgeLabel&)> edge_label_equal =
        [](const EdgeLabel& a, const EdgeLabel& b) { return a == b; };

    /**
     * @brief Optional limit on explored backtracking states (`0` = unlimited).
     */
    std::size_t max_search_steps = 0;

    /**
     * @brief Optional per-pattern-vertex candidate cap after heuristic ordering.
     *
     * Only applied in `heuristic_constrained` mode.
     */
    std::size_t max_candidates_per_vertex = 0;

    /**
     * @brief Maximum number of witnesses to collect (`0` means collect none).
     */
    std::size_t max_witnesses = 1;
};

/**
 * @brief Rich result for subgraph matching.
 *
 * @tparam PatternVertex Pattern-graph vertex type.
 * @tparam TargetVertex Target-graph vertex type.
 * @tparam EdgeLabel Edge-label type.
 */
template<class PatternVertex, class TargetVertex, class EdgeLabel>
struct subgraph_match_result {
    subgraph_match_status status { subgraph_match_status::no_match };
    std::vector<subgraph_match_witness<PatternVertex, TargetVertex, EdgeLabel>> witnesses;
    std::vector<PatternVertex> search_order;
    std::size_t search_steps = 0;
    bool exhaustive = true;

    /**
     * @brief Returns true iff at least one witness was produced.
     */
    [[nodiscard]] bool matched() const noexcept {
        return status == subgraph_match_status::match_found && !witnesses.empty();
    }
};

/**
 * @brief Vertex-comparison options for structural equality checks.
 */
template<class Vertex>
struct structural_equality_options {
    std::function<bool(const Vertex&, const Vertex&)> vertex_equal =
        [](const Vertex& a, const Vertex& b) { return a == b; };
};

/**
 * @brief Options for graph isomorphism checks.
 *
 * @tparam Vertex Vertex type.
 * @tparam EdgeLabel Edge-label type.
 */
template<class Vertex, class EdgeLabel>
struct isomorphism_options {
    bool match_edge_labels = true;
    std::function<bool(const EdgeLabel&, const EdgeLabel&)> edge_label_equal =
        [](const EdgeLabel& a, const EdgeLabel& b) { return a == b; };
};

/**
 * @brief Rich result for graph isomorphism queries.
 *
 * @tparam VertexA Vertex type of left graph.
 * @tparam VertexB Vertex type of right graph.
 */
template<class VertexA, class VertexB>
struct isomorphism_result {
    isomorphism_status status { isomorphism_status::not_isomorphic };
    std::unordered_map<VertexA, VertexB> forward_map;
    std::unordered_map<VertexB, VertexA> reverse_map;
    std::vector<std::size_t> lhs_degree_profile;
    std::vector<std::size_t> rhs_degree_profile;

    /**
     * @brief Returns true iff an isomorphism witness mapping was found.
     */
    [[nodiscard]] bool isomorphic() const noexcept {
        return status == isomorphism_status::isomorphic;
    }
};

namespace detail {

template<class GraphA, class GraphB>
[[nodiscard]] std::vector<std::size_t> undirected_degree_profile(const GraphA& lhs, const GraphB& rhs, bool for_lhs) {
    std::vector<std::size_t> profile;
    if (for_lhs) {
        profile.reserve(lhs.nb_vertex());
        lhs.iter_vertex([&](const auto& v) {
            profile.push_back(lhs.succ(v).size() + lhs.pred(v).size());
        });
    } else {
        profile.reserve(rhs.nb_vertex());
        rhs.iter_vertex([&](const auto& v) {
            profile.push_back(rhs.succ(v).size() + rhs.pred(v).size());
        });
    }
    std::sort(profile.begin(), profile.end());
    return profile;
}

template<class GraphA, class GraphB>
[[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> directed_degree_profile(
    const GraphA& lhs,
    const GraphB& rhs,
    bool for_lhs
) {
    std::vector<std::pair<std::size_t, std::size_t>> profile;
    if (for_lhs) {
        profile.reserve(lhs.nb_vertex());
        lhs.iter_vertex([&](const auto& v) {
            profile.emplace_back(lhs.pred(v).size(), lhs.succ(v).size());
        });
    } else {
        profile.reserve(rhs.nb_vertex());
        rhs.iter_vertex([&](const auto& v) {
            profile.emplace_back(rhs.pred(v).size(), rhs.succ(v).size());
        });
    }
    std::sort(profile.begin(), profile.end());
    return profile;
}

template<class Graph, class Eq>
[[nodiscard]] std::size_t edge_multiplicity(
    const Graph& g,
    const typename Graph::vertex_type& src,
    const typename Graph::vertex_type& dst,
    Eq&& edge_label_equal,
    const typename Graph::edge_label_type& wanted
) {
    std::size_t count = 0;
    for (const auto& e : g.find_all_edges(src, dst)) {
        if (std::invoke(edge_label_equal, e.label, wanted)) ++count;
    }
    return count;
}

template<class GraphA, class GraphB, class Eq>
[[nodiscard]] bool compatible_under_mapping(
    const GraphA& lhs,
    const GraphB& rhs,
    const typename GraphA::vertex_type& lhs_v,
    const typename GraphB::vertex_type& rhs_v,
    const std::unordered_map<typename GraphA::vertex_type, typename GraphB::vertex_type>& fwd,
    const std::unordered_map<typename GraphB::vertex_type, typename GraphA::vertex_type>& rev,
    bool match_edge_labels,
    Eq&& edge_label_equal
) {
    using VA = typename GraphA::vertex_type;
    using VB = typename GraphB::vertex_type;
    using LA = typename GraphA::edge_label_type;

    auto check_from_lhs = [&](const VA& mapped_from, const VB& mapped_to, bool outgoing) -> bool {
        const auto neighbors = outgoing ? lhs.succ_e(mapped_from) : lhs.pred_e(mapped_from);
        for (const auto& e : neighbors) {
            const VA other_lhs = outgoing ? e.dst : e.src;
            auto it = fwd.find(other_lhs);
            if (it == fwd.end()) continue;
            const VB other_rhs = it->second;

            const VB rhs_src = outgoing ? mapped_to : other_rhs;
            const VB rhs_dst = outgoing ? other_rhs : mapped_to;
            if (!rhs.mem_edge(rhs_src, rhs_dst)) return false;

            if (match_edge_labels) {
                const auto rhs_edges = rhs.find_all_edges(rhs_src, rhs_dst);
                bool matched = false;
                for (const auto& re : rhs_edges) {
                    if (std::invoke(edge_label_equal, static_cast<LA>(e.label), static_cast<LA>(re.label))) {
                        matched = true;
                        break;
                    }
                }
                if (!matched) return false;
            }
        }
        return true;
    };

    if (!check_from_lhs(lhs_v, rhs_v, true)) return false;
    if (!check_from_lhs(lhs_v, rhs_v, false)) return false;

    auto check_from_rhs = [&](const VB& mapped_to, const VA& mapped_from, bool outgoing) -> bool {
        const auto neighbors = outgoing ? rhs.succ_e(mapped_to) : rhs.pred_e(mapped_to);
        for (const auto& e : neighbors) {
            const VB other_rhs = outgoing ? e.dst : e.src;
            auto it = rev.find(other_rhs);
            if (it == rev.end()) continue;
            const VA other_lhs = it->second;

            const VA lhs_src = outgoing ? mapped_from : other_lhs;
            const VA lhs_dst = outgoing ? other_lhs : mapped_from;
            if (!lhs.mem_edge(lhs_src, lhs_dst)) return false;

            if (match_edge_labels) {
                const auto lhs_edges = lhs.find_all_edges(lhs_src, lhs_dst);
                bool matched = false;
                for (const auto& le : lhs_edges) {
                    if (std::invoke(edge_label_equal, static_cast<LA>(le.label), static_cast<LA>(e.label))) {
                        matched = true;
                        break;
                    }
                }
                if (!matched) return false;
            }
        }
        return true;
    };

    if (!check_from_rhs(rhs_v, lhs_v, true)) return false;
    if (!check_from_rhs(rhs_v, lhs_v, false)) return false;
    return true;
}

template<class GraphA, class GraphB, class Eq>
[[nodiscard]] bool mapped_edges_preserved(
    const GraphA& lhs,
    const GraphB& rhs,
    const std::unordered_map<typename GraphA::vertex_type, typename GraphB::vertex_type>& fwd,
    bool match_edge_labels,
    Eq&& edge_label_equal
) {
    using VA = typename GraphA::vertex_type;
    using VB = typename GraphB::vertex_type;
    using LA = typename GraphA::edge_label_type;

    bool ok = true;
    lhs.iter_edges_e([&](const auto& e) {
        if (!ok) return;
        auto src_it = fwd.find(e.src);
        auto dst_it = fwd.find(e.dst);
        if (src_it == fwd.end() || dst_it == fwd.end()) {
            ok = false;
            return;
        }
        const VB mapped_src = src_it->second;
        const VB mapped_dst = dst_it->second;
        if (!rhs.mem_edge(mapped_src, mapped_dst)) {
            ok = false;
            return;
        }
        if (match_edge_labels) {
            const auto rhs_edges = rhs.find_all_edges(mapped_src, mapped_dst);
            bool matched = false;
            for (const auto& re : rhs_edges) {
                if (std::invoke(edge_label_equal, static_cast<LA>(e.label), static_cast<LA>(re.label))) {
                    matched = true;
                    break;
                }
            }
            if (!matched) ok = false;
        }
    });
    if (!ok) return false;

    rhs.iter_edges_e([&](const auto& e) {
        if (!ok) return;
        std::optional<VA> lhs_src;
        std::optional<VA> lhs_dst;
        for (const auto& [a, b] : fwd) {
            if (!lhs_src && b == e.src) lhs_src = a;
            if (!lhs_dst && b == e.dst) lhs_dst = a;
            if (lhs_src && lhs_dst) break;
        }
        if (!lhs_src || !lhs_dst) {
            ok = false;
            return;
        }
        if (!lhs.mem_edge(*lhs_src, *lhs_dst)) {
            ok = false;
            return;
        }
        if (match_edge_labels) {
            const auto lhs_edges = lhs.find_all_edges(*lhs_src, *lhs_dst);
            bool matched = false;
            for (const auto& le : lhs_edges) {
                if (std::invoke(edge_label_equal, static_cast<LA>(le.label), static_cast<LA>(e.label))) {
                    matched = true;
                    break;
                }
            }
            if (!matched) ok = false;
        }
    });
    return ok;
}

template<class PatternGraph, class TargetGraph, class Eq>
[[nodiscard]] bool compatible_subgraph_under_mapping(
    const PatternGraph& pattern,
    const TargetGraph& target,
    const typename PatternGraph::vertex_type& pattern_v,
    const typename TargetGraph::vertex_type& target_v,
    const std::unordered_map<typename PatternGraph::vertex_type, typename TargetGraph::vertex_type>& fwd,
    bool match_edge_labels,
    Eq&& edge_label_equal,
    bool induced
) {
    using PV = typename PatternGraph::vertex_type;
    using TV = typename TargetGraph::vertex_type;

    for (const auto& [other_pattern, other_target] : fwd) {
        if (other_pattern == pattern_v) continue;

        const bool p_out = pattern.mem_edge(pattern_v, other_pattern);
        const bool p_in  = pattern.mem_edge(other_pattern, pattern_v);
        const bool t_out = target.mem_edge(target_v, other_target);
        const bool t_in  = target.mem_edge(other_target, target_v);

        if (p_out && !t_out) return false;
        if (p_in && !t_in) return false;
        if (induced) {
            if (!p_out && t_out) return false;
            if (!p_in && t_in) return false;
        }

        if (match_edge_labels && p_out) {
            const auto pattern_edges = pattern.find_all_edges(pattern_v, other_pattern);
            for (const auto& pe : pattern_edges) {
                const auto target_edges = target.find_all_edges(target_v, other_target);
                bool label_ok = false;
                for (const auto& te : target_edges) {
                    if (std::invoke(edge_label_equal, pe.label, te.label)) {
                        label_ok = true;
                        break;
                    }
                }
                if (!label_ok) return false;
            }
        }
        if (match_edge_labels && p_in) {
            const auto pattern_edges = pattern.find_all_edges(other_pattern, pattern_v);
            for (const auto& pe : pattern_edges) {
                const auto target_edges = target.find_all_edges(other_target, target_v);
                bool label_ok = false;
                for (const auto& te : target_edges) {
                    if (std::invoke(edge_label_equal, pe.label, te.label)) {
                        label_ok = true;
                        break;
                    }
                }
                if (!label_ok) return false;
            }
        }
    }
    return true;
}

template<class PatternGraph, class TargetGraph, class Eq>
[[nodiscard]] std::vector<subgraph_edge_witness<typename PatternGraph::vertex_type, typename TargetGraph::vertex_type, typename PatternGraph::edge_label_type>>
build_subgraph_edge_witnesses(
    const PatternGraph& pattern,
    const TargetGraph& target,
    const std::unordered_map<typename PatternGraph::vertex_type, typename TargetGraph::vertex_type>& fwd,
    bool match_edge_labels,
    Eq&& edge_label_equal
) {
    using PV = typename PatternGraph::vertex_type;
    using TV = typename TargetGraph::vertex_type;
    using L  = typename PatternGraph::edge_label_type;
    using witness_t = subgraph_edge_witness<PV, TV, L>;
    std::vector<witness_t> out;
    out.reserve(pattern.nb_edges());

    pattern.iter_edges_e([&](const auto& pe) {
        auto src_it = fwd.find(pe.src);
        auto dst_it = fwd.find(pe.dst);
        if (src_it == fwd.end() || dst_it == fwd.end()) return;
        const TV mapped_src = src_it->second;
        const TV mapped_dst = dst_it->second;

        edge<TV, L> picked { mapped_src, mapped_dst, {} };
        bool found = false;
        for (const auto& te : target.find_all_edges(mapped_src, mapped_dst)) {
            if (!match_edge_labels || std::invoke(edge_label_equal, pe.label, te.label)) {
                picked = edge<TV, L> { te.src, te.dst, te.label };
                found = true;
                break;
            }
        }
        if (!found) return;
        out.push_back(witness_t {
            edge<PV, L> { pe.src, pe.dst, pe.label },
            picked
        });
    });
    return out;
}

} // namespace detail

/**
 * @brief Checks representational structural equality (same vertex identities).
 *
 * Requires matching directionality, vertex/edge counts, vertex identity set,
 * and edge membership (including labels through edge identity).
 *
 * @tparam GraphA Left graph type.
 * @tparam GraphB Right graph type.
 * @param lhs Left graph.
 * @param rhs Right graph.
 * @param options Vertex equality options.
 */
template<class GraphA, class GraphB>
[[nodiscard]] bool structurally_equal(
    const GraphA& lhs,
    const GraphB& rhs,
    structural_equality_options<typename GraphA::vertex_type> options = {}
) requires (
    std::same_as<typename GraphA::vertex_type, typename GraphB::vertex_type> &&
    std::same_as<typename GraphA::edge_label_type, typename GraphB::edge_label_type>
) {
    using VA = typename GraphA::vertex_type;
    if (lhs.is_directed() != rhs.is_directed()) return false;
    if (lhs.nb_vertex() != rhs.nb_vertex()) return false;
    if (lhs.nb_edges() != rhs.nb_edges()) return false;

    std::vector<VA> lhs_vertices;
    lhs_vertices.reserve(lhs.nb_vertex());
    lhs.iter_vertex([&](const auto& v) { lhs_vertices.push_back(v); });
    for (const auto& v : lhs_vertices) {
        bool found = false;
        rhs.iter_vertex([&](const auto& rv) {
            if (!found && std::invoke(options.vertex_equal, v, static_cast<VA>(rv))) found = true;
        });
        if (!found) return false;
    }

    bool edge_ok = true;
    lhs.iter_edges_e([&](const auto& e) {
        if (!edge_ok) return;
        if (!rhs.mem_edge_e(typename GraphB::edge_type{e.src, e.dst, e.label})) edge_ok = false;
    });
    if (!edge_ok) return false;

    rhs.iter_edges_e([&](const auto& e) {
        if (!edge_ok) return;
        if (!lhs.mem_edge_e(typename GraphA::edge_type{e.src, e.dst, e.label})) edge_ok = false;
    });
    return edge_ok;
}

/**
 * @brief Alias for representational equality check.
 */
template<class GraphA, class GraphB>
[[nodiscard]] bool representationally_equal(const GraphA& lhs, const GraphB& rhs) {
    return structurally_equal(lhs, rhs);
}

/**
 * @brief Exact graph isomorphism with witness mapping and fast rejects.
 *
 * Uses degree/profile filters and backtracking search. On success, returns
 * bijection maps (`forward_map`, `reverse_map`) preserving adjacency and,
 * optionally, edge labels.
 *
 * @tparam GraphA Left graph type.
 * @tparam GraphB Right graph type.
 */
template<class GraphA, class GraphB>
[[nodiscard]] isomorphism_result<typename GraphA::vertex_type, typename GraphB::vertex_type>
graph_isomorphism(
    const GraphA& lhs,
    const GraphB& rhs,
    isomorphism_options<typename GraphA::vertex_type, typename GraphA::edge_label_type> options = {}
) requires (
    std::same_as<typename GraphA::edge_label_type, typename GraphB::edge_label_type>
) {
    using VA = typename GraphA::vertex_type;
    using VB = typename GraphB::vertex_type;
    using LA = typename GraphA::edge_label_type;
    isomorphism_result<VA, VB> out;

    if (lhs.is_directed() != rhs.is_directed()) {
        out.status = isomorphism_status::directed_mismatch;
        return out;
    }
    if (lhs.nb_vertex() != rhs.nb_vertex() || lhs.nb_edges() != rhs.nb_edges()) {
        out.status = isomorphism_status::size_mismatch;
        return out;
    }

    const bool directed = lhs.is_directed();
    const auto lhs_ud = detail::undirected_degree_profile(lhs, rhs, true);
    const auto rhs_ud = detail::undirected_degree_profile(lhs, rhs, false);
    out.lhs_degree_profile = lhs_ud;
    out.rhs_degree_profile = rhs_ud;
    if (lhs_ud != rhs_ud) {
        out.status = isomorphism_status::degree_profile_mismatch;
        return out;
    }
    if (directed) {
        const auto lhs_dd = detail::directed_degree_profile(lhs, rhs, true);
        const auto rhs_dd = detail::directed_degree_profile(lhs, rhs, false);
        if (lhs_dd != rhs_dd) {
            out.status = isomorphism_status::degree_profile_mismatch;
            return out;
        }
    }

    if (options.match_edge_labels) {
        std::vector<std::string> lhs_labels;
        std::vector<std::string> rhs_labels;
        lhs_labels.reserve(lhs.nb_edges());
        rhs_labels.reserve(rhs.nb_edges());
        lhs.iter_edges_e([&](const auto& e) { lhs_labels.push_back(grafitt::to_string_fallback(e.label)); });
        rhs.iter_edges_e([&](const auto& e) { rhs_labels.push_back(grafitt::to_string_fallback(e.label)); });
        std::sort(lhs_labels.begin(), lhs_labels.end());
        std::sort(rhs_labels.begin(), rhs_labels.end());
        if (lhs_labels != rhs_labels) {
            out.status = isomorphism_status::edge_label_profile_mismatch;
            return out;
        }
    }

    std::vector<VA> lhs_vertices;
    lhs_vertices.reserve(lhs.nb_vertex());
    lhs.iter_vertex([&](const auto& v) { lhs_vertices.push_back(v); });
    std::sort(lhs_vertices.begin(), lhs_vertices.end(), [&](const VA& a, const VA& b) {
        const std::size_t da = lhs.succ(a).size() + lhs.pred(a).size();
        const std::size_t db = lhs.succ(b).size() + lhs.pred(b).size();
        if (da != db) return da > db;
        return grafitt::to_string_fallback(a) < grafitt::to_string_fallback(b);
    });

    std::vector<VB> rhs_vertices;
    rhs_vertices.reserve(rhs.nb_vertex());
    rhs.iter_vertex([&](const auto& v) { rhs_vertices.push_back(v); });

    std::unordered_map<VA, VB> fwd;
    std::unordered_map<VB, VA> rev;

    auto candidate_degree_compatible = [&](const VA& a, const VB& b) -> bool {
        const auto au = lhs.succ(a).size() + lhs.pred(a).size();
        const auto bu = rhs.succ(b).size() + rhs.pred(b).size();
        if (au != bu) return false;
        if (directed) {
            if (lhs.succ(a).size() != rhs.succ(b).size()) return false;
            if (lhs.pred(a).size() != rhs.pred(b).size()) return false;
        }
        return true;
    };

    std::function<bool(std::size_t)> backtrack = [&](std::size_t idx) -> bool {
        if (idx == lhs_vertices.size()) {
            return detail::mapped_edges_preserved(lhs, rhs, fwd, options.match_edge_labels, options.edge_label_equal);
        }

        const VA a = lhs_vertices[idx];
        for (const auto& b : rhs_vertices) {
            if (rev.contains(b)) continue;
            if (!candidate_degree_compatible(a, b)) continue;

            fwd[a] = b;
            rev[b] = a;

            if (detail::compatible_under_mapping(lhs, rhs, a, b, fwd, rev, options.match_edge_labels, options.edge_label_equal)) {
                if (backtrack(idx + 1)) return true;
            }

            fwd.erase(a);
            rev.erase(b);
        }
        return false;
    };

    if (backtrack(0)) {
        out.status = isomorphism_status::isomorphic;
        out.forward_map = std::move(fwd);
        out.reverse_map = std::move(rev);
        return out;
    }

    out.status = isomorphism_status::not_isomorphic;
    return out;
}

/**
 * @brief Finds subgraph matches of `pattern` inside `target`.
 *
 * The routine supports exact exhaustive matching and constrained heuristic
 * matching through `subgraph_match_options::search_mode`.
 *
 * @tparam PatternGraph Pattern graph type.
 * @tparam TargetGraph Target graph type.
 * @param pattern Pattern graph to embed.
 * @param target Target graph where embeddings are searched.
 * @param options Matching options and constraints.
 * @return Rich matching result including vertex bindings and edge witnesses.
 *
 * @pre `PatternGraph::edge_label_type` equals `TargetGraph::edge_label_type`.
 * @post `status == match_found` implies `witnesses` is non-empty.
 * @complexity Worst-case exponential in `|V(pattern)|` (subgraph isomorphism).
 */
template<class PatternGraph, class TargetGraph>
[[nodiscard]] subgraph_match_result<
    typename PatternGraph::vertex_type,
    typename TargetGraph::vertex_type,
    typename PatternGraph::edge_label_type
>
subgraph_match(
    const PatternGraph& pattern,
    const TargetGraph& target,
    subgraph_match_options<
        typename PatternGraph::vertex_type,
        typename TargetGraph::vertex_type,
        typename PatternGraph::edge_label_type
    > options = {}
) requires (
    std::same_as<typename PatternGraph::edge_label_type, typename TargetGraph::edge_label_type>
) {
    using PV = typename PatternGraph::vertex_type;
    using TV = typename TargetGraph::vertex_type;
    using L  = typename PatternGraph::edge_label_type;
    using result_t = subgraph_match_result<PV, TV, L>;

    result_t out;

    if (pattern.is_directed() != target.is_directed()) {
        out.status = subgraph_match_status::directed_mismatch;
        return out;
    }
    if (pattern.nb_vertex() > target.nb_vertex()) {
        out.status = subgraph_match_status::pattern_larger_than_target;
        return out;
    }

    if (pattern.nb_vertex() == 0) {
        out.status = subgraph_match_status::match_found;
        if (options.max_witnesses > 0) out.witnesses.emplace_back();
        return out;
    }

    std::vector<PV> p_vertices;
    pattern.iter_vertex([&](const auto& v) { p_vertices.push_back(v); });
    std::sort(p_vertices.begin(), p_vertices.end(), [&](const PV& a, const PV& b) {
        const auto da = pattern.succ(a).size() + pattern.pred(a).size();
        const auto db = pattern.succ(b).size() + pattern.pred(b).size();
        if (da != db) return da > db;
        return grafitt::to_string_fallback(a) < grafitt::to_string_fallback(b);
    });
    out.search_order = p_vertices;

    std::vector<TV> t_vertices;
    target.iter_vertex([&](const auto& v) { t_vertices.push_back(v); });

    const bool directed = pattern.is_directed();
    const bool constrained = options.search_mode == subgraph_match_search_mode::heuristic_constrained;

    std::unordered_map<PV, TV> fwd;
    std::unordered_map<TV, PV> rev;
    bool exhausted_budget = false;

    auto degree_compatible = [&](const PV& pv, const TV& tv) -> bool {
        const auto pu = pattern.succ(pv).size() + pattern.pred(pv).size();
        const auto tu = target.succ(tv).size() + target.pred(tv).size();
        if (pu > tu) return false;
        if (directed) {
            if (pattern.succ(pv).size() > target.succ(tv).size()) return false;
            if (pattern.pred(pv).size() > target.pred(tv).size()) return false;
        }
        return true;
    };

    auto build_candidates = [&](const PV& pv) {
        std::vector<TV> candidates;
        candidates.reserve(t_vertices.size());
        for (const auto& tv : t_vertices) {
            if (rev.contains(tv)) continue;
            if (!std::invoke(options.vertex_compatible, pv, tv)) continue;
            if (!degree_compatible(pv, tv)) continue;
            candidates.push_back(tv);
        }
        std::sort(candidates.begin(), candidates.end(), [&](const TV& a, const TV& b) {
            const auto deg_a = target.succ(a).size() + target.pred(a).size();
            const auto deg_b = target.succ(b).size() + target.pred(b).size();
            if (deg_a != deg_b) return deg_a < deg_b;
            return grafitt::to_string_fallback(a) < grafitt::to_string_fallback(b);
        });
        if (constrained && options.max_candidates_per_vertex > 0 && candidates.size() > options.max_candidates_per_vertex) {
            candidates.resize(options.max_candidates_per_vertex);
        }
        return candidates;
    };

    std::function<void(std::size_t)> backtrack = [&](std::size_t idx) {
        if (exhausted_budget) return;

        if (constrained && options.max_search_steps > 0 && out.search_steps >= options.max_search_steps) {
            exhausted_budget = true;
            return;
        }
        ++out.search_steps;

        if (idx == p_vertices.size()) {
            if (options.max_witnesses == 0) return;
            subgraph_match_witness<PV, TV, L> witness;
            witness.vertex_bindings = fwd;
            witness.reverse_vertex_bindings = rev;
            witness.edge_witnesses = detail::build_subgraph_edge_witnesses(
                pattern,
                target,
                witness.vertex_bindings,
                options.match_edge_labels,
                options.edge_label_equal
            );
            out.witnesses.push_back(std::move(witness));
            if (out.witnesses.size() >= options.max_witnesses) return;
            return;
        }

        const PV pv = p_vertices[idx];
        auto candidates = build_candidates(pv);
        for (const auto& tv : candidates) {
            if (exhausted_budget) return;
            if (out.witnesses.size() >= options.max_witnesses && options.max_witnesses > 0) return;

            fwd[pv] = tv;
            rev[tv] = pv;
            const bool compatible = detail::compatible_subgraph_under_mapping(
                pattern,
                target,
                pv,
                tv,
                fwd,
                options.match_edge_labels,
                options.edge_label_equal,
                options.induced
            );
            if (compatible) backtrack(idx + 1);
            fwd.erase(pv);
            rev.erase(tv);
        }
    };

    backtrack(0);

    if (!out.witnesses.empty()) {
        out.status = subgraph_match_status::match_found;
    } else if (exhausted_budget) {
        out.status = subgraph_match_status::search_budget_exhausted;
        out.exhaustive = false;
    } else {
        out.status = subgraph_match_status::no_match;
    }
    if (constrained) out.exhaustive = !exhausted_budget;
    return out;
}

/**
 * @brief Constrained heuristic subgraph matching convenience wrapper.
 *
 * Sets `options.search_mode` to `heuristic_constrained` and then dispatches to
 * `subgraph_match`.
 */
template<class PatternGraph, class TargetGraph>
[[nodiscard]] subgraph_match_result<
    typename PatternGraph::vertex_type,
    typename TargetGraph::vertex_type,
    typename PatternGraph::edge_label_type
>
subgraph_match_heuristic(
    const PatternGraph& pattern,
    const TargetGraph& target,
    subgraph_match_options<
        typename PatternGraph::vertex_type,
        typename TargetGraph::vertex_type,
        typename PatternGraph::edge_label_type
    > options
) requires (
    std::same_as<typename PatternGraph::edge_label_type, typename TargetGraph::edge_label_type>
) {
    options.search_mode = subgraph_match_search_mode::heuristic_constrained;
    return subgraph_match(pattern, target, std::move(options));
}

template<class Graph>
[[nodiscard]] std::size_t degree(const Graph& g, const typename Graph::vertex_type& v) {
    return g.succ(v).size() + g.pred(v).size();
}

template<class Graph>
[[nodiscard]] std::vector<typename Graph::vertex_type>
vertices(const Graph& g) {
    std::vector<typename Graph::vertex_type> out;
    g.iter_vertex([&](const auto& v) { out.push_back(v); });
    return out;
}

template<class Graph>
[[nodiscard]] std::vector<typename Graph::edge_type>
edges(const Graph& g) {
    std::vector<typename Graph::edge_type> out;
    g.iter_edges_e([&](const auto& e) { out.push_back(e); });
    return out;
}

} // namespace algo

// ============================================================
// Visualization/export
// ============================================================

namespace vizz {

/**
 * @brief Styling and label options for DOT graph export.
 */
template<class Graph>
struct dot_export_options {
    using vertex_type = typename Graph::vertex_type;
    using edge_type = typename Graph::edge_type;

    std::string graph_name = "G";
    std::function<std::string(const vertex_type&)> vertex_id = [](const auto& v) {
        return grafitt::to_string_fallback(v);
    };
    std::function<std::string(const vertex_type&)> vertex_label = [](const auto& v) {
        return grafitt::to_string_fallback(v);
    };
    std::function<std::string(const edge_type&)> edge_label = [](const auto& e) {
        return grafitt::to_string_fallback(e.label);
    };
    std::function<std::string(const vertex_type&)> vertex_style = [](const auto&) {
        return std::string{};
    };
    std::function<std::string(const edge_type&)> edge_style = [](const auto&) {
        return std::string{};
    };
    bool include_edge_labels = true;
};

/**
 * @brief Styling and label options for TikZ graph export.
 */
template<class Graph>
struct tikz_export_options {
    using vertex_type = typename Graph::vertex_type;
    using edge_type = typename Graph::edge_type;

    std::function<std::string(const vertex_type&)> vertex_id = [](const auto& v) {
        return grafitt::to_string_fallback(v);
    };
    std::function<std::string(const vertex_type&)> vertex_label = [](const auto& v) {
        return grafitt::to_string_fallback(v);
    };
    std::function<std::string(const edge_type&)> edge_label = [](const auto& e) {
        return grafitt::to_string_fallback(e.label);
    };
    std::function<std::string(const vertex_type&)> vertex_style = [](const auto&) {
        return std::string{};
    };
    std::function<std::string(const edge_type&)> edge_style = [](const auto&) {
        return std::string{};
    };
    bool include_edge_labels = true;
    double radius = 3.0;
};

/**
 * @brief DOT options for condensation DAG rendering.
 */
template<class Vertex>
struct condensation_dot_export_options {
    std::string graph_name = "Condensation";
    std::function<std::string(std::size_t)> component_label = [](std::size_t cid) {
        return std::string("C") + std::to_string(cid);
    };
    std::function<std::string(std::size_t, const std::vector<Vertex>&)> component_style =
        [](std::size_t, const std::vector<Vertex>&) { return std::string{}; };
    std::function<std::string(const algo::condensation_edge_witness<Vertex>&)> edge_style =
        [](const auto&) { return std::string{}; };
    bool include_component_vertices = true;
};

/**
 * @brief Serialization options for Graph.js-style JSON export.
 */
template<class Graph>
struct graphjs_export_options {
    using vertex_type = typename Graph::vertex_type;
    using edge_type = typename Graph::edge_type;

    std::function<std::string(const vertex_type&)> vertex_id = [](const auto& v) {
        return grafitt::to_string_fallback(v);
    };
    std::function<std::string(const vertex_type&)> vertex_label = [](const auto& v) {
        return grafitt::to_string_fallback(v);
    };
    std::function<std::string(const edge_type&)> edge_label = [](const auto& e) {
        return grafitt::to_string_fallback(e.label);
    };
    std::function<std::string(const vertex_type&)> vertex_class = [](const auto&) {
        return std::string{};
    };
    std::function<std::string(const edge_type&)> edge_class = [](const auto&) {
        return std::string{};
    };
    bool include_edge_labels = true;
};

/**
 * @brief Serialization options for condensation Graph.js-style JSON export.
 */
template<class Vertex>
struct condensation_graphjs_export_options {
    std::function<std::string(std::size_t)> component_label = [](std::size_t cid) {
        return std::string("C") + std::to_string(cid);
    };
    std::function<std::string(std::size_t, const std::vector<Vertex>&)> component_class =
        [](std::size_t, const std::vector<Vertex>&) { return std::string{}; };
    std::function<std::string(const algo::condensation_edge_witness<Vertex>&)> edge_class =
        [](const auto&) { return std::string{}; };
    bool include_component_vertices = true;
};

namespace detail {

[[nodiscard]] inline std::string escape_dot(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

[[nodiscard]] inline std::string escape_tikz(std::string_view in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        switch (c) {
            case '\\':
            case '{':
            case '}':
            case '_':
            case '%':
            case '#':
            case '&':
            case '$':
                out.push_back('\\');
                out.push_back(c);
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}

[[nodiscard]] inline std::string escape_json(std::string_view in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}

template<class Graph, class Options>
[[nodiscard]] std::vector<typename Graph::vertex_type> collect_vertices(const Graph& g, const Options& opt) {
    using V = typename Graph::vertex_type;
    std::vector<V> vs;
    vs.reserve(g.nb_vertex());
    g.iter_vertex([&](const auto& v) { vs.push_back(v); });
    std::sort(vs.begin(), vs.end(), [&](const V& a, const V& b) {
        return opt.vertex_id(a) < opt.vertex_id(b);
    });
    return vs;
}

} // namespace detail

/**
 * @brief Creates a vertex-style callback that highlights selected vertices.
 *
 * @tparam Vertex Vertex type.
 * @param highlighted Vertices to style.
 * @param highlighted_style Style string for highlighted vertices.
 * @param default_style Style string for non-highlighted vertices.
 */
template<class Vertex>
[[nodiscard]] auto dot_vertex_highlight_style(
    std::unordered_set<Vertex> highlighted,
    std::string highlighted_style = "style=filled, fillcolor=gold",
    std::string default_style = ""
) {
    return [highlighted = std::move(highlighted),
        highlighted_style = std::move(highlighted_style),
        default_style = std::move(default_style)](const Vertex& v) -> std::string {
        if (highlighted.contains(v)) return highlighted_style;
        return default_style;
    };
}

/**
 * @brief Creates an edge-style callback that highlights one directed path.
 *
 * @tparam Graph Graph type.
 * @param path Vertex sequence path (`v0, v1, ..., vk`).
 * @param highlighted_style Style string for path edges.
 * @param default_style Style string for non-path edges.
 */
template<class Graph>
[[nodiscard]] auto dot_edge_path_highlight_style(
    const std::vector<typename Graph::vertex_type>& path,
    std::string highlighted_style = "color=red, penwidth=2",
    std::string default_style = ""
) {
    using V = typename Graph::vertex_type;
    using edge_key = std::pair<V, V>;
    struct edge_key_hash {
        std::size_t operator()(const edge_key& k) const noexcept {
            return std::hash<V>{}(k.first) ^ (std::hash<V>{}(k.second) << 1);
        }
    };

    std::unordered_set<edge_key, edge_key_hash> highlighted_edges;
    if (path.size() >= 2) {
        for (std::size_t i = 0; i + 1 < path.size(); ++i) {
            highlighted_edges.insert(edge_key{path[i], path[i + 1]});
        }
    }

    return [highlighted_edges = std::move(highlighted_edges),
        highlighted_style = std::move(highlighted_style),
        default_style = std::move(default_style)](const typename Graph::edge_type& e) -> std::string {
        if (highlighted_edges.contains(edge_key{e.src, e.dst})) return highlighted_style;
        return default_style;
    };
}

/**
 * @brief Creates an edge-style callback that highlights one cycle witness.
 *
 * Accepts witness format where first and last vertices may be equal.
 */
template<class Graph>
[[nodiscard]] auto dot_edge_cycle_highlight_style(
    const std::vector<typename Graph::vertex_type>& cycle_witness,
    std::string highlighted_style = "color=orange, penwidth=2",
    std::string default_style = ""
) {
    return dot_edge_path_highlight_style<Graph>(cycle_witness, std::move(highlighted_style), std::move(default_style));
}

/**
 * @brief Creates component-style callback for condensation DOT export.
 *
 * Components larger than one vertex are emphasized by default.
 */
template<class Vertex>
[[nodiscard]] auto condensation_component_size_style(
    std::string multi_vertex_style = "style=filled, fillcolor=lightblue",
    std::string singleton_style = ""
) {
    return [multi_vertex_style = std::move(multi_vertex_style),
        singleton_style = std::move(singleton_style)](std::size_t, const std::vector<Vertex>& component) -> std::string {
        if (component.size() > 1) return multi_vertex_style;
        return singleton_style;
    };
}

/**
 * @brief Exports graph to GraphViz DOT text.
 *
 * @tparam Graph Graph type exposing Grafitt graph API.
 * @param g Input graph.
 * @param options Label/style hooks and naming options.
 * @return DOT source text.
 */
template<class Graph>
[[nodiscard]] std::string to_dot(
    const Graph& g,
    dot_export_options<Graph> options = {}
) {
    using V = typename Graph::vertex_type;
    std::ostringstream oss;
    const bool directed = g.is_directed();
    const char* graph_kw = directed ? "digraph" : "graph";
    const char* edge_op = directed ? "->" : "--";

    oss << graph_kw << " \"" << detail::escape_dot(options.graph_name) << "\" {\n";
    const auto verts = detail::collect_vertices(g, options);
    for (const auto& v : verts) {
        const auto id = detail::escape_dot(options.vertex_id(v));
        const auto label = detail::escape_dot(options.vertex_label(v));
        const auto style = options.vertex_style(v);
        oss << "  \"" << id << "\" [label=\"" << label << "\"";
        if (!style.empty()) oss << ", " << style;
        oss << "];\n";
    }

    g.iter_edges_e([&](const auto& e) {
        const auto src = detail::escape_dot(options.vertex_id(e.src));
        const auto dst = detail::escape_dot(options.vertex_id(e.dst));
        const auto style = options.edge_style(e);
        oss << "  \"" << src << "\" " << edge_op << " \"" << dst << "\"";
        const auto lbl = detail::escape_dot(options.edge_label(e));
        if (options.include_edge_labels || !style.empty()) {
            oss << " [";
            bool need_sep = false;
            if (options.include_edge_labels) {
                oss << "label=\"" << lbl << "\"";
                need_sep = true;
            }
            if (!style.empty()) {
                if (need_sep) oss << ", ";
                oss << style;
            }
            oss << "]";
        }
        oss << ";\n";
    });
    oss << "}\n";
    return oss.str();
}

/**
 * @brief Exports graph to LaTeX TikZ text.
 *
 * Uses a simple circular layout by default and emits edge labels optionally.
 *
 * @tparam Graph Graph type exposing Grafitt graph API.
 * @param g Input graph.
 * @param options Label/style hooks and layout options.
 * @return TikZ source text.
 */
template<class Graph>
[[nodiscard]] std::string to_tikz(
    const Graph& g,
    tikz_export_options<Graph> options = {}
) {
    using V = typename Graph::vertex_type;
    std::ostringstream oss;
    const auto verts = detail::collect_vertices(g, options);
    const bool directed = g.is_directed();

    std::unordered_map<V, std::size_t> index_of;
    for (std::size_t i = 0; i < verts.size(); ++i) index_of[verts[i]] = i;

    constexpr double two_pi = 6.28318530717958647692;
    const std::size_t n = verts.size();
    const double radius = options.radius > 0.0 ? options.radius : 3.0;

    oss << "\\begin{tikzpicture}[>=Stealth, node distance=2cm]\n";
    for (std::size_t i = 0; i < n; ++i) {
        const auto& v = verts[i];
        const double angle = n > 0 ? (two_pi * static_cast<double>(i) / static_cast<double>(n)) : 0.0;
        const double x = radius * std::cos(angle);
        const double y = radius * std::sin(angle);
        const auto id = detail::escape_tikz(options.vertex_id(v));
        const auto lbl = detail::escape_tikz(options.vertex_label(v));
        const auto style = options.vertex_style(v);

        oss << "  \\node";
        if (!style.empty()) oss << "[" << style << "]";
        oss << " (" << id << ") at (" << x << "," << y << ") {" << lbl << "};\n";
    }

    g.iter_edges_e([&](const auto& e) {
        const auto src = detail::escape_tikz(options.vertex_id(e.src));
        const auto dst = detail::escape_tikz(options.vertex_id(e.dst));
        const auto style = options.edge_style(e);
        oss << "  \\draw";
        if (!style.empty()) {
            oss << "[" << style << "]";
        } else if (directed) {
            oss << "[->]";
        }
        oss << " (" << src << ") -- (" << dst << ")";
        if (options.include_edge_labels) {
            const auto lbl = detail::escape_tikz(options.edge_label(e));
            oss << " node[midway, above] {" << lbl << "}";
        }
        oss << ";\n";
    });
    oss << "\\end{tikzpicture}\n";
    return oss.str();
}

/**
 * @brief Exports SCC condensation result to GraphViz DOT text.
 *
 * @tparam Vertex Original graph vertex type.
 * @param cond Condensation graph result.
 * @param options Condensation-specific rendering options.
 * @return DOT source text.
 */
template<class Vertex>
[[nodiscard]] std::string to_dot(
    const algo::condensation_graph_result<Vertex>& cond,
    condensation_dot_export_options<Vertex> options = {}
) {
    std::ostringstream oss;
    oss << "digraph \"" << detail::escape_dot(options.graph_name) << "\" {\n";
    for (std::size_t cid = 0; cid < cond.components.size(); ++cid) {
        std::ostringstream label_oss;
        label_oss << options.component_label(cid);
        if (options.include_component_vertices) {
            label_oss << "\\n{";
            for (std::size_t i = 0; i < cond.components[cid].size(); ++i) {
                if (i) label_oss << ", ";
                label_oss << grafitt::to_string_fallback(cond.components[cid][i]);
            }
            label_oss << "}";
        }
        const auto style = options.component_style(cid, cond.components[cid]);
        oss << "  \"" << cid << "\" [label=\"" << detail::escape_dot(label_oss.str()) << "\"";
        if (!style.empty()) oss << ", " << style;
        oss << "];\n";
    }

    for (const auto& w : cond.edge_witnesses) {
        const auto style = options.edge_style(w);
        oss << "  \"" << w.src_component << "\" -> \"" << w.dst_component << "\"";
        if (!style.empty()) oss << " [" << style << "]";
        oss << ";\n";
    }
    oss << "}\n";
    return oss.str();
}

/**
 * @brief Exports graph as Graph.js-style JSON (`nodes`/`edges`) payload.
 *
 * @tparam Graph Graph type exposing Grafitt graph API.
 * @param g Input graph.
 * @param options Label/class hooks.
 * @return JSON string suitable for Graph.js-like consumers.
 */
template<class Graph>
[[nodiscard]] std::string to_graphjs(
    const Graph& g,
    graphjs_export_options<Graph> options = {}
) {
    std::ostringstream oss;
    const auto verts = detail::collect_vertices(g, options);
    oss << "{\n";
    oss << "  \"directed\": " << (g.is_directed() ? "true" : "false") << ",\n";
    oss << "  \"nodes\": [\n";
    for (std::size_t i = 0; i < verts.size(); ++i) {
        const auto& v = verts[i];
        const auto id = detail::escape_json(options.vertex_id(v));
        const auto label = detail::escape_json(options.vertex_label(v));
        const auto klass = detail::escape_json(options.vertex_class(v));
        oss << "    {\"id\":\"" << id << "\",\"label\":\"" << label << "\"";
        if (!klass.empty()) oss << ",\"class\":\"" << klass << "\"";
        oss << "}";
        if (i + 1 < verts.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ],\n";
    oss << "  \"edges\": [\n";
    std::vector<typename Graph::edge_type> es;
    es.reserve(g.nb_edges());
    g.iter_edges_e([&](const auto& e) { es.push_back(e); });
    for (std::size_t i = 0; i < es.size(); ++i) {
        const auto& e = es[i];
        const auto src = detail::escape_json(options.vertex_id(e.src));
        const auto dst = detail::escape_json(options.vertex_id(e.dst));
        const auto klass = detail::escape_json(options.edge_class(e));
        oss << "    {\"source\":\"" << src << "\",\"target\":\"" << dst << "\"";
        if (options.include_edge_labels) {
            const auto label = detail::escape_json(options.edge_label(e));
            oss << ",\"label\":\"" << label << "\"";
        }
        if (!klass.empty()) oss << ",\"class\":\"" << klass << "\"";
        oss << "}";
        if (i + 1 < es.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

/**
 * @brief Exports condensation DAG as Graph.js-style JSON payload.
 *
 * @tparam Vertex Original graph vertex type.
 * @param cond Condensation result.
 * @param options Label/class hooks.
 * @return JSON string with component provenance summary.
 */
template<class Vertex>
[[nodiscard]] std::string to_graphjs(
    const algo::condensation_graph_result<Vertex>& cond,
    condensation_graphjs_export_options<Vertex> options = {}
) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"directed\": true,\n";
    oss << "  \"nodes\": [\n";
    for (std::size_t cid = 0; cid < cond.components.size(); ++cid) {
        const auto label = detail::escape_json(options.component_label(cid));
        const auto klass = detail::escape_json(options.component_class(cid, cond.components[cid]));
        oss << "    {\"id\":\"" << cid << "\",\"label\":\"" << label << "\"";
        if (!klass.empty()) oss << ",\"class\":\"" << klass << "\"";
        if (options.include_component_vertices) {
            std::ostringstream vertices_oss;
            vertices_oss << "[";
            for (std::size_t i = 0; i < cond.components[cid].size(); ++i) {
                if (i) vertices_oss << ",";
                vertices_oss << "\"" << detail::escape_json(grafitt::to_string_fallback(cond.components[cid][i])) << "\"";
            }
            vertices_oss << "]";
            oss << ",\"vertices\":" << vertices_oss.str();
        }
        oss << "}";
        if (cid + 1 < cond.components.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ],\n";
    oss << "  \"edges\": [\n";
    for (std::size_t i = 0; i < cond.edge_witnesses.size(); ++i) {
        const auto& w = cond.edge_witnesses[i];
        const auto klass = detail::escape_json(options.edge_class(w));
        oss << "    {\"source\":\"" << w.src_component
            << "\",\"target\":\"" << w.dst_component << "\"";
        if (!klass.empty()) oss << ",\"class\":\"" << klass << "\"";
        oss << "}";
        if (i + 1 < cond.edge_witnesses.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

/**
 * @brief Exports condensation DAG cover pieces as DOT subgraphs.
 */
template<class Vertex>
[[nodiscard]] std::string to_dot(
    const algo::condensation_dag_cover_result<Vertex>& cover,
    condensation_dot_export_options<Vertex> options = {}
) {
    std::ostringstream oss;
    oss << "digraph \"" << detail::escape_dot(options.graph_name) << "\" {\n";
    for (const auto& piece : cover.pieces) {
        oss << "  subgraph \"cluster_piece_" << piece.piece_id << "\" {\n";
        oss << "    label=\"Piece " << piece.piece_id << "\";\n";
        for (const auto cid : piece.component_ids) {
            std::ostringstream label_oss;
            label_oss << options.component_label(cid);
            if (options.include_component_vertices && cid < cover.condensation.components.size()) {
                label_oss << "\\n{";
                const auto& vs = cover.condensation.components[cid];
                for (std::size_t i = 0; i < vs.size(); ++i) {
                    if (i) label_oss << ", ";
                    label_oss << grafitt::to_string_fallback(vs[i]);
                }
                label_oss << "}";
            }
            const auto style = options.component_style(
                cid,
                cid < cover.condensation.components.size() ? cover.condensation.components[cid] : std::vector<Vertex>{}
            );
            oss << "    \"" << cid << "\" [label=\"" << detail::escape_dot(label_oss.str()) << "\"";
            if (!style.empty()) oss << ", " << style;
            oss << "];\n";
        }
        piece.dag_piece.iter_edges_e([&](const auto& e) {
            oss << "    \"" << e.src << "\" -> \"" << e.dst << "\";\n";
        });
        oss << "  }\n";
    }
    oss << "}\n";
    return oss.str();
}

/**
 * @brief Exports BFS-forest cover as Graph.js-style JSON payload.
 */
template<class Vertex, class EdgeLabel>
[[nodiscard]] std::string to_graphjs(
    const algo::bfs_forest_cover_result<Vertex, EdgeLabel>& cover
) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"directed\": true,\n";
    oss << "  \"pieces\": [\n";
    for (std::size_t i = 0; i < cover.pieces.size(); ++i) {
        const auto& piece = cover.pieces[i];
        oss << "    {\"id\":" << piece.piece_id << ",\"root\":\""
            << detail::escape_json(grafitt::to_string_fallback(piece.root)) << "\",\"nodes\":[";
        for (std::size_t j = 0; j < piece.vertices.size(); ++j) {
            if (j) oss << ",";
            oss << "\"" << detail::escape_json(grafitt::to_string_fallback(piece.vertices[j])) << "\"";
        }
        oss << "],\"edges\":[";
        for (std::size_t j = 0; j < piece.tree_edges.size(); ++j) {
            const auto& e = piece.tree_edges[j];
            if (j) oss << ",";
            oss << "{\"source\":\"" << detail::escape_json(grafitt::to_string_fallback(e.src))
                << "\",\"target\":\"" << detail::escape_json(grafitt::to_string_fallback(e.dst))
                << "\",\"label\":\"" << detail::escape_json(grafitt::to_string_fallback(e.label)) << "\"}";
        }
        oss << "]}";
        if (i + 1 < cover.pieces.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ],\n";
    oss << "  \"metrics\": {"
        << "\"piece_count\":" << cover.metrics.piece_count << ","
        << "\"covered_vertices\":" << cover.metrics.covered_vertices << ","
        << "\"overlap_vertices\":" << cover.metrics.overlap_vertices << ","
        << "\"max_piece_vertices\":" << cover.metrics.max_piece_vertices << ","
        << "\"total_witness_edges\":" << cover.metrics.total_witness_edges
        << "}\n";
    oss << "}\n";
    return oss.str();
}

} // namespace vizz

// ============================================================
// Equality saturation (e-graph scaffold)
// ============================================================

namespace eqsat {

using eclass_id = std::size_t;

struct term {
    std::string op;
    std::vector<term> children;
};

struct enode {
    std::string op;
    std::vector<eclass_id> children;
    friend bool operator==(const enode&, const enode&) = default;
};

struct enode_hash {
    std::size_t operator()(const enode& n) const noexcept {
        std::size_t h = std::hash<std::string>{}(n.op);
        for (const auto c : n.children) h ^= (c + 0x9e3779b9u + (h << 6) + (h >> 2));
        return h;
    }
};

struct eclass {
    eclass_id id { 0 };
    std::vector<enode> nodes;
};

struct rewrite_rule {
    std::string name;
    term lhs;
    term rhs;
};

struct saturation_options {
    std::size_t max_iterations = 20;
    std::size_t max_rule_applications_per_iteration = 0;
};

/**
 * @brief Rule scheduling policy for equality saturation.
 */
enum class rule_schedule_policy {
    stable,
    reverse,
    round_robin
};

/**
 * @brief One recorded merge event during saturation.
 */
struct merge_trace_entry {
    std::size_t iteration = 0;
    std::string rule_name;
    eclass_id lhs_class = 0;
    eclass_id rhs_class = 0;
    eclass_id merged_into = 0;
    std::unordered_map<std::string, eclass_id> bindings;
};

/**
 * @brief Trace bundle for one saturation run.
 */
struct saturation_trace {
    std::vector<merge_trace_entry> merges;
};

struct saturation_summary {
    std::size_t iterations = 0;
    std::size_t rule_applications = 0;
    std::size_t merges = 0;
    std::size_t eclasses = 0;
    std::size_t enodes = 0;
    bool reached_fixpoint = false;
    bool hit_application_limit = false;
    std::size_t trace_events = 0;
};

struct extraction_result {
    bool found = false;
    eclass_id root = 0;
    std::size_t cost = 0;
    term best;
    std::string cost_model = "unit_node_cost";
};

/**
 * @brief Tie-breaking policy when extraction costs are equal.
 */
enum class extraction_tie_break {
    stable,
    lexicographic_smallest,
    lexicographic_largest
};

/**
 * @brief One chosen enode decision in extraction explanation.
 */
struct extraction_decision {
    eclass_id eclass = 0;
    std::string op;
    std::vector<eclass_id> children;
    std::size_t node_cost = 0;
    std::size_t total_cost = 0;
};

/**
 * @brief Extraction explanation metadata.
 */
struct extraction_explanation {
    std::vector<extraction_decision> decisions;
};

/**
 * @brief Policy options for best-term extraction.
 */
struct extraction_options {
    std::function<std::size_t(std::string_view, std::size_t)> node_cost =
        [](std::string_view, std::size_t) { return static_cast<std::size_t>(1); };
    extraction_tie_break tie_break = extraction_tie_break::stable;
    bool capture_explanation = true;
    std::string cost_model_name = "unit_node_cost";
};

class egraph {
public:
    using binding_map = std::unordered_map<std::string, eclass_id>;

private:
    std::vector<eclass_id> parent_;
    std::vector<std::size_t> rank_;
    std::vector<eclass> classes_;
    std::unordered_map<enode, eclass_id, enode_hash> memo_;
    std::size_t merge_count_ = 0;

public:
    [[nodiscard]] std::size_t raw_class_count() const noexcept { return classes_.size(); }
    [[nodiscard]] std::size_t merge_count() const noexcept { return merge_count_; }

    [[nodiscard]] eclass_id make_class() {
        const eclass_id id = classes_.size();
        classes_.push_back(eclass{id, {}});
        parent_.push_back(id);
        rank_.push_back(0);
        return id;
    }

    [[nodiscard]] eclass_id find(eclass_id id) {
        if (parent_.at(id) == id) return id;
        parent_[id] = find(parent_[id]);
        return parent_[id];
    }

    [[nodiscard]] eclass_id find(eclass_id id) const {
        if (parent_.at(id) == id) return id;
        auto self = const_cast<egraph*>(this);
        return self->find(id);
    }

    [[nodiscard]] const eclass* class_of(eclass_id id) const {
        if (id >= classes_.size()) return nullptr;
        const auto root = find(id);
        return &classes_[root];
    }

    [[nodiscard]] std::vector<eclass_id> live_classes() const {
        std::vector<eclass_id> out;
        out.reserve(classes_.size());
        for (eclass_id i = 0; i < classes_.size(); ++i) {
            if (find(i) == i) out.push_back(i);
        }
        return out;
    }

    [[nodiscard]] eclass_id add_enode(enode node) {
        for (auto& child : node.children) child = find(child);
        if (auto it = memo_.find(node); it != memo_.end()) return find(it->second);
        const auto id = make_class();
        classes_[id].nodes.push_back(std::move(node));
        memo_[classes_[id].nodes.front()] = id;
        return id;
    }

    [[nodiscard]] eclass_id add_term(const term& t) {
        std::vector<eclass_id> child_ids;
        child_ids.reserve(t.children.size());
        for (const auto& c : t.children) child_ids.push_back(add_term(c));
        return add_enode(enode{t.op, std::move(child_ids)});
    }

    [[nodiscard]] bool merge(eclass_id a, eclass_id b) {
        a = find(a);
        b = find(b);
        if (a == b) return false;
        if (rank_[a] < rank_[b]) std::swap(a, b);
        parent_[b] = a;
        if (rank_[a] == rank_[b]) ++rank_[a];
        auto& dst = classes_[a].nodes;
        auto& src = classes_[b].nodes;
        dst.insert(dst.end(), src.begin(), src.end());
        src.clear();
        ++merge_count_;
        rebuild();
        return true;
    }

    void rebuild() {
        memo_.clear();
        for (auto cid : live_classes()) {
            auto& cls = classes_[cid];
            std::vector<enode> normalized;
            normalized.reserve(cls.nodes.size());
            for (auto n : cls.nodes) {
                for (auto& c : n.children) c = find(c);
                normalized.push_back(std::move(n));
            }
            cls.nodes = std::move(normalized);
            std::vector<enode> unique_nodes;
            for (const auto& n : cls.nodes) {
                if (std::find(unique_nodes.begin(), unique_nodes.end(), n) == unique_nodes.end()) {
                    unique_nodes.push_back(n);
                }
            }
            cls.nodes = std::move(unique_nodes);
            for (const auto& n : cls.nodes) memo_[n] = cid;
        }
    }

    [[nodiscard]] std::size_t enode_count() const {
        std::size_t total = 0;
        for (auto cid : live_classes()) total += classes_[cid].nodes.size();
        return total;
    }
};

[[nodiscard]] inline bool is_variable(std::string_view token) {
    return !token.empty() && token.front() == '$';
}

namespace detail {

inline bool match_term_at_class(
    const egraph& eg,
    eclass_id root,
    const term& pat,
    egraph::binding_map& bindings
) {
    root = eg.find(root);
    if (is_variable(pat.op)) {
        if (auto it = bindings.find(pat.op); it != bindings.end()) return eg.find(it->second) == root;
        bindings.emplace(pat.op, root);
        return true;
    }
    const auto* cls = eg.class_of(root);
    if (!cls) return false;
    for (const auto& node : cls->nodes) {
        if (node.op != pat.op || node.children.size() != pat.children.size()) continue;
        auto local = bindings;
        bool ok = true;
        for (std::size_t i = 0; i < node.children.size(); ++i) {
            if (!match_term_at_class(eg, node.children[i], pat.children[i], local)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            bindings = std::move(local);
            return true;
        }
    }
    return false;
}

inline eclass_id instantiate_rhs(
    egraph& eg,
    const term& rhs,
    const egraph::binding_map& bindings
) {
    if (is_variable(rhs.op)) {
        if (auto it = bindings.find(rhs.op); it != bindings.end()) return eg.find(it->second);
        return eg.add_term(rhs);
    }
    std::vector<eclass_id> kids;
    kids.reserve(rhs.children.size());
    for (const auto& c : rhs.children) kids.push_back(instantiate_rhs(eg, c, bindings));
    return eg.add_enode(enode{rhs.op, std::move(kids)});
}

inline std::optional<std::pair<std::size_t, term>> extract_best_rec(
    const egraph& eg,
    eclass_id root,
    std::unordered_map<eclass_id, std::pair<std::size_t, term>>& memo,
    std::unordered_set<eclass_id>& active
) {
    root = eg.find(root);
    if (auto it = memo.find(root); it != memo.end()) return it->second;
    if (active.contains(root)) return std::nullopt;
    active.insert(root);
    const auto* cls = eg.class_of(root);
    if (!cls || cls->nodes.empty()) {
        active.erase(root);
        return std::nullopt;
    }
    std::optional<std::pair<std::size_t, term>> best;
    for (const auto& node : cls->nodes) {
        std::size_t total = 1;
        term candidate{node.op, {}};
        bool ok = true;
        for (const auto child : node.children) {
            auto sub = extract_best_rec(eg, child, memo, active);
            if (!sub) { ok = false; break; }
            total += sub->first;
            candidate.children.push_back(sub->second);
        }
        if (!ok) continue;
        if (!best || total < best->first) best = std::pair<std::size_t, term>{total, std::move(candidate)};
    }
    active.erase(root);
    if (best) memo[root] = *best;
    return best;
}

inline std::string term_key(const term& t) {
    std::ostringstream oss;
    oss << t.op;
    if (!t.children.empty()) {
        oss << "(";
        for (std::size_t i = 0; i < t.children.size(); ++i) {
            if (i) oss << ",";
            oss << term_key(t.children[i]);
        }
        oss << ")";
    }
    return oss.str();
}

struct extraction_candidate {
    std::size_t total_cost = 0;
    std::size_t node_cost = 0;
    term tree;
    std::string key;
    std::string op;
    std::vector<eclass_id> children;
};

inline std::optional<extraction_candidate> extract_best_rec_policy(
    const egraph& eg,
    eclass_id root,
    const extraction_options& options,
    std::unordered_map<eclass_id, extraction_candidate>& memo,
    std::unordered_set<eclass_id>& active
) {
    root = eg.find(root);
    if (auto it = memo.find(root); it != memo.end()) return it->second;
    if (active.contains(root)) return std::nullopt;
    active.insert(root);

    const auto* cls = eg.class_of(root);
    if (!cls || cls->nodes.empty()) {
        active.erase(root);
        return std::nullopt;
    }

    std::optional<extraction_candidate> best;
    for (const auto& node : cls->nodes) {
        extraction_candidate cur;
        cur.op = node.op;
        cur.children = node.children;
        cur.node_cost = static_cast<std::size_t>(std::invoke(options.node_cost, std::string_view{node.op}, node.children.size()));
        cur.total_cost = cur.node_cost;
        cur.tree = term{node.op, {}};

        bool ok = true;
        for (const auto child : node.children) {
            auto sub = extract_best_rec_policy(eg, child, options, memo, active);
            if (!sub) {
                ok = false;
                break;
            }
            cur.total_cost += sub->total_cost;
            cur.tree.children.push_back(sub->tree);
        }
        if (!ok) continue;
        cur.key = term_key(cur.tree);

        bool take = false;
        if (!best) {
            take = true;
        } else if (cur.total_cost < best->total_cost) {
            take = true;
        } else if (cur.total_cost == best->total_cost) {
            if (options.tie_break == extraction_tie_break::lexicographic_smallest) {
                take = cur.key < best->key;
            } else if (options.tie_break == extraction_tie_break::lexicographic_largest) {
                take = cur.key > best->key;
            }
        }
        if (take) best = std::move(cur);
    }

    active.erase(root);
    if (best) memo[root] = *best;
    return best;
}

} // namespace detail

[[nodiscard]] inline saturation_summary saturate(
    egraph& eg,
    const std::vector<rewrite_rule>& rules,
    saturation_trace& trace,
    saturation_options options,
    rule_schedule_policy schedule
);

[[nodiscard]] inline saturation_summary saturate(
    egraph& eg,
    const std::vector<rewrite_rule>& rules,
    saturation_options options = {}
) {
    // Convenience overload that discards the trace by routing through the
    // trace-capturing form. This is not a stub: the saturation engine is
    // fully implemented in the overload below.
    saturation_trace discarded_trace;
    return saturate(eg, rules, discarded_trace, options, rule_schedule_policy::stable);
}

/**
 * @brief Saturates e-graph with scheduler and trace capture.
 */
[[nodiscard]] inline saturation_summary saturate(
    egraph& eg,
    const std::vector<rewrite_rule>& rules,
    saturation_trace& trace,
    saturation_options options = {},
    rule_schedule_policy schedule = rule_schedule_policy::stable
) {
    saturation_summary out;
    if (rules.empty()) {
        out.reached_fixpoint = true;
        out.eclasses = eg.live_classes().size();
        out.enodes = eg.enode_count();
        return out;
    }

    std::size_t rr_offset = 0;
    for (std::size_t iter = 0; iter < options.max_iterations; ++iter) {
        ++out.iterations;
        bool changed = false;
        std::size_t applied_this_iter = 0;
        const auto classes = eg.live_classes();
        std::vector<std::size_t> order(rules.size());
        std::iota(order.begin(), order.end(), std::size_t{0});
        if (schedule == rule_schedule_policy::reverse) {
            std::reverse(order.begin(), order.end());
        } else if (schedule == rule_schedule_policy::round_robin && !order.empty()) {
            rr_offset %= order.size();
            std::rotate(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(rr_offset), order.end());
            ++rr_offset;
        }

        for (const auto cid : classes) {
            for (const auto ridx : order) {
                const auto& rule = rules[ridx];
                if (options.max_rule_applications_per_iteration > 0 &&
                    applied_this_iter >= options.max_rule_applications_per_iteration) {
                    out.hit_application_limit = true;
                    break;
                }
                egraph::binding_map bind;
                if (!detail::match_term_at_class(eg, cid, rule.lhs, bind)) continue;
                const auto rhs_id = detail::instantiate_rhs(eg, rule.rhs, bind);
                const auto lhs_before = eg.find(cid);
                const auto rhs_before = eg.find(rhs_id);
                if (eg.merge(cid, rhs_id)) {
                    changed = true;
                    ++out.rule_applications;
                    ++applied_this_iter;
                    trace.merges.push_back(merge_trace_entry{
                        .iteration = iter,
                        .rule_name = rule.name,
                        .lhs_class = lhs_before,
                        .rhs_class = rhs_before,
                        .merged_into = eg.find(lhs_before),
                        .bindings = std::move(bind)
                    });
                    break;
                }
            }
            if (out.hit_application_limit) break;
        }
        if (!changed || out.hit_application_limit) {
            out.reached_fixpoint = !changed && !out.hit_application_limit;
            break;
        }
    }

    out.merges = eg.merge_count();
    out.eclasses = eg.live_classes().size();
    out.enodes = eg.enode_count();
    out.trace_events = trace.merges.size();
    return out;
}

[[nodiscard]] inline extraction_result extract_best(
    const egraph& eg,
    eclass_id root,
    extraction_options options
) {
    extraction_result out;
    out.root = eg.find(root);
    out.cost_model = options.cost_model_name;
    std::unordered_map<eclass_id, detail::extraction_candidate> memo;
    std::unordered_set<eclass_id> active;
    auto best = detail::extract_best_rec_policy(eg, out.root, options, memo, active);
    if (!best) return out;
    out.found = true;
    out.cost = best->total_cost;
    out.best = std::move(best->tree);
    return out;
}

/**
 * @brief Extracts a representative term with default unit-cost policy.
 */
[[nodiscard]] inline extraction_result extract_best(const egraph& eg, eclass_id root) {
    return extract_best(eg, root, extraction_options{});
}

/**
 * @brief Extracts best representative and returns explanation metadata.
 */
[[nodiscard]] inline std::pair<extraction_result, extraction_explanation> extract_best_with_explanation(
    const egraph& eg,
    eclass_id root,
    extraction_options options = {}
) {
    extraction_result out;
    extraction_explanation exp;
    out.root = eg.find(root);
    out.cost_model = options.cost_model_name;

    std::unordered_map<eclass_id, detail::extraction_candidate> memo;
    std::unordered_set<eclass_id> active;
    auto best = detail::extract_best_rec_policy(eg, out.root, options, memo, active);
    if (!best) return {std::move(out), std::move(exp)};

    out.found = true;
    out.cost = best->total_cost;
    out.best = best->tree;

    if (options.capture_explanation) {
        exp.decisions.reserve(memo.size());
        for (const auto& [cid, cand] : memo) {
            exp.decisions.push_back(extraction_decision{
                .eclass = cid,
                .op = cand.op,
                .children = cand.children,
                .node_cost = cand.node_cost,
                .total_cost = cand.total_cost
            });
        }
        std::sort(exp.decisions.begin(), exp.decisions.end(), [](const auto& a, const auto& b) {
            return a.eclass < b.eclass;
        });
    }
    return {std::move(out), std::move(exp)};
}

/**
 * @brief Returns all trace events touching one e-class id.
 */
[[nodiscard]] inline std::vector<merge_trace_entry> explain_class(
    const saturation_trace& trace,
    eclass_id cid
) {
    std::vector<merge_trace_entry> out;
    for (const auto& ev : trace.merges) {
        if (ev.lhs_class == cid || ev.rhs_class == cid || ev.merged_into == cid) out.push_back(ev);
    }
    return out;
}

/**
 * @brief Returns trace events produced by one rule name.
 */
[[nodiscard]] inline std::vector<merge_trace_entry> explain_rule(
    const saturation_trace& trace,
    std::string_view rule_name
) {
    std::vector<merge_trace_entry> out;
    for (const auto& ev : trace.merges) {
        if (ev.rule_name == rule_name) out.push_back(ev);
    }
    return out;
}

} // namespace eqsat

// ============================================================
// Queryfitt: AST and execution model
// ============================================================

namespace queryfitt {

enum class query_kind {
    pattern,
    traversal,
    path,
    reachability,
    aggregation
};

struct meta_block {
    std::optional<std::string> name;
    std::optional<std::string> desc;
    std::optional<std::string> graph;
};

struct predicate_expr {
    std::string text;
};

struct pattern_clause {
    std::string vertex_alias = "NODE";
    std::string edge_alias = "EDGE";
    std::size_t count = 0;
    std::string vertex_type;
    std::optional<predicate_expr> edge_predicate;
    std::optional<predicate_expr> where_predicate;
};

struct traversal_clause {
    std::string from;
    std::size_t depth = 1;
    std::optional<predicate_expr> edge_filter;
    std::optional<predicate_expr> vertex_filter;
};

struct path_clause {
    std::string from;
    std::string to;
    bool shortest = true;
    std::optional<predicate_expr> edge_filter;
};

struct reachability_clause {
    std::string from;
    std::string to;
    std::size_t max_depth = 0;
};

struct aggregation_clause {
    std::string op;     // count, sum, avg, min, max, degree, etc
    std::string target; // vertices, edges, paths, matches
    std::optional<predicate_expr> where;
    std::optional<std::string> by;
};

using body_clause = std::variant<
    pattern_clause,
    traversal_clause,
    path_clause,
    reachability_clause,
    aggregation_clause
>;

struct query {
    meta_block meta;
    std::string source;
    query_kind kind {};
    body_clause body;
};

template<class Vertex, class EdgeLabel>
struct match_result {
    using edge_type = edge<Vertex, EdgeLabel>;
    std::vector<Vertex> vertices;
    std::vector<edge_type> edges;
    std::map<std::string, std::string> metadata;
};

template<class Vertex>
struct path_result {
    std::vector<Vertex> path;
};

struct scalar_result {
    using value_type = std::variant<std::int64_t, double, std::string, bool>;
    value_type value;
};

struct grouped_scalar_result {
    std::map<std::string, scalar_result::value_type> groups;
};

template<class Vertex, class EdgeLabel>
using result = std::variant<
    std::vector<match_result<Vertex, EdgeLabel>>,
    std::vector<Vertex>,
    std::vector<path_result<Vertex>>,
    bool,
    scalar_result,
    grouped_scalar_result
>;

// ------------------------------------------------------------
// C++-native DSL
// Built on MetaTk DSLtk mixins when available; the query builders
// below work regardless.
// ------------------------------------------------------------

#if GRAFITT_HAS_DSLUTILS

struct QueryfittDSL
    : dsl::DSL<
          QueryfittDSL,
          dsl::Pipeline,
          dsl::Operators,
          dsl::PatternMatch,
          dsl::AST,
          dsl::Rewrite,
          dsl::ExprTemplates,
          dsl::CustomLiterals> {
    using self_type = QueryfittDSL;
};

#endif

struct vertex_ref {
    std::string name;
};

struct edge_ref {
    std::string name;
};

struct native_pattern_builder {
    pattern_clause clause;

    native_pattern_builder& alias_vertex(std::string a) {
        clause.vertex_alias = std::move(a);
        return *this;
    }

    native_pattern_builder& alias_edge(std::string a) {
        clause.edge_alias = std::move(a);
        return *this;
    }

    native_pattern_builder& select_n(std::size_t n, std::string vertex_type) {
        clause.count = n;
        clause.vertex_type = std::move(vertex_type);
        return *this;
    }

    native_pattern_builder& edge_if(std::string pred) {
        clause.edge_predicate = predicate_expr{std::move(pred)};
        return *this;
    }

    native_pattern_builder& where(std::string pred) {
        clause.where_predicate = predicate_expr{std::move(pred)};
        return *this;
    }

    [[nodiscard]] query into_query(std::string source = {}) const {
        query q;
        q.source = std::move(source);
        q.kind = query_kind::pattern;
        q.body = clause;
        return q;
    }
};

inline native_pattern_builder find_pattern() {
    return {};
}

inline query traversal_from(std::string from, std::size_t depth = 1) {
    query q;
    q.kind = query_kind::traversal;
    q.body = traversal_clause{std::move(from), depth, std::nullopt, std::nullopt};
    return q;
}

inline query shortest_path_between(std::string from, std::string to) {
    query q;
    q.kind = query_kind::path;
    q.body = path_clause{std::move(from), std::move(to), true, std::nullopt};
    return q;
}

inline query reachability_between(std::string from, std::string to, std::size_t max_depth = 0) {
    query q;
    q.kind = query_kind::reachability;
    q.body = reachability_clause{std::move(from), std::move(to), max_depth};
    return q;
}

inline query aggregate(std::string op, std::string target) {
    query q;
    q.kind = query_kind::aggregation;
    q.body = aggregation_clause{std::move(op), std::move(target), std::nullopt, std::nullopt};
    return q;
}

// Text parser boundary: MetaTk DSLtk PEG matching validates the overall
// statement structure before the line-oriented clause extraction below
// populates the query AST.
struct parse_output {
    query value;
    std::size_t consumed = 0;
};

[[nodiscard]] inline std::string trim_copy(std::string_view sv) {
    std::size_t b = 0;
    std::size_t e = sv.size();
    auto is_ws = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };
    while (b < e && is_ws(static_cast<unsigned char>(sv[b]))) ++b;
    while (e > b && is_ws(static_cast<unsigned char>(sv[e - 1]))) --e;
    return std::string{sv.substr(b, e - b)};
}

#if GRAFITT_HAS_DSLTK

namespace detail {

// Structural token grammar for Queryfitt text, per specs/Queryfitt.ebnf.
// Whitespace/comments are routed to @IGNORE; every other token category is
// recognized explicitly so malformed input fails with a precise offset.
//
// Note on the DSLtk pattern compiler: it does not support backslash escapes
// inside character classes ([\.\]] parses as ".", "]", "]"), and it does not
// understand \n/\r/\t escapes — the *source* bytes of the pattern literal are
// what matter, so literal control characters must be written as actual
// newlines/tabs. The rules below account for both: classes use only literal
// ranges/members, and the whitespace rule embeds a real newline/CR/tab.
[[nodiscard]] inline const dsl::PEGDefinition& queryfitt_peg_definition() {
    static const dsl::PEGDefinition def = [] {
        dsl::PEGDefinition d = dsl::create_peg_definition();
        // The whitespace/comment channel is @IGNORE: matched spans are
        // consumed but produce no token. The class lists space, tab, and
        // (via real bytes in the literal) CR and LF.
        d.add_rule<"[\t\n\r ]+">().channel = dsl::PEGIgnoreChannel;
        d.add_rule<"#[^\n]*">().channel = dsl::PEGIgnoreChannel; // line comment
        d.add_rule<"\"[^\"]*\"">();           // quoted string literal
        d.add_rule<"---">();                  // meta block fence
        d.add_rule<"[A-Za-z_][A-Za-z0-9_]*">(); // identifier / keyword
        d.add_rule<"[0-9]+">();               // unsigned integer
        // Punctuation and singletons Queryfitt may use. Each bracket member
        // is a literal character (no escapes needed): braces, parens,
        // angle/colon/equals/comma/dot/star/plus/slash/bang/question, the
        // literal '-', and the rest of the symbol set. ':' (used in
        // "Friend:" type prefixes) is a member here.
        d.add_rule<"[-{}()<>:=,.*+/!?]">();
        d.add_rule<"[\\[\\]|&@;'~^%$_]">();
        return d;
    }();
    return def;
}

// Returns true iff the whole input tokenizes cleanly under the PEG above.
[[nodiscard]] inline bool queryfitt_structure_valid(std::string_view text) {
    const auto result = queryfitt_peg_definition().parse(text);
    return result.ok();
}

} // namespace detail

#endif // GRAFITT_HAS_DSLTK

[[nodiscard]] inline std::optional<parse_output> parse_text(std::string_view text) {
    if (trim_copy(text).empty()) return std::nullopt;
#if GRAFITT_HAS_DSLTK
    if (!detail::queryfitt_structure_valid(text)) return std::nullopt;
#endif

    query q;
    q.source = std::string{text};
    std::string s = std::string{text};
    auto extract_quoted = [&](std::string_view line) -> std::optional<std::string> {
        auto first = line.find('"');
        if (first == std::string_view::npos) return std::nullopt;
        auto second = line.find('"', first + 1);
        if (second == std::string_view::npos || second <= first + 1) return std::nullopt;
        return std::string{line.substr(first + 1, second - first - 1)};
    };

    std::istringstream iss(s);
    std::string line;
    while (std::getline(iss, line)) {
        auto t = trim_copy(line);
        if (t.rfind(".name", 0) == 0) q.meta.name = extract_quoted(t);
        if (t.rfind(".desc", 0) == 0) q.meta.desc = extract_quoted(t);
        if (t.rfind(".graph", 0) == 0) q.meta.graph = extract_quoted(t);
    }

    auto kind_from = trim_copy(s);
    if (kind_from.rfind("path", 0) == 0 || kind_from.find("\npath") != std::string::npos) {
        q.kind = query_kind::path;
    } else if (kind_from.rfind("traverse", 0) == 0 || kind_from.find("\ntraverse") != std::string::npos) {
        q.kind = query_kind::traversal;
    } else if (kind_from.rfind("reachable", 0) == 0 || kind_from.find("\nreachable") != std::string::npos) {
        q.kind = query_kind::reachability;
    } else if (kind_from.rfind("aggregate", 0) == 0 || kind_from.find("\naggregate") != std::string::npos) {
        q.kind = query_kind::aggregation;
    } else {
        q.kind = query_kind::pattern;
    }

    auto body_start = s.find('{');
    auto body_end = s.rfind('}');
    std::string body = (body_start != std::string::npos && body_end != std::string::npos && body_end > body_start)
        ? s.substr(body_start + 1, body_end - body_start - 1)
        : "";

    if (q.kind == query_kind::path) {
        std::string from;
        std::string to;
        bool shortest = body.find("shortest") != std::string::npos;
        std::istringstream bss(body);
        while (std::getline(bss, line)) {
            auto t = trim_copy(line);
            if (t.rfind("from", 0) == 0) from = extract_quoted(t).value_or("");
            if (t.rfind("to", 0) == 0) to = extract_quoted(t).value_or("");
        }
        if (from.empty() || to.empty()) return std::nullopt;
        q.body = path_clause{std::move(from), std::move(to), shortest, std::nullopt};
    } else if (q.kind == query_kind::traversal) {
        std::string from;
        std::size_t depth = 1;
        std::istringstream bss(body);
        while (std::getline(bss, line)) {
            auto t = trim_copy(line);
            if (t.rfind("from", 0) == 0) from = extract_quoted(t).value_or("");
            if (t.rfind("depth", 0) == 0) depth = static_cast<std::size_t>(std::stoull(trim_copy(t.substr(5))));
        }
        if (from.empty()) return std::nullopt;
        q.body = traversal_clause{std::move(from), depth, std::nullopt, std::nullopt};
    } else if (q.kind == query_kind::reachability) {
        std::string from;
        std::string to;
        std::size_t depth = 0;
        std::istringstream bss(body);
        while (std::getline(bss, line)) {
            auto t = trim_copy(line);
            if (t.rfind("from", 0) == 0) from = extract_quoted(t).value_or("");
            if (t.rfind("to", 0) == 0) to = extract_quoted(t).value_or("");
            if (t.rfind("depth", 0) == 0) depth = static_cast<std::size_t>(std::stoull(trim_copy(t.substr(5))));
        }
        if (from.empty() || to.empty()) return std::nullopt;
        q.body = reachability_clause{std::move(from), std::move(to), depth};
    } else if (q.kind == query_kind::aggregation) {
        std::istringstream bss(body);
        std::string op;
        std::string target;
        std::optional<predicate_expr> where;
        std::optional<std::string> by;
        bool got_head = false;
        while (std::getline(bss, line)) {
            auto t = trim_copy(line);
            if (t.empty()) continue;
            if (!got_head) {
                std::istringstream lss(t);
                lss >> op >> target;
                got_head = true;
                continue;
            }
            if (t.rfind("where", 0) == 0) {
                auto rhs = trim_copy(std::string_view{t}.substr(5));
                if (!rhs.empty()) where = predicate_expr{std::move(rhs)};
                continue;
            }
            if (t.rfind("by", 0) == 0) {
                auto rhs = trim_copy(std::string_view{t}.substr(2));
                if (!rhs.empty()) by = std::move(rhs);
                continue;
            }
        }
        if (op.empty() || target.empty()) return std::nullopt;
        q.body = aggregation_clause{std::move(op), std::move(target), std::move(where), std::move(by)};
    } else {
        pattern_clause p;
        std::istringstream bss(body);
        while (std::getline(bss, line)) {
            auto t = trim_copy(line);
            if (t.rfind("vertices", 0) == 0) {
                std::istringstream lss(t);
                std::string kw;
                std::string type_with_colon;
                lss >> kw >> p.count >> type_with_colon;
                if (!type_with_colon.empty() && type_with_colon[0] == ':') p.vertex_type = type_with_colon.substr(1);
            } else if (t.rfind("edge", 0) == 0) {
                p.edge_predicate = predicate_expr{t};
            } else if (t.rfind("where", 0) == 0) {
                p.where_predicate = predicate_expr{t};
            }
        }
        q.body = std::move(p);
    }

    return parse_output{std::move(q), text.size()};
}

template<class Graph, class LabelOf, class VertexPred, class EdgePred>
[[nodiscard]] inline result<typename Graph::vertex_type, typename Graph::edge_label_type>
execute(const Graph& g, const query& q, LabelOf&& label_of, VertexPred&& vpred, EdgePred&& epred) {
    using V = typename Graph::vertex_type;
    using E = typename Graph::edge_label_type;
    if (q.kind == query_kind::traversal) {
        const auto& c = std::get<traversal_clause>(q.body);
        auto src = algo::find_vertex_by_label(g, c.from, std::forward<LabelOf>(label_of));
        if (!src) return std::vector<V>{};
        auto order = algo::bfs_order(g, *src);
        if (c.depth < order.size()) order.resize(c.depth + 1);
        return order;
    }
    if (q.kind == query_kind::path) {
        const auto& c = std::get<path_clause>(q.body);
        auto src = algo::find_vertex_by_label(g, c.from, std::forward<LabelOf>(label_of));
        auto dst = algo::find_vertex_by_label(g, c.to, std::forward<LabelOf>(label_of));
        if (!src || !dst) return std::vector<path_result<V>>{};
        auto sp = algo::shortest_path(g, *src, *dst);
        if (!sp) return std::vector<path_result<V>>{};
        return std::vector<path_result<V>>{path_result<V>{*sp}};
    }
    if (q.kind == query_kind::reachability) {
        const auto& c = std::get<reachability_clause>(q.body);
        auto src = algo::find_vertex_by_label(g, c.from, std::forward<LabelOf>(label_of));
        auto dst = algo::find_vertex_by_label(g, c.to, std::forward<LabelOf>(label_of));
        if (!src || !dst) return false;
        if (c.max_depth == 0) return algo::reachable(g, *src, *dst);
        if (*src == *dst) return true;
        std::queue<std::pair<V, std::size_t>> qv;
        std::unordered_set<V> seen;
        qv.push({*src, 0});
        seen.insert(*src);
        while (!qv.empty()) {
            auto [cur, depth] = qv.front();
            qv.pop();
            if (depth >= c.max_depth) continue;
            for (const auto& n : g.succ(cur)) {
                if (n == *dst) return true;
                if (seen.insert(n).second) qv.push({n, depth + 1});
            }
        }
        return false;
    }
    if (q.kind == query_kind::aggregation) {
        const auto& c = std::get<aggregation_clause>(q.body);
        if (c.by && *c.by != "all" && *c.by != "vertex_type") {
            throw query_error("unsupported aggregation by");
        }
        const bool by_vertex_type = c.by && *c.by == "vertex_type";
        auto vertex_type_of = [](const std::string& label) -> std::string {
            auto t = queryfitt::trim_copy(label);
            if (t.empty()) return "unknown";
            const auto colon = t.find(':');
            if (colon == std::string::npos) return "default";
            if (colon == 0) {
                auto rhs = queryfitt::trim_copy(std::string_view{t}.substr(1));
                return rhs.empty() ? std::string("unknown") : rhs;
            }
            auto lhs = queryfitt::trim_copy(std::string_view{t}.substr(0, colon));
            return lhs.empty() ? std::string("unknown") : lhs;
        };
        auto match_where = [&](const std::string& value) -> bool {
            if (!c.where) return true;
            const auto& expr = c.where->text;
            if (expr == "*" || expr == "all" || expr == "true") return true;
            if (auto pos = expr.find("contains "); pos == 0) {
                auto needle = queryfitt::trim_copy(std::string_view{expr}.substr(9));
                return value.find(needle) != std::string::npos;
            }
            if (auto pos = expr.find("== "); pos == 0) {
                auto rhs = queryfitt::trim_copy(std::string_view{expr}.substr(3));
                return value == rhs;
            }
            return value == expr;
        };

        if (c.op == "count" && c.target == "vertices") {
            if (!c.where && !by_vertex_type) return scalar_result{static_cast<std::int64_t>(g.nb_vertex())};
            if (by_vertex_type) {
                std::map<std::string, scalar_result::value_type> groups;
                g.iter_vertex([&](const auto& v) {
                    const auto label = std::invoke(label_of, v);
                    if (!match_where(label)) return;
                    auto key = vertex_type_of(label);
                    auto it = groups.find(key);
                    if (it == groups.end()) {
                        groups.emplace(std::move(key), static_cast<std::int64_t>(1));
                        return;
                    }
                    auto* n = std::get_if<std::int64_t>(&it->second);
                    if (!n) throw query_error("internal aggregation type error");
                    ++(*n);
                });
                return grouped_scalar_result{std::move(groups)};
            }
            std::int64_t count = 0;
            g.iter_vertex([&](const auto& v) {
                const auto label = std::invoke(label_of, v);
                if (match_where(label)) ++count;
            });
            return scalar_result{count};
        }
        if (c.op == "count" && c.target == "edges") {
            if (by_vertex_type) throw query_error("by vertex_type is unsupported for count edges");
            if (!c.where) return scalar_result{static_cast<std::int64_t>(g.nb_edges())};
            std::int64_t count = 0;
            g.iter_edges_e([&](const auto& e) {
                if (match_where(to_string_fallback(e.label))) ++count;
            });
            return scalar_result{count};
        }
        if (c.op == "degree") {
            auto v = algo::find_vertex_by_label(g, c.target, std::forward<LabelOf>(label_of));
            if (!v) throw query_error("degree target vertex not found");
            const auto label = std::invoke(label_of, *v);
            if (c.where && !match_where(label)) {
                if (by_vertex_type) return grouped_scalar_result{};
                return scalar_result{static_cast<std::int64_t>(0)};
            }
            const auto deg = static_cast<std::int64_t>(algo::degree(g, *v));
            if (by_vertex_type) {
                std::map<std::string, scalar_result::value_type> groups;
                groups.emplace(vertex_type_of(label), deg);
                return grouped_scalar_result{std::move(groups)};
            }
            return scalar_result{deg};
        }
        if (c.target == "degree") {
            if (by_vertex_type) {
                std::unordered_map<std::string, std::vector<std::size_t>> grouped_degs;
                g.iter_vertex([&](const auto& v) {
                    const auto label = std::invoke(label_of, v);
                    if (!match_where(label)) return;
                    grouped_degs[vertex_type_of(label)].push_back(algo::degree(g, v));
                });
                std::map<std::string, scalar_result::value_type> out;
                for (auto& [key, degs] : grouped_degs) {
                    if (degs.empty()) continue;
                    if (c.op == "sum") {
                        std::int64_t sum = 0;
                        for (auto d : degs) sum += static_cast<std::int64_t>(d);
                        out.emplace(key, sum);
                        continue;
                    }
                    if (c.op == "avg") {
                        double sum = 0.0;
                        for (auto d : degs) sum += static_cast<double>(d);
                        out.emplace(key, sum / static_cast<double>(degs.size()));
                        continue;
                    }
                    if (c.op == "min") {
                        out.emplace(key, static_cast<std::int64_t>(*std::min_element(degs.begin(), degs.end())));
                        continue;
                    }
                    if (c.op == "max") {
                        out.emplace(key, static_cast<std::int64_t>(*std::max_element(degs.begin(), degs.end())));
                        continue;
                    }
                    throw query_error("unsupported aggregation");
                }
                return grouped_scalar_result{std::move(out)};
            }
            std::vector<std::size_t> degs;
            g.iter_vertex([&](const auto& v) {
                const auto label = std::invoke(label_of, v);
                if (match_where(label)) degs.push_back(algo::degree(g, v));
            });
            if (degs.empty()) return scalar_result{static_cast<std::int64_t>(0)};
            if (c.op == "sum") {
                std::int64_t sum = 0;
                for (auto d : degs) sum += static_cast<std::int64_t>(d);
                return scalar_result{sum};
            }
            if (c.op == "avg") {
                double sum = 0.0;
                for (auto d : degs) sum += static_cast<double>(d);
                return scalar_result{sum / static_cast<double>(degs.size())};
            }
            if (c.op == "min") {
                return scalar_result{static_cast<std::int64_t>(*std::min_element(degs.begin(), degs.end()))};
            }
            if (c.op == "max") {
                return scalar_result{static_cast<std::int64_t>(*std::max_element(degs.begin(), degs.end()))};
            }
        }
        throw query_error("unsupported aggregation");
    }
    std::vector<match_result<V, E>> out;
    g.iter_edges_e([&](const auto& e) {
        if (std::invoke(vpred, e.src) && std::invoke(vpred, e.dst) && std::invoke(epred, e, e.label)) {
            match_result<V, E> m;
            m.vertices.push_back(e.src);
            m.vertices.push_back(e.dst);
            m.edges.push_back(e);
            out.push_back(std::move(m));
        }
    });
    return out;
}

} // namespace queryfitt

// ============================================================
// Rewriting
// ============================================================

namespace rewrite {

/**
 * @brief Rewrite application strategy over discovered match witnesses.
 */
enum class matched_rewrite_strategy {
    first,
    all,
    bounded,
    until_fixpoint
};

/**
 * @brief Result object for rewrite flows driven by subgraph matching.
 *
 * @tparam PatternVertex Pattern-graph vertex type.
 * @tparam TargetVertex Target-graph vertex type.
 * @tparam EdgeLabel Edge-label type.
 */
template<class PatternVertex, class TargetVertex, class EdgeLabel>
struct matched_rewrite_result {
    bool applied = false;
    algo::subgraph_match_status match_status { algo::subgraph_match_status::no_match };
    std::size_t matched_witness_index = 0;
    std::vector<algo::subgraph_match_witness<PatternVertex, TargetVertex, EdgeLabel>> witnesses;
    std::map<std::string, std::string> bindings;

    /**
     * @brief Returns true iff at least one match witness was available.
     */
    [[nodiscard]] bool has_match() const noexcept {
        return !witnesses.empty();
    }
};

/**
 * @brief Options for multi-match rewrite scheduling.
 */
struct matched_rewrite_run_options {
    matched_rewrite_strategy strategy { matched_rewrite_strategy::first };
    std::size_t max_applications = 0;
    std::size_t max_rounds = 0;
};

/**
 * @brief Summary diagnostics for strategy-driven match rewrites.
 */
struct matched_rewrite_stats {
    std::size_t rounds = 0;
    std::size_t applications = 0;
    std::size_t matches_observed = 0;
    bool hit_application_limit = false;
    bool hit_round_limit = false;
};

/**
 * @brief Rich result for strategy-driven subgraph rewrite runs.
 */
template<class PatternVertex, class TargetVertex, class EdgeLabel>
struct matched_rewrite_run_result {
    bool changed = false;
    algo::subgraph_match_status final_match_status { algo::subgraph_match_status::no_match };
    std::vector<matched_rewrite_result<PatternVertex, TargetVertex, EdgeLabel>> applications;
    matched_rewrite_stats stats;
};

/**
 * @brief Converts one algorithm-level witness to Queryfitt-style match shape.
 */
template<class PatternVertex, class TargetVertex, class EdgeLabel>
[[nodiscard]] queryfitt::match_result<TargetVertex, EdgeLabel>
to_query_match_result(const algo::subgraph_match_witness<PatternVertex, TargetVertex, EdgeLabel>& witness) {
    queryfitt::match_result<TargetVertex, EdgeLabel> out;
    out.vertices.reserve(witness.vertex_bindings.size());
    for (const auto& [_, tv] : witness.vertex_bindings) out.vertices.push_back(tv);
    std::sort(out.vertices.begin(), out.vertices.end(), [](const auto& a, const auto& b) {
        return grafitt::to_string_fallback(a) < grafitt::to_string_fallback(b);
    });
    out.vertices.erase(std::unique(out.vertices.begin(), out.vertices.end()), out.vertices.end());

    out.edges.reserve(witness.edge_witnesses.size());
    for (const auto& ew : witness.edge_witnesses) out.edges.push_back(ew.target_edge);
    out.metadata = witness.to_named_bindings(
        [](const PatternVertex& pv) { return grafitt::to_string_fallback(pv); },
        [](const TargetVertex& tv) { return grafitt::to_string_fallback(tv); }
    );
    return out;
}

/**
 * @brief Converts all witnesses to Queryfitt-style match objects.
 */
template<class PatternVertex, class TargetVertex, class EdgeLabel>
[[nodiscard]] std::vector<queryfitt::match_result<TargetVertex, EdgeLabel>>
to_query_match_results(
    const std::vector<algo::subgraph_match_witness<PatternVertex, TargetVertex, EdgeLabel>>& witnesses
) {
    std::vector<queryfitt::match_result<TargetVertex, EdgeLabel>> out;
    out.reserve(witnesses.size());
    for (const auto& w : witnesses) out.push_back(to_query_match_result(w));
    return out;
}

template<class PatternVertex, class TargetVertex, class EdgeLabel>
[[nodiscard]] std::string witness_sort_key(
    const algo::subgraph_match_witness<PatternVertex, TargetVertex, EdgeLabel>& witness
) {
    std::vector<std::pair<std::string, std::string>> pairs;
    pairs.reserve(witness.vertex_bindings.size());
    for (const auto& [p, t] : witness.vertex_bindings) {
        pairs.emplace_back(grafitt::to_string_fallback(p), grafitt::to_string_fallback(t));
    }
    std::sort(pairs.begin(), pairs.end());
    std::ostringstream oss;
    for (const auto& [k, v] : pairs) oss << k << "->" << v << ";";
    return oss.str();
}

struct rule {
    std::string name;
    queryfitt::query match;
    std::string replacement;
};

/**
 * @brief Converts a textual rewrite edge form (`lhs->rhs`) into eqsat rule.
 *
 * Variables can be encoded with leading `$`, for example:
 * `"$x->f($x)"` represented as replacement string `"$x->f"`.
 * This adapter remains intentionally lightweight and primarily targets
 * unary symbol rewrites for early eqsat interop.
 */
[[nodiscard]] inline std::optional<eqsat::rewrite_rule>
to_eqsat_rule(const rule& r) {
    if (r.replacement.empty()) return std::nullopt;
    const auto arrow = r.replacement.find("->");
    if (arrow == std::string::npos) return std::nullopt;
    auto lhs = queryfitt::trim_copy(std::string_view{r.replacement}.substr(0, arrow));
    auto rhs = queryfitt::trim_copy(std::string_view{r.replacement}.substr(arrow + 2));
    if (lhs.empty() || rhs.empty()) return std::nullopt;
    return eqsat::rewrite_rule{
        .name = r.name,
        .lhs = eqsat::term{lhs, {}},
        .rhs = eqsat::term{rhs, {}}
    };
}

/**
 * @brief Builds an e-graph, feeds rules, and runs one saturation workflow.
 */
[[nodiscard]] inline std::pair<eqsat::egraph, eqsat::saturation_summary>
run_eqsat(
    const eqsat::term& seed,
    const std::vector<eqsat::rewrite_rule>& rules,
    eqsat::saturation_options options = {}
) {
    eqsat::egraph eg;
    const auto root = eg.add_term(seed);
    (void)root;
    auto summary = eqsat::saturate(eg, rules, options);
    return {std::move(eg), std::move(summary)};
}

/**
 * @brief Convenience overload from Grafitt rewrite rules.
 */
[[nodiscard]] inline std::pair<eqsat::egraph, eqsat::saturation_summary>
run_eqsat_from_rules(
    const eqsat::term& seed,
    const std::vector<rule>& rules,
    eqsat::saturation_options options = {}
) {
    std::vector<eqsat::rewrite_rule> converted;
    converted.reserve(rules.size());
    for (const auto& r : rules) {
        if (auto eqr = to_eqsat_rule(r)) converted.push_back(std::move(*eqr));
    }
    return run_eqsat(seed, converted, options);
}

template<class Graph>
[[nodiscard]] inline Graph add_vertex(Graph g, const typename Graph::vertex_type& v) {
    if constexpr (std::is_void_v<decltype(g.add_vertex(v))>) {
        g.add_vertex(v);
        return g;
    } else {
        return g.add_vertex(v);
    }
}

template<class Graph>
[[nodiscard]] inline Graph add_edge(
    Graph g,
    const typename Graph::vertex_type& src,
    const typename Graph::vertex_type& dst,
    const typename Graph::edge_label_type& label = {}
) {
    if constexpr (std::is_void_v<decltype(g.add_edge(src, dst, label))>) {
        g.add_edge(src, dst, label);
        return g;
    } else {
        return g.add_edge(src, dst, label);
    }
}

template<class Graph>
[[nodiscard]] inline Graph remove_edge_e(Graph g, const typename Graph::edge_type& e) {
    if constexpr (std::is_void_v<decltype(g.remove_edge_e(e))>) {
        g.remove_edge_e(e);
        return g;
    } else {
        return g.remove_edge_e(e);
    }
}

template<class Graph>
[[nodiscard]] inline Graph apply_once(const Graph& g, const rule& r) {
    Graph out = g;
    if (r.replacement.empty()) return out;
    const auto arrow = r.replacement.find("->");
    if (arrow == std::string::npos) return out;
    auto lhs = queryfitt::trim_copy(std::string_view{r.replacement}.substr(0, arrow));
    auto rhs = queryfitt::trim_copy(std::string_view{r.replacement}.substr(arrow + 2));
    if (lhs.empty() || rhs.empty()) return out;
    if constexpr (std::is_same_v<typename Graph::vertex_type, std::string>) {
        auto matched = queryfitt::execute(
            out,
            r.match,
            [](const std::string& v) { return v; },
            [](const std::string&) { return true; },
            [](const auto&, const auto&) { return true; }
        );
        const bool should_apply = std::visit([](const auto& x) -> bool {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, bool>) return x;
            else if constexpr (std::is_same_v<T, queryfitt::scalar_result>) return true;
            else if constexpr (requires(const T& v) { v.empty(); }) return !x.empty();
            else return true;
        }, matched);
        if (!should_apply) return out;
        if (!out.mem_vertex(lhs)) return out;
        out = add_vertex(std::move(out), rhs);
        auto succ_edges = out.succ_e(lhs);
        for (const auto& e : succ_edges) out = remove_edge_e(std::move(out), e);
        out = add_edge(std::move(out), lhs, rhs);
    }
    return out;
}

/**
 * @brief Applies a transform callback using the first subgraph witness, if any.
 *
 * The callback may either:
 * - mutate the graph in place and return `void`, or
 * - return a new graph value.
 *
 * @tparam Graph Target graph type.
 * @tparam PatternGraph Pattern graph type.
 * @tparam Transform Callable of shape `f(Graph&, witness)` or
 *         `f(Graph, witness) -> Graph`.
 */
template<class Graph, class PatternGraph, class Transform>
[[nodiscard]] matched_rewrite_result<
    typename PatternGraph::vertex_type,
    typename Graph::vertex_type,
    typename Graph::edge_label_type
>
apply_first_subgraph_match(
    Graph& g,
    const PatternGraph& pattern,
    Transform&& transform,
    algo::subgraph_match_options<
        typename PatternGraph::vertex_type,
        typename Graph::vertex_type,
        typename Graph::edge_label_type
    > options = {}
) requires (
    std::same_as<typename Graph::edge_label_type, typename PatternGraph::edge_label_type>
) {
    using PV = typename PatternGraph::vertex_type;
    using TV = typename Graph::vertex_type;
    using EL = typename Graph::edge_label_type;

    auto matches = algo::subgraph_match(pattern, g, std::move(options));
    matched_rewrite_result<PV, TV, EL> out;
    out.match_status = matches.status;
    out.witnesses = matches.witnesses;
    if (matches.witnesses.empty()) return out;

    auto& witness = out.witnesses.front();
    out.bindings = witness.to_named_bindings(
        [](const PV& pv) { return grafitt::to_string_fallback(pv); },
        [](const TV& tv) { return grafitt::to_string_fallback(tv); }
    );
    out.matched_witness_index = 0;

    if constexpr (std::is_void_v<std::invoke_result_t<Transform, Graph&, const algo::subgraph_match_witness<PV, TV, EL>&>>) {
        std::invoke(std::forward<Transform>(transform), g, witness);
    } else {
        g = std::invoke(std::forward<Transform>(transform), g, witness);
    }
    out.applied = true;
    return out;
}

/**
 * @brief Value-returning convenience overload.
 */
template<class Graph, class PatternGraph, class Transform>
[[nodiscard]] std::pair<Graph, matched_rewrite_result<
    typename PatternGraph::vertex_type,
    typename Graph::vertex_type,
    typename Graph::edge_label_type
>>
apply_first_subgraph_match_copy(
    const Graph& g,
    const PatternGraph& pattern,
    Transform&& transform,
    algo::subgraph_match_options<
        typename PatternGraph::vertex_type,
        typename Graph::vertex_type,
        typename Graph::edge_label_type
    > options = {}
) requires (
    std::same_as<typename Graph::edge_label_type, typename PatternGraph::edge_label_type>
) {
    Graph out_graph = g;
    auto out = apply_first_subgraph_match(out_graph, pattern, std::forward<Transform>(transform), std::move(options));
    return { std::move(out_graph), std::move(out) };
}

/**
 * @brief Applies a transform according to strategy over subgraph matches.
 *
 * Deterministically orders match witnesses by canonicalized binding keys before
 * each strategy round, then applies selected witnesses.
 */
template<class Graph, class PatternGraph, class Transform>
[[nodiscard]] matched_rewrite_run_result<
    typename PatternGraph::vertex_type,
    typename Graph::vertex_type,
    typename Graph::edge_label_type
>
apply_subgraph_matches(
    Graph& g,
    const PatternGraph& pattern,
    Transform&& transform,
    algo::subgraph_match_options<
        typename PatternGraph::vertex_type,
        typename Graph::vertex_type,
        typename Graph::edge_label_type
    > match_options = {},
    matched_rewrite_run_options run_options = {}
) requires (
    std::same_as<typename Graph::edge_label_type, typename PatternGraph::edge_label_type>
) {
    using PV = typename PatternGraph::vertex_type;
    using TV = typename Graph::vertex_type;
    using EL = typename Graph::edge_label_type;

    matched_rewrite_run_result<PV, TV, EL> out;
    const auto app_limit = run_options.max_applications;
    const auto round_limit = run_options.max_rounds;

    auto apply_one = [&](const algo::subgraph_match_witness<PV, TV, EL>& witness) {
        matched_rewrite_result<PV, TV, EL> app;
        app.applied = true;
        app.match_status = algo::subgraph_match_status::match_found;
        app.matched_witness_index = 0;
        app.witnesses.push_back(witness);
        app.bindings = witness.to_named_bindings(
            [](const PV& pv) { return grafitt::to_string_fallback(pv); },
            [](const TV& tv) { return grafitt::to_string_fallback(tv); }
        );

        if constexpr (std::is_void_v<std::invoke_result_t<Transform, Graph&, const algo::subgraph_match_witness<PV, TV, EL>&>>) {
            std::invoke(transform, g, witness);
        } else {
            g = std::invoke(transform, g, witness);
        }
        out.applications.push_back(std::move(app));
        out.changed = true;
        ++out.stats.applications;
    };

    std::size_t round = 0;
    while (true) {
        if (round_limit > 0 && round >= round_limit) {
            out.stats.hit_round_limit = true;
            break;
        }
        ++round;
        ++out.stats.rounds;

        auto current_match_options = match_options;
        if (run_options.strategy == matched_rewrite_strategy::first) current_match_options.max_witnesses = 1;
        if (run_options.strategy == matched_rewrite_strategy::bounded && app_limit > 0) {
            const std::size_t remaining = app_limit - std::min(app_limit, out.stats.applications);
            current_match_options.max_witnesses = remaining;
        } else if (run_options.strategy == matched_rewrite_strategy::all || run_options.strategy == matched_rewrite_strategy::until_fixpoint) {
            if (current_match_options.max_witnesses == 0) current_match_options.max_witnesses = g.nb_vertex() == 0 ? 1 : g.nb_vertex();
        }

        auto matched = algo::subgraph_match(pattern, g, current_match_options);
        out.final_match_status = matched.status;
        out.stats.matches_observed += matched.witnesses.size();
        if (matched.witnesses.empty()) break;

        std::sort(matched.witnesses.begin(), matched.witnesses.end(), [](const auto& a, const auto& b) {
            return witness_sort_key(a) < witness_sort_key(b);
        });

        if (run_options.strategy == matched_rewrite_strategy::first) {
            apply_one(matched.witnesses.front());
            break;
        }

        if (run_options.strategy == matched_rewrite_strategy::all ||
            run_options.strategy == matched_rewrite_strategy::bounded ||
            run_options.strategy == matched_rewrite_strategy::until_fixpoint) {
            for (const auto& w : matched.witnesses) {
                // Stop before exceeding the cap when more witnesses remain.
                if (app_limit > 0 && out.stats.applications >= app_limit) {
                    out.stats.hit_application_limit = true;
                    break;
                }
                apply_one(w);
            }
            // The cap may have been reached exactly by the last available
            // witness; the per-witness guard above only fires when a witness
            // is left unapplied, so re-check here to flag the exact-cap case.
            if (app_limit > 0 && out.stats.applications >= app_limit) {
                out.stats.hit_application_limit = true;
            }
        }

        if (out.stats.hit_application_limit) break;
        if (run_options.strategy != matched_rewrite_strategy::until_fixpoint) break;
    }
    return out;
}

/**
 * @brief Value-returning strategy runner convenience overload.
 */
template<class Graph, class PatternGraph, class Transform>
[[nodiscard]] std::pair<Graph, matched_rewrite_run_result<
    typename PatternGraph::vertex_type,
    typename Graph::vertex_type,
    typename Graph::edge_label_type
>>
apply_subgraph_matches_copy(
    const Graph& g,
    const PatternGraph& pattern,
    Transform&& transform,
    algo::subgraph_match_options<
        typename PatternGraph::vertex_type,
        typename Graph::vertex_type,
        typename Graph::edge_label_type
    > match_options = {},
    matched_rewrite_run_options run_options = {}
) requires (
    std::same_as<typename Graph::edge_label_type, typename PatternGraph::edge_label_type>
) {
    Graph out_graph = g;
    auto out = apply_subgraph_matches(out_graph, pattern, std::forward<Transform>(transform), std::move(match_options), run_options);
    return { std::move(out_graph), std::move(out) };
}

#if GRAFITT_HAS_DSLUTILS

/**
 * @brief Runs subgraph matching with lightweight DSL tag constraints.
 *
 * Integrates `dsl::pattern<S>::matches(...)` with Grafitt's matching options
 * without introducing a separate pattern language.
 *
 * @tparam PatternTagPattern Compile-time DSL pattern for pattern-vertex tags.
 * @tparam TargetTagPattern Compile-time DSL pattern for target-vertex tags.
 */
template<dsl::FixedString PatternTagPattern, dsl::FixedString TargetTagPattern, class PatternGraph, class TargetGraph, class PatternTagOf, class TargetTagOf>
[[nodiscard]] auto subgraph_match_with_dsl_tags(
    const PatternGraph& pattern,
    const TargetGraph& target,
    PatternTagOf&& pattern_tag_of,
    TargetTagOf&& target_tag_of,
    algo::subgraph_match_options<
        typename PatternGraph::vertex_type,
        typename TargetGraph::vertex_type,
        typename PatternGraph::edge_label_type
    > options = {}
) requires (
    std::same_as<typename PatternGraph::edge_label_type, typename TargetGraph::edge_label_type>
) {
    using PV = typename PatternGraph::vertex_type;
    using TV = typename TargetGraph::vertex_type;
    const auto prior = options.vertex_compatible;
    options.vertex_compatible = [prior, ptag = std::forward<PatternTagOf>(pattern_tag_of), ttag = std::forward<TargetTagOf>(target_tag_of)](const PV& pv, const TV& tv) mutable {
        if (prior && !std::invoke(prior, pv, tv)) return false;
        const auto ptxt = grafitt::to_string_fallback(std::invoke(ptag, pv));
        const auto ttxt = grafitt::to_string_fallback(std::invoke(ttag, tv));
        return dsl::pattern<PatternTagPattern>::matches(ptxt) &&
               dsl::pattern<TargetTagPattern>::matches(ttxt);
    };
    return algo::subgraph_match(pattern, target, std::move(options));
}

/**
 * @brief Applies first tagged subgraph witness and runs transform callback.
 *
 * This combines DSL tag filtering and rewrite execution in one helper.
 */
template<dsl::FixedString PatternTagPattern, dsl::FixedString TargetTagPattern, class Graph, class PatternGraph, class Transform, class PatternTagOf, class TargetTagOf>
[[nodiscard]] matched_rewrite_result<
    typename PatternGraph::vertex_type,
    typename Graph::vertex_type,
    typename Graph::edge_label_type
>
apply_first_subgraph_match_with_dsl_tags(
    Graph& g,
    const PatternGraph& pattern,
    PatternTagOf&& pattern_tag_of,
    TargetTagOf&& target_tag_of,
    Transform&& transform,
    algo::subgraph_match_options<
        typename PatternGraph::vertex_type,
        typename Graph::vertex_type,
        typename Graph::edge_label_type
    > options = {}
) requires (
    std::same_as<typename Graph::edge_label_type, typename PatternGraph::edge_label_type>
) {
    using PV = typename PatternGraph::vertex_type;
    using TV = typename Graph::vertex_type;
    using EL = typename Graph::edge_label_type;

    auto matches = subgraph_match_with_dsl_tags<PatternTagPattern, TargetTagPattern>(
        pattern,
        g,
        std::forward<PatternTagOf>(pattern_tag_of),
        std::forward<TargetTagOf>(target_tag_of),
        std::move(options)
    );

    matched_rewrite_result<PV, TV, EL> out;
    out.match_status = matches.status;
    out.witnesses = matches.witnesses;
    if (matches.witnesses.empty()) return out;

    auto& witness = out.witnesses.front();
    out.bindings = witness.to_named_bindings(
        [](const PV& pv) { return grafitt::to_string_fallback(pv); },
        [](const TV& tv) { return grafitt::to_string_fallback(tv); }
    );

    if constexpr (std::is_void_v<std::invoke_result_t<Transform, Graph&, const algo::subgraph_match_witness<PV, TV, EL>&>>) {
        std::invoke(std::forward<Transform>(transform), g, witness);
    } else {
        g = std::invoke(std::forward<Transform>(transform), g, witness);
    }
    out.applied = true;
    return out;
}

#endif

} // namespace rewrite

// ============================================================
// Serialization hooks and schema lookup
// ============================================================

namespace gbin {

enum class decode_status {
    ok,
    truncated,
    bad_magic,
    unsupported_version,
    invalid_directed_flag,
    invalid_counts,
    trailing_bytes,
    vertex_decode_failed,
    edge_decode_failed
};

struct gbin_header_info {
    std::uint8_t version = 0;
    bool directed = false;
    std::uint32_t vertex_count = 0;
    std::uint32_t edge_count = 0;
};

template<class Graph>
struct decode_result {
    decode_status status { decode_status::truncated };
    std::optional<Graph> graph;
    std::optional<gbin_header_info> header;
    std::size_t offset = 0;
    std::string message;

    [[nodiscard]] bool ok() const noexcept {
        return status == decode_status::ok && graph.has_value();
    }
};

inline std::filesystem::path schema_root() {
    if (const char* env = std::getenv("GRAFITT_SCHEMA_DIR")) {
        if (*env != '\0') return std::filesystem::path{env};
    }
    return std::filesystem::path{"specs"};
}

inline std::filesystem::path default_schema_path() {
    return schema_root() / "GBIN.sktl";
}

[[nodiscard]] inline std::optional<gbin_header_info> inspect_header(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < 14) return std::nullopt;
    if (!(bytes[0] == 'G' && bytes[1] == 'B' && bytes[2] == 'I' && bytes[3] == 'N')) return std::nullopt;
    gbin_header_info h;
    h.version = bytes[4];
    if (bytes[5] == 0) h.directed = false;
    else if (bytes[5] == 1) h.directed = true;
    else return std::nullopt;
    h.vertex_count = static_cast<std::uint32_t>(bytes[6]) |
                     (static_cast<std::uint32_t>(bytes[7]) << 8) |
                     (static_cast<std::uint32_t>(bytes[8]) << 16) |
                     (static_cast<std::uint32_t>(bytes[9]) << 24);
    h.edge_count = static_cast<std::uint32_t>(bytes[10]) |
                   (static_cast<std::uint32_t>(bytes[11]) << 8) |
                   (static_cast<std::uint32_t>(bytes[12]) << 16) |
                   (static_cast<std::uint32_t>(bytes[13]) << 24);
    return h;
}

template<class Graph>
[[nodiscard]] inline std::vector<std::uint8_t> serialize(const Graph& g) {
    std::vector<std::uint8_t> out;
    auto emit_u8 = [&](std::uint8_t v) { out.push_back(v); };
    auto emit_u32 = [&](std::uint32_t v) {
        for (int i = 0; i < 4; ++i) emit_u8(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    };
    auto emit_str = [&](const std::string& s) {
        emit_u32(static_cast<std::uint32_t>(s.size()));
        out.insert(out.end(), s.begin(), s.end());
    };
    out.insert(out.end(), {'G', 'B', 'I', 'N'});
    emit_u8(1);
    emit_u8(g.is_directed() ? 1 : 0);
    emit_u32(static_cast<std::uint32_t>(g.nb_vertex()));
    emit_u32(static_cast<std::uint32_t>(g.nb_edges()));
    g.iter_vertex([&](const auto& v) { emit_str(to_string_fallback(v)); });
    g.iter_edges_e([&](const auto& e) {
        emit_str(to_string_fallback(e.src));
        emit_str(to_string_fallback(e.dst));
        emit_str(to_string_fallback(e.label));
    });
    return out;
}

template<class Graph>
[[nodiscard]] inline decode_result<Graph> deserialize_detailed(
    const std::vector<std::uint8_t>& bytes,
    bool strict_trailing_bytes = true
);

template<class Graph>
[[nodiscard]] inline std::optional<Graph> deserialize(const std::vector<std::uint8_t>& bytes) {
    auto detailed = deserialize_detailed<Graph>(bytes, false);
    if (!detailed.ok()) return std::nullopt;
    return detailed.graph;
}

template<class Graph>
[[nodiscard]] inline decode_result<Graph> deserialize_detailed(
    const std::vector<std::uint8_t>& bytes,
    bool strict_trailing_bytes
) {
    static_assert(std::is_same_v<typename Graph::vertex_type, std::string>, "GBIN deserialize currently requires std::string vertices");
    static_assert(std::is_same_v<typename Graph::edge_label_type, std::string> || std::is_same_v<typename Graph::edge_label_type, unit>,
        "GBIN deserialize currently supports edge labels of std::string or unit");
    decode_result<Graph> out;
    std::size_t i = 0;
    auto take_u8 = [&]() -> std::optional<std::uint8_t> {
        if (i >= bytes.size()) {
            out.status = decode_status::truncated;
            out.offset = i;
            out.message = "unexpected end of input while reading u8";
            return std::nullopt;
        }
        return bytes[i++];
    };
    auto take_u32 = [&]() -> std::optional<std::uint32_t> {
        if (i + 4 > bytes.size()) {
            out.status = decode_status::truncated;
            out.offset = i;
            out.message = "unexpected end of input while reading u32";
            return std::nullopt;
        }
        std::uint32_t v = 0;
        for (int b = 0; b < 4; ++b) v |= static_cast<std::uint32_t>(bytes[i++]) << (8 * b);
        return v;
    };
    auto take_str = [&]() -> std::optional<std::string> {
        auto n = take_u32();
        if (!n) return std::nullopt;
        if (i + *n > bytes.size()) {
            out.status = decode_status::truncated;
            out.offset = i;
            out.message = "unexpected end of input while reading string payload";
            return std::nullopt;
        }
        std::string s(bytes.begin() + static_cast<std::ptrdiff_t>(i), bytes.begin() + static_cast<std::ptrdiff_t>(i + *n));
        i += *n;
        return s;
    };
    if (bytes.size() < 14) {
        out.status = decode_status::truncated;
        out.offset = bytes.size();
        out.message = "input shorter than GBIN header";
        return out;
    }
    if (!(bytes[0] == 'G' && bytes[1] == 'B' && bytes[2] == 'I' && bytes[3] == 'N')) {
        out.status = decode_status::bad_magic;
        out.offset = 0;
        out.message = "invalid GBIN magic";
        return out;
    }
    i = 4;
    auto version = take_u8();
    auto dir = take_u8();
    if (!version || !dir) return out;
    if (*version != 1) {
        out.status = decode_status::unsupported_version;
        out.offset = 4;
        out.message = "unsupported GBIN version";
        return out;
    }
    if (!(*dir == 0 || *dir == 1)) {
        out.status = decode_status::invalid_directed_flag;
        out.offset = 5;
        out.message = "directed flag must be 0 or 1";
        return out;
    }
    auto vcount = take_u32();
    auto ecount = take_u32();
    if (!vcount || !ecount) return out;
    out.header = gbin_header_info{
        .version = *version,
        .directed = (*dir == 1),
        .vertex_count = *vcount,
        .edge_count = *ecount
    };
    Graph g((*dir == 1) ? direction::directed : direction::undirected);
    for (std::uint32_t v = 0; v < *vcount; ++v) {
        auto name = take_str();
        if (!name) {
            if (out.status == decode_status::truncated) out.status = decode_status::vertex_decode_failed;
            out.message = "failed to decode vertex entry";
            return out;
        }
        g.add_vertex(*name);
    }
    for (std::uint32_t e = 0; e < *ecount; ++e) {
        auto src = take_str();
        auto dst = take_str();
        auto lbl = take_str();
        if (!src || !dst || !lbl) {
            if (out.status == decode_status::truncated) out.status = decode_status::edge_decode_failed;
            out.message = "failed to decode edge entry";
            return out;
        }
        if constexpr (std::is_same_v<typename Graph::edge_label_type, std::string>) g.add_edge(*src, *dst, *lbl);
        else g.add_edge(*src, *dst);
    }
    if (strict_trailing_bytes && i != bytes.size()) {
        out.status = decode_status::trailing_bytes;
        out.offset = i;
        out.message = "trailing bytes after valid GBIN payload";
        return out;
    }
    out.status = decode_status::ok;
    out.offset = i;
    out.graph = std::move(g);
    out.message = "ok";
    return out;
}

} // namespace gbin

} // namespace grafitt
