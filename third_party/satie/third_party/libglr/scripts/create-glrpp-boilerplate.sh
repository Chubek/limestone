#!/usr/bin/env bash
# create_glrpp_boilerplate.sh - Generate boilerplate for GLRpp/GLRpp.hpp

set -euo pipefail

# Target file
TARGET_FILE="GLRpp/GLRpp.hpp"

# Create directory if it doesn't exist
mkdir -p "$(dirname "$TARGET_FILE")"

# Generate the boilerplate
cat > "$TARGET_FILE" << 'EOF'
#ifndef LIBGLR_GLRPP_GLRPP_HPP
#define LIBGLR_GLRPP_GLRPP_HPP

/**
 * @file GLRpp.hpp
 * @brief Modern C++ wrapper for libglr with DSL utilities
 * 
 * This header provides a type-safe, RAII-based C++ interface to libglr,
 * leveraging dslutils.hpp for DSL construction and Polyfills.hpp for
 * dynamic loading and C API interop.
 * 
 * @author GLRpp Project
 * @version 1.0.0
 * @date 2026-05-10
 */

// Standard library includes
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// Project includes
#include "Polyfills.hpp"

// Check for dslutils.hpp availability
#if __has_include("dslutils.hpp")
#include "dslutils.hpp"
#define GLRPP_HAS_DSLUTILS 1
#else
#error "GLRpp requires dslutils.hpp to be available"
#endif

// Feature detection
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
#define GLRPP_HAS_CONCEPTS 1
#endif

#if defined(__cpp_consteval) && __cpp_consteval >= 201811L
#define GLRPP_CONSTEVAL consteval
#else
#define GLRPP_CONSTEVAL constexpr
#endif

// Compiler attributes
#if defined(__GNUC__) || defined(__clang__)
#define GLRPP_LIKELY(x) __builtin_expect(!!(x), 1)
#define GLRPP_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define GLRPP_NODISCARD [[nodiscard]]
#define GLRPP_MAYBE_UNUSED [[maybe_unused]]
#else
#define GLRPP_LIKELY(x) (x)
#define GLRPP_UNLIKELY(x) (x)
#define GLRPP_NODISCARD
#define GLRPP_MAYBE_UNUSED
#endif

namespace glrpp {

// Forward declarations
class Parser;
class Grammar;
class Symbol;
class Production;
class ParseTree;
class ParseTreeNode;
class DisambiguationContext;
class DisambiguationHook;
class ProductionBuilder;
class DisambiguationBuilder;
class PrecedenceBuilder;
class AssociativityBuilder;

namespace detail {

// Internal implementation details

/**
 * @brief RAII wrapper for libglr resources
 * @tparam T Resource handle type
 * @tparam Deleter Function pointer type for cleanup
 */
template<typename T, auto Deleter>
class ResourceHandle {
public:
    using handle_type = T;
    using deleter_type = decltype(Deleter);

    ResourceHandle() noexcept : handle_(nullptr) {}
    
    explicit ResourceHandle(T handle) noexcept : handle_(handle) {}
    
    ~ResourceHandle() noexcept {
        if (handle_) {
            Deleter(handle_);
        }
    }

    // Move semantics
    ResourceHandle(ResourceHandle&& other) noexcept 
        : handle_(std::exchange(other.handle_, nullptr)) {}
    
    ResourceHandle& operator=(ResourceHandle&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                Deleter(handle_);
            }
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    // Delete copy semantics
    ResourceHandle(const ResourceHandle&) = delete;
    ResourceHandle& operator=(const ResourceHandle&) = delete;

    GLRPP_NODISCARD T get() const noexcept { return handle_; }
    GLRPP_NODISCARD explicit operator bool() const noexcept { return handle_ != nullptr; }
    
    T release() noexcept { return std::exchange(handle_, nullptr); }
    
    void reset(T new_handle = nullptr) noexcept {
        if (handle_) {
            Deleter(handle_);
        }
        handle_ = new_handle;
    }

private:
    T handle_;
};

/**
 * @brief Exception boundary wrapper for C callbacks
 * @tparam Fn Callable type
 */
template<typename Fn>
class ExceptionBoundary {
public:
    explicit ExceptionBoundary(Fn&& fn) : fn_(std::forward<Fn>(fn)) {}

    template<typename... Args>
    auto operator()(Args&&... args) noexcept -> decltype(fn_(std::forward<Args>(args)...)) {
        try {
            return fn_(std::forward<Args>(args)...);
        } catch (const std::exception& e) {
            // Log error or store for later retrieval
            // Cannot propagate through C boundary
            std::terminate();
        } catch (...) {
            std::terminate();
        }
    }

private:
    Fn fn_;
};

} // namespace detail

// ============================================================================
// Core RAII Wrappers
// ============================================================================

/**
 * @brief Type-safe wrapper for libglr symbol identifiers
 */
class Symbol {
public:
    using id_type = symbol_id_t;

    Symbol() noexcept : id_(0), name_() {}
    
    explicit Symbol(id_type id, std::string_view name = "") 
        : id_(id), name_(name) {}

    GLRPP_NODISCARD id_type id() const noexcept { return id_; }
    GLRPP_NODISCARD std::string_view name() const noexcept { return name_; }
    
    GLRPP_NODISCARD bool is_terminal() const noexcept;
    GLRPP_NODISCARD bool is_nonterminal() const noexcept;

    bool operator==(const Symbol& other) const noexcept { return id_ == other.id_; }
    bool operator!=(const Symbol& other) const noexcept { return id_ != other.id_; }

private:
    id_type id_;
    std::string name_;
};

/**
 * @brief Type-safe wrapper for libglr production identifiers
 */
class Production {
public:
    using id_type = production_id_t;

    Production() noexcept : id_(0) {}
    explicit Production(id_type id) noexcept : id_(id) {}

    GLRPP_NODISCARD id_type id() const noexcept { return id_; }

    bool operator==(const Production& other) const noexcept { return id_ == other.id_; }
    bool operator!=(const Production& other) const noexcept { return id_ != other.id_; }

private:
    id_type id_;
};

// ============================================================================
// DSL Base Class
// ============================================================================

/**
 * @brief DSL for grammar definition using dslutils.hpp facilities
 * 
 * Provides a fluent interface for defining GLR grammars with support for:
 * - Terminal and non-terminal symbols
 * - Production rules with semantic actions
 * - Precedence and associativity
 * - Disambiguation hooks
 * - Pattern matching and rewriting
 * 
 * @tparam Derived CRTP derived class
 */
template<typename Derived>
class GrammarDSL : public dsl::DSL<Derived,
                                    dsl::PatternMatch,
                                    dsl::Pipeline,
                                    dsl::CustomLiterals,
                                    dsl::Rewrite> {
public:
    using base_type = dsl::DSL<Derived, dsl::PatternMatch, dsl::Pipeline, 
                                dsl::CustomLiterals, dsl::Rewrite>;

    /**
     * @brief Define a terminal symbol
     * @param name Symbol name
     * @return Symbol object
     */
    GLRPP_NODISCARD Symbol terminal(std::string_view name);

    /**
     * @brief Define a non-terminal symbol
     * @param name Symbol name
     * @return Symbol object
     */
    GLRPP_NODISCARD Symbol nonterminal(std::string_view name);

    /**
     * @brief Begin defining a production rule
     * @param lhs Left-hand side symbol
     * @return ProductionBuilder for fluent chaining
     */
    GLRPP_NODISCARD ProductionBuilder rule(Symbol lhs);

    /**
     * @brief Define precedence level
     * @param level Precedence level (higher = tighter binding)
     * @return PrecedenceBuilder for fluent chaining
     */
    GLRPP_NODISCARD PrecedenceBuilder precedence(int level);

protected:
    GrammarDSL() = default;
    ~GrammarDSL() = default;
};

// ============================================================================
// Builder Classes
// ============================================================================

/**
 * @brief Fluent builder for production rules
 */
class ProductionBuilder {
public:
    explicit ProductionBuilder(Symbol lhs);

    /**
     * @brief Add symbol to right-hand side
     * @param rhs Symbol to add
     * @return Reference to this builder
     */
    ProductionBuilder& operator>>(Symbol rhs);

    /**
     * @brief Attach semantic action
     * @tparam Fn Callable type
     * @param action Semantic action callback
     * @return Reference to this builder
     */
    template<typename Fn>
    ProductionBuilder& action(Fn&& action);

    /**
     * @brief Set precedence level
     * @param level Precedence level
     * @return Reference to this builder
     */
    ProductionBuilder& prec(int level);

    /**
     * @brief Build the production
     * @return Production object
     */
    GLRPP_NODISCARD Production build();

private:
    Symbol lhs_;
    std::vector<Symbol> rhs_;
    std::optional<std::function<void()>> action_;
    std::optional<int> precedence_;
};

/**
 * @brief Fluent builder for disambiguation hooks
 */
class DisambiguationBuilder {
public:
    DisambiguationBuilder() = default;

    /**
     * @brief Prefer certain parse branches
     * @tparam Pred Predicate type
     * @param predicate Condition for preference
     * @return Reference to this builder
     */
    template<typename Pred>
    DisambiguationBuilder& prefer(Pred&& predicate);

    /**
     * @brief Reject certain parse branches
     * @tparam Pred Predicate type
     * @param predicate Condition for rejection
     * @return Reference to this builder
     */
    template<typename Pred>
    DisambiguationBuilder& reject(Pred&& predicate);

    /**
     * @brief Build the disambiguation hook
     * @return DisambiguationHook object
     */
    GLRPP_NODISCARD DisambiguationHook build();

private:
    std::vector<std::function<bool()>> prefer_predicates_;
    std::vector<std::function<bool()>> reject_predicates_;
};

// ============================================================================
// Disambiguation API
// ============================================================================

/**
 * @brief Wrapper for libglr disambiguation context
 */
class DisambiguationContext {
public:
    enum class ConflictType {
        ShiftReduce,
        ReduceReduce,
        Unknown
    };

    DisambiguationContext() = default;

    GLRPP_NODISCARD ConflictType conflict_type() const noexcept;
    GLRPP_NODISCARD std::vector<Production> conflicting_productions() const;

private:
    // TODO: Wrap glr_disambig_context_t
};

/**
 * @brief Wrapper for libglr disambiguation hook
 */
class DisambiguationHook {
public:
    DisambiguationHook() = default;

    /**
     * @brief Register this hook with a grammar
     * @param grammar Target grammar
     */
    void register_with(Grammar& grammar);

private:
    // TODO: Wrap glr_disambig_hook_t
};

// ============================================================================
// AST Integration
// ============================================================================

/**
 * @brief Parse tree node using dslutils.hpp AST facilities
 */
class ParseTreeNode : public dsl::ASTNode<ParseTreeNode> {
public:
    using base_type = dsl::ASTNode<ParseTreeNode>;

    ParseTreeNode() = default;

    GLRPP_NODISCARD Symbol symbol() const noexcept;
    GLRPP_NODISCARD std::vector<ParseTreeNode> children() const;

    /**
     * @brief Apply visitor pattern
     * @tparam Visitor Visitor type
     * @param visitor Visitor callable
     */
    template<typename Visitor>
    void accept(Visitor&& visitor);

private:
    Symbol symbol_;
    std::vector<ParseTreeNode> children_;
};

/**
 * @brief Complete parse tree (SPPF wrapper)
 */
class ParseTree {
public:
    ParseTree() = default;

    GLRPP_NODISCARD ParseTreeNode root() const;
    GLRPP_NODISCARD bool is_ambiguous() const noexcept;
    GLRPP_NODISCARD size_t num_parses() const noexcept;

private:
    // TODO: Wrap libglr SPPF structure
};

// ============================================================================
// Main API Classes
// ============================================================================

/**
 * @brief Grammar definition and management
 */
class Grammar {
public:
    Grammar();
    ~Grammar();

    // Move semantics
    Grammar(Grammar&&) noexcept;
    Grammar& operator=(Grammar&&) noexcept;

    // Delete copy semantics
    Grammar(const Grammar&) = delete;
    Grammar& operator=(const Grammar&) = delete;

    /**
     * @brief Add terminal symbol
     * @param name Symbol name
     * @return Symbol object
     */
    GLRPP_NODISCARD Symbol add_terminal(std::string_view name);

    /**
     * @brief Add non-terminal symbol
     * @param name Symbol name
     * @return Symbol object
     */
    GLRPP_NODISCARD Symbol add_nonterminal(std::string_view name);

    /**
     * @brief Add production rule
     * @param lhs Left-hand side symbol
     * @param rhs Right-hand side symbols
     * @return Production object
     */
    GLRPP_NODISCARD Production add_production(Symbol lhs, std::vector<Symbol> rhs);

    /**
     * @brief Set start symbol
     * @param start Start symbol
     */
    void set_start_symbol(Symbol start);

private:
    // TODO: Wrap libglr grammar handle
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief GLR parser instance
 */
class Parser {
public:
    explicit Parser(Grammar& grammar);
    ~Parser();

    // Move semantics
    Parser(Parser&&) noexcept;
    Parser& operator=(Parser&&) noexcept;

    // Delete copy semantics
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;

    /**
     * @brief Parse input string
     * @param input Input string
     * @return Parse tree
     * @throws std::runtime_error on parse error
     */
    GLRPP_NODISCARD ParseTree parse(std::string_view input);

    /**
     * @brief Register disambiguation hook
     * @param hook Disambiguation hook
     */
    void register_disambiguation(DisambiguationHook hook);

private:
    // TODO: Wrap libglr parser handle
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Convenience Functions and Operators
// ============================================================================

namespace literals {

/**
 * @brief User-defined literal for terminal symbols
 * @param str Symbol name
 * @param len String length
 * @return Symbol object
 */
GLRPP_NODISCARD inline Symbol operator""_T(const char* str, size_t len) {
    return Symbol(0, std::string_view(str, len));
}

/**
 * @brief User-defined literal for non-terminal symbols
 * @param str Symbol name
 * @param len String length
 * @return Symbol object
 */
GLRPP_NODISCARD inline Symbol operator""_NT(const char* str, size_t len) {
    return Symbol(0, std::string_view(str, len));
}

} // namespace literals

// ============================================================================
// Template Implementations
// ============================================================================

template<typename Fn>
ProductionBuilder& ProductionBuilder::action(Fn&& action) {
    action_ = std::forward<Fn>(action);
    return *this;
}

template<typename Pred>
DisambiguationBuilder& DisambiguationBuilder::prefer(Pred&& predicate) {
    prefer_predicates_.push_back(std::forward<Pred>(predicate));
    return *this;
}

template<typename Pred>
DisambiguationBuilder& DisambiguationBuilder::reject(Pred&& predicate) {
    reject_predicates_.push_back(std::forward<Pred>(predicate));
    return *this;
}

template<typename Visitor>
void ParseTreeNode::accept(Visitor&& visitor) {
    visitor(*this);
    for (auto& child : children_) {
        child.accept(std::forward<Visitor>(visitor));
    }
}

template<typename Derived>
Symbol GrammarDSL<Derived>::terminal(std::string_view name) {
    // TODO: Implement using Polyfills.hpp C API wrappers
    return Symbol(0, name);
}

template<typename Derived>
Symbol GrammarDSL<Derived>::nonterminal(std::string_view name) {
    // TODO: Implement using Polyfills.hpp C API wrappers
    return Symbol(0, name);
}

template<typename Derived>
ProductionBuilder GrammarDSL<Derived>::rule(Symbol lhs) {
    return ProductionBuilder(lhs);
}

template<typename Derived>
PrecedenceBuilder GrammarDSL<Derived>::precedence(int level) {
    // TODO: Implement
    return PrecedenceBuilder();
}

} // namespace glrpp

#endif // LIBGLR_GLRPP_GLRPP_HPP
EOF

echo "✓ Created $TARGET_FILE with boilerplate"
echo ""
echo "Next steps:"
echo "  1. Implement Grammar::Impl and Parser::Impl classes"
echo "  2. Add Polyfills.hpp patches for disambiguation API"
echo "  3. Integrate dslutils.hpp facilities"
echo "  4. Add comprehensive Doxygen documentation"
echo "  5. Implement unit tests"
