#ifndef LIBGLR_GLRPP_GLRPP_HPP
#define LIBGLR_GLRPP_GLRPP_HPP

#include <climits>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <array>

#include "DSLUtils.hpp"
#include "Polyfills.hpp"
#include "rewrite/GrammarIR.hpp"
#include "rewrite/RewritePipeline.hpp"

#include <glr/disambiguate.h>
#include <glr/cache.h>
#include <glr/dependency.h>
#include <glr/diff.h>
#include <glr/graph.h>
#include <glr/forest.h>
#include <glr/grammar.h>
#include <glr/lexer-hooks.h>
#include <glr/live-parsing.h>
#include <glr/parser.h>
#include <glr/query.h>
#include <glr/scannerless.h>
#include <glr/serialization.h>
#include <glr/semantic-action.h>

namespace glrpp
{

namespace detail
{
template <typename T, auto DeleterFunc> class ResourceHandle
{
public:
  explicit ResourceHandle (T handle = nullptr) noexcept : handle_ (handle) {}
  ~ResourceHandle () { reset (); }
  ResourceHandle (ResourceHandle &&other) noexcept
      : handle_ (std::exchange (other.handle_, nullptr))
  {
  }
  ResourceHandle &
  operator= (ResourceHandle &&other) noexcept
  {
    if (this != &other)
      {
        reset ();
        handle_ = std::exchange (other.handle_, nullptr);
      }
    return *this;
  }
  ResourceHandle (const ResourceHandle &) = delete;
  ResourceHandle &operator= (const ResourceHandle &) = delete;

  T
  get () const noexcept
  {
    return handle_;
  }
  explicit
  operator bool () const noexcept
  {
    return handle_ != nullptr;
  }
  void
  reset (T handle = nullptr) noexcept
  {
    if (handle_ != nullptr)
      {
        DeleterFunc (handle_);
      }
    handle_ = handle;
  }
  T
  release () noexcept
  {
    return std::exchange (handle_, nullptr);
  }

private:
  T handle_;
};
} // namespace detail

class Symbol
{
public:
  Symbol () = default;
  Symbol (int id, std::string_view name, bool is_terminal)
      : id_ (id), name_ (name), is_terminal_ (is_terminal)
  {
  }
  int
  id () const noexcept
  {
    return id_;
  }
  std::string_view
  name () const noexcept
  {
    return name_;
  }
  bool
  is_terminal () const noexcept
  {
    return is_terminal_;
  }

private:
  int id_ = -1;
  std::string name_;
  bool is_terminal_ = false;
};

class Production
{
public:
  Production () = default;
  explicit Production (int id) : id_ (id) {}
  int
  id () const noexcept
  {
    return id_;
  }

private:
  int id_ = -1;
};

class DisambiguationContext
{
public:
  explicit DisambiguationContext (glr_disambig_context_t *ctx) : ctx_ (ctx) {}
  size_t
  candidate_count () const noexcept
  {
    return ctx_->candidate_count;
  }
  const glr_disambig_candidate_t &
  candidate (size_t i) const
  {
    return ctx_->candidates[i];
  }
  void
  reject (size_t i)
  {
    glr_disambig_context_reject_candidate (ctx_, i);
  }
  void
  set_score (size_t i, double score)
  {
    ctx_->candidates[i].score = score;
  }
  int
  lookahead_symbol () const noexcept
  {
    return ctx_->lookahead_symbol_id;
  }

private:
  glr_disambig_context_t *ctx_;
};

class DisambiguationHook
{
public:
  DisambiguationHook () = default;
  DisambiguationHook (std::string_view name, int priority, glr_disambig_fn fn,
                      void *user_data = nullptr,
                      glr_disambig_destroy_fn destroy = nullptr)
      : handle_ (glr_disambig_hook_create (std::string (name).c_str (),
                                           priority, fn, user_data, destroy))
  {
    if (!handle_)
      {
        throw std::runtime_error ("glr_disambig_hook_create failed");
      }
  }

  glr_disambig_hook_t *
  release () noexcept
  {
    return handle_.release ();
  }

private:
  friend class Parser;
  using HookHandle = detail::ResourceHandle<glr_disambig_hook_t *,
                                            &glr_disambig_hook_destroy>;
  HookHandle handle_;
};

class ParseTreeNode : public dsl::ASTNode
{
public:
  explicit ParseTreeNode (glr_forest_node_t *node = nullptr) : node_ (node) {}
  int
  symbol_id () const noexcept
  {
    return node_ ? node_->symbol_id : -1;
  }
  std::vector<ParseTreeNode>
  children () const
  {
    std::vector<ParseTreeNode> out;
    if (!node_)
      {
        return out;
      }
    out.reserve (node_->child_count);
    for (size_t i = 0; i < node_->child_count; ++i)
      {
        out.emplace_back (node_->children[i]);
      }
    return out;
  }

private:
  glr_forest_node_t *node_ = nullptr;
};

class ParseTree
{
public:
  explicit ParseTree (glr_forest_t *forest = nullptr) : forest_ (forest) {}
  bool
  is_ambiguous () const
  {
    if (!forest_)
      {
        return false;
      }
    for (size_t i = 0; i < forest_->node_count; ++i)
      {
        /* SPPF ambiguity shows up as a sibling chain: more than one
           packed node sharing the same input position. A large
           child_count on a single node is normal derivation width,
           not ambiguity. */
        size_t chain = 0;
        for (auto *n = forest_->nodes[i]; n != nullptr; n = n->next)
          {
            ++chain;
            if (chain > 1)
              {
                return true;
              }
          }
      }
    return false;
  }
  size_t
  num_parses () const noexcept
  {
    if (!forest_)
      {
        return 0u;
      }
    /* Count the widest sibling chain: each packed alternative at the
       most ambiguous position is a distinct parse. Non-ambiguous
       forests yield exactly one parse. */
    size_t widest = 1u;
    for (size_t i = 0; i < forest_->node_count; ++i)
      {
        size_t chain = 0;
        for (auto *n = forest_->nodes[i]; n != nullptr; n = n->next)
          {
            ++chain;
          }
        if (chain > widest)
          {
            widest = chain;
          }
      }
    return widest;
  }
  ParseTreeNode
  root () const
  {
    if (!forest_ || forest_->node_count == 0 || !forest_->nodes[0])
      {
        return ParseTreeNode{};
      }
    return ParseTreeNode{ forest_->nodes[0] };
  }

  glr_forest_t *handle () const noexcept { return forest_; }
  size_t node_count () const noexcept
  {
    return glr_forest_total_nodes (forest_);
  }

private:
  glr_forest_t *forest_ = nullptr;
};

class Grammar
{
public:
  using Status = dsl::Result<bool, std::string>;

  Grammar () : handle_ (glr_grammar_create ())
  {
    if (!handle_)
      {
        throw std::runtime_error ("glr_grammar_create failed");
      }
  }

  Symbol
  terminal (std::string_view name)
  {
    return add_symbol (name, GLR_SYMBOL_TERMINAL);
  }
  Symbol
  nonterminal (std::string_view name)
  {
    return add_symbol (name, GLR_SYMBOL_NONTERMINAL);
  }
  Production
  add_production (Symbol lhs, const std::vector<Symbol> &rhs)
  {
    auto result = try_add_production (lhs, rhs, std::nullopt, std::nullopt);
    if (result.is_err ())
      {
        throw std::runtime_error ("glr_grammar_add_production failed");
      }
    return result.unwrap ();
  }
  dsl::Result<Production, std::string>
  try_add_production (Symbol lhs, const std::vector<Symbol> &rhs)
  {
    return try_add_production (lhs, rhs, std::nullopt, std::nullopt);
  }
  dsl::Result<Production, std::string>
  try_add_production (Symbol lhs, const std::vector<Symbol> &rhs,
                      std::optional<int> precedence,
                      std::optional<glr_disambig_associativity_t> assoc)
  {
    std::vector<glr_symbol_t *> body;
    std::vector<int> rhs_ids;
    body.reserve (rhs.size ());
    rhs_ids.reserve (rhs.size ());
    for (const auto &s : rhs)
      {
        auto *sym = glr_grammar_get_symbol (handle_.get (), s.id ());
        if (!sym)
          {
            return dsl::Result<Production, std::string>::from_err (
                "invalid rhs symbol id");
          }
        body.push_back (sym);
        rhs_ids.push_back (s.id ());
      }
    int id = glr_grammar_add_production (handle_.get (), lhs.id (),
                                         body.data (), body.size ());
    if (id < 0)
      {
        return dsl::Result<Production, std::string>::from_err (
            "glr_grammar_add_production failed");
      }
    rewrite_ir_.productions.push_back (
        { id, lhs.id (), std::move (rhs_ids), precedence, assoc, {}, {} });
    if (id >= rewrite_ir_.next_production_id)
      {
        rewrite_ir_.next_production_id = id + 1;
      }
    return dsl::Result<Production, std::string>::from_ok (Production (id));
  }
  void
  set_start (Symbol start)
  {
    auto st = try_set_start (start);
    if (st.is_err ())
      {
        throw std::runtime_error ("glr_grammar_set_start_symbol failed");
      }
  }
  Status
  try_set_start (Symbol start)
  {
    if (glr_grammar_set_start_symbol (handle_.get (), start.id ()) != 0)
      {
        return Status::from_err ("glr_grammar_set_start_symbol failed");
      }
    rewrite_ir_.start_symbol_id = start.id ();
    return Status::from_ok (true);
  }

  Status
  set_scannerless_pattern (Symbol terminal, std::string_view expression)
  {
    char error[256] = {};
    if (glr_scannerless_set_pattern (handle_.get (), terminal.id (),
                                     std::string (expression).c_str (), error,
                                     sizeof error)
        != 0)
      return Status::from_err (error[0] ? error : "invalid scannerless pattern");
    return Status::from_ok (true);
  }

  Status
  set_scannerless_literal (Symbol terminal, std::string_view literal)
  {
    if (glr_scannerless_set_literal (handle_.get (), terminal.id (),
                                     literal.data (), literal.size ())
        != 0)
      return Status::from_err ("invalid scannerless literal");
    return Status::from_ok (true);
  }

  bool has_scannerless_patterns () const noexcept
  {
    return glr_scannerless_has_patterns (handle_.get ());
  }

  Status set_alias (Production production, size_t position,
                    std::string_view alias)
  {
    std::string copy (alias);
    if (glr_production_set_alias (handle_.get (), production.id (), position,
                                  copy.c_str ())
        != 0)
      return Status::from_err ("invalid production alias");
    return Status::from_ok (true);
  }

  Status set_semantic_action (Production production, glr_semantic_action_fn fn,
                              void *data = nullptr,
                              glr_semantic_data_destroy_fn destroy = nullptr)
  {
    if (glr_production_set_semantic_action (handle_.get (), production.id (),
                                            fn, data, destroy)
        != 0)
      return Status::from_err ("invalid semantic action");
    return Status::from_ok (true);
  }

  bool has_semantic_actions () const noexcept
  {
    return glr_grammar_has_semantic_actions (handle_.get ());
  }
  rewrite::GrammarIR
  to_ir () const
  {
    return rewrite_ir_;
  }
  static Grammar
  from_ir (const rewrite::GrammarIR &ir)
  {
    std::string validation_error;
    if (!ir.validate (&validation_error))
      {
        throw std::runtime_error ("invalid GrammarIR: " + validation_error);
      }

    Grammar out;
    std::unordered_map<int, Symbol> id_map;
    for (const auto &symbol : ir.symbols)
      {
        Symbol added = symbol.is_terminal ? out.terminal (symbol.name)
                                          : out.nonterminal (symbol.name);
        id_map.emplace (symbol.id, added);
      }

    for (const auto &production : ir.productions)
      {
        std::vector<Symbol> rhs_symbols;
        rhs_symbols.reserve (production.rhs.size ());
        for (int rhs : production.rhs)
          {
            rhs_symbols.push_back (id_map.at (rhs));
          }
        out.try_add_production (id_map.at (production.lhs), rhs_symbols,
                                production.precedence,
                                production.associativity);
      }
    if (ir.start_symbol_id)
      {
        out.set_start (id_map.at (*ir.start_symbol_id));
      }
    out.rewrite_ir_ = ir;
    return out;
  }
  Grammar
  rewritten (const rewrite::RewritePipeline &pipeline) const
  {
    auto ir = to_ir ();
    pipeline.run (ir);
    return from_ir (ir);
  }
  glr_grammar_t *
  handle () const noexcept
  {
    return handle_.get ();
  }

private:
  Symbol
  add_symbol (std::string_view name, glr_symbol_type_t type)
  {
    if (name.empty ())
      {
        throw std::runtime_error ("symbol name must not be empty");
      }
    auto key = std::string (name);
    auto it = symbol_map_.find (key);
    if (it != symbol_map_.end ())
      {
        auto *sym = glr_grammar_get_symbol (handle_.get (), it->second);
        if (!sym)
          {
            throw std::runtime_error ("symbol map out of sync with grammar");
          }
        const bool want_terminal = type == GLR_SYMBOL_TERMINAL;
        const bool have_terminal = sym->type == GLR_SYMBOL_TERMINAL;
        if (want_terminal != have_terminal)
          {
            throw std::runtime_error (
                "symbol '" + key
                + "' already exists with a different kind");
          }
        return Symbol (it->second, key, have_terminal);
      }
    int id = glr_grammar_add_symbol (handle_.get (), type, key.c_str ());
    if (id < 0)
      {
        throw std::runtime_error ("glr_grammar_add_symbol failed");
      }
    symbol_map_.emplace (key, id);
    rewrite_ir_.symbols.push_back (
        { id, key, type == GLR_SYMBOL_TERMINAL, {}, {} });
    if (id >= rewrite_ir_.next_symbol_id)
      {
        rewrite_ir_.next_symbol_id = id + 1;
      }
    return Symbol (id, key, type == GLR_SYMBOL_TERMINAL);
  }

  using GrammarHandle
      = detail::ResourceHandle<glr_grammar_t *, &glr_grammar_destroy>;
  GrammarHandle handle_;
  std::unordered_map<std::string, int> symbol_map_;
  rewrite::GrammarIR rewrite_ir_;
};

class ProductionBuilder
{
public:
  ProductionBuilder (Grammar &grammar, Symbol lhs)
      : grammar_ (grammar), lhs_ (lhs)
  {
  }
  ProductionBuilder &
  operator>> (Symbol rhs)
  {
    rhs_.push_back (rhs);
    return *this;
  }
  ProductionBuilder &
  prec (int p)
  {
    precedence_ = p;
    return *this;
  }
  ProductionBuilder &
  assoc (glr_disambig_associativity_t a)
  {
    assoc_ = a;
    return *this;
  }
  Production
  build ()
  {
    auto result = grammar_.try_add_production (lhs_, rhs_, precedence_, assoc_);
    if (result.is_err ())
      {
        throw std::runtime_error ("glr_grammar_add_production failed");
      }
    return result.unwrap ();
  }

private:
  Grammar &grammar_;
  Symbol lhs_;
  std::vector<Symbol> rhs_;
  std::optional<int> precedence_;
  std::optional<glr_disambig_associativity_t> assoc_;
};

class DisambiguationBuilder
{
public:
  using Predicate = std::function<bool (const DisambiguationContext &)>;
  using Action = std::function<glr_disambig_result_t (DisambiguationContext &,
                                                      size_t &)>;

  DisambiguationBuilder &
  when (Predicate pred)
  {
    clauses_.push_back ({ std::move (pred), {} });
    return *this;
  }
  DisambiguationBuilder &
  otherwise ()
  {
    clauses_.push_back (
        { [] (const DisambiguationContext &) { return true; }, {} });
    return *this;
  }
  DisambiguationBuilder &
  prefer (size_t index)
  {
    clauses_.back ().action = [index] (DisambiguationContext &, size_t &winner)
      {
        winner = index;
        return GLR_DISAMBIG_RESOLVED;
      };
    return *this;
  }
  DisambiguationBuilder &
  reject ()
  {
    clauses_.back ().action = [] (DisambiguationContext &ctx, size_t &)
      {
        for (size_t i = 0; i < ctx.candidate_count (); ++i)
          {
            ctx.reject (i);
          }
        return GLR_DISAMBIG_NO_MATCH;
      };
    return *this;
  }
  DisambiguationBuilder &
  reject_candidate (size_t index)
  {
    clauses_.back ().action = [index] (DisambiguationContext &ctx, size_t &)
      {
        if (index < ctx.candidate_count ())
          {
            ctx.reject (index);
          }
        return GLR_DISAMBIG_NO_MATCH;
      };
    return *this;
  }
  DisambiguationHook
  build (std::string_view name = "custom", int priority = 0)
  {
    auto *state = new std::vector<Clause> (std::move (clauses_));
    auto fn = +[] (glr_disambig_context_t *ctx, size_t *winner, void *ud)
                {
                  auto *clauses = static_cast<std::vector<Clause> *> (ud);
                  DisambiguationContext w (ctx);
                  for (const auto &cl : *clauses)
                    {
                      if (cl.predicate && cl.predicate (w))
                        {
                          return cl.action ? cl.action (w, *winner)
                                           : GLR_DISAMBIG_NO_MATCH;
                        }
                    }
                  return GLR_DISAMBIG_NO_MATCH;
                };
    auto destroy
        = +[] (void *ud) { delete static_cast<std::vector<Clause> *> (ud); };
    return DisambiguationHook (name, priority, fn, state, destroy);
  }

private:
  struct Clause
  {
    Predicate predicate;
    Action action;
  };
  std::vector<Clause> clauses_;
};

template <typename Derived>
class GrammarDSL : public dsl::DSL<Derived, dsl::PatternMatch, dsl::Pipeline,
                                   dsl::CustomLiterals, dsl::Rewrite, dsl::AST>
{
public:
  Symbol
  terminal (std::string_view name)
  {
    return static_cast<Derived *> (this)->grammar_.terminal (name);
  }
  Symbol
  nonterminal (std::string_view name)
  {
    return static_cast<Derived *> (this)->grammar_.nonterminal (name);
  }
  ProductionBuilder
  rule (Symbol lhs)
  {
    return ProductionBuilder (static_cast<Derived *> (this)->grammar_, lhs);
  }
  void
  start (Symbol s)
  {
    static_cast<Derived *> (this)->grammar_.set_start (s);
  }
};

class Parser
{
public:
  using ParseResult = dsl::Result<ParseTree, std::string>;
  using Status = dsl::Result<bool, std::string>;

  explicit Parser (Grammar &grammar)
      : grammar_ (&grammar), handle_ (glr_parser_create (grammar.handle ()))
  {
    if (!handle_)
      {
        throw std::runtime_error ("glr_parser_create failed");
      }
  }

  ParseTree
  parse (std::string_view input)
  {
    auto out = try_parse (input);
    if (out.is_err ())
      {
        throw std::runtime_error ("glr_parse failed");
      }
    return out.unwrap ();
  }

  ParseResult
  try_parse (std::string_view input)
  {
    auto result = glr_parse (handle_.get (), input.data (), input.size ());
    if (result.error != GLR_PARSE_SUCCESS)
      {
        return ParseResult::from_err ("glr_parse failed with error code "
                                      + std::to_string (result.error)
                                      + " at position "
                                      + std::to_string (result.position));
      }
    return ParseResult::from_ok (ParseTree (result.forest));
  }

  void
  register_disambiguation (DisambiguationHook hook)
  {
    auto st = try_register_disambiguation (std::move (hook));
    if (st.is_err ())
      {
        throw std::runtime_error ("glr_parser_add_disambiguator failed");
      }
  }

  Status
  try_register_disambiguation (DisambiguationHook hook)
  {
    auto *raw = hook.release ();
    if (glr_parser_add_disambiguator (handle_.get (), raw) != 0)
      {
        glr_disambig_hook_destroy (raw);
        return Status::from_err ("glr_parser_add_disambiguator failed");
      }
    return Status::from_ok (true);
  }

  Status set_scannerless (bool enabled)
  {
    if (glr_parser_set_scannerless (handle_.get (), enabled) != 0)
      return Status::from_err ("failed to configure scannerless parsing");
    return Status::from_ok (true);
  }

  Status set_trivia (std::string_view trivia)
  {
    std::string copy (trivia);
    if (glr_parser_set_trivia (handle_.get (), copy.c_str ()) != 0)
      return Status::from_err ("failed to configure parser trivia");
    return Status::from_ok (true);
  }

  glr_parse_error_t error () const noexcept
  {
    return glr_parser_get_error (handle_.get ());
  }

  glr_parser_t *
  handle () const noexcept
  {
    return handle_.get ();
  }

private:
  using ParserHandle
      = detail::ResourceHandle<glr_parser_t *, &glr_parser_destroy>;
  Grammar *grammar_;
  ParserHandle handle_;
};

class Forest
{
public:
  Forest () : handle_ (glr_forest_create ())
  {
    if (!handle_)
      throw std::runtime_error ("glr_forest_create failed");
  }
  explicit Forest (glr_forest_t *forest) : handle_ (forest) {}
  glr_forest_t *handle () const noexcept { return handle_.get (); }
  size_t size () const noexcept { return glr_forest_total_nodes (handle_.get ()); }
  bool ambiguous () const noexcept
  {
    return handle_ && glr_forest_is_ambiguous (root_handle ());
  }
  ParseTree view () const noexcept { return ParseTree (handle_.get ()); }
  Forest clone () const
  {
    auto *copy = glr_forest_clone (handle_.get ());
    if (!copy)
      throw std::runtime_error ("glr_forest_clone failed");
    return Forest (copy);
  }
  glr_forest_t *release () noexcept { return handle_.release (); }

private:
  glr_forest_node_t *root_handle () const noexcept
  {
    auto *forest = handle_.get ();
    return forest && forest->node_count ? forest->nodes[0] : nullptr;
  }
  using Handle = detail::ResourceHandle<glr_forest_t *, &glr_forest_destroy>;
  Handle handle_;
};

class SerializedBuffer
{
public:
  SerializedBuffer () = default;
  SerializedBuffer (uint8_t *data, size_t size) : data_ (data), size_ (size) {}
  ~SerializedBuffer () { std::free (data_); }
  SerializedBuffer (SerializedBuffer &&other) noexcept
      : data_ (std::exchange (other.data_, nullptr)),
        size_ (std::exchange (other.size_, 0)) {}
  SerializedBuffer &operator= (SerializedBuffer &&other) noexcept
  {
    if (this != &other)
      {
        std::free (data_);
        data_ = std::exchange (other.data_, nullptr);
        size_ = std::exchange (other.size_, 0);
      }
    return *this;
  }
  SerializedBuffer (const SerializedBuffer &) = delete;
  SerializedBuffer &operator= (const SerializedBuffer &) = delete;
  const uint8_t *data () const noexcept { return data_; }
  size_t size () const noexcept { return size_; }
  explicit operator bool () const noexcept { return data_ != nullptr; }

private:
  uint8_t *data_ = nullptr;
  size_t size_ = 0;
};

inline SerializedBuffer
serialize (const ParseTree &tree)
{
  uint8_t *data = nullptr;
  size_t size = 0;
  if (glr_serialize_forest (tree.handle (), &data, &size) != 0)
    throw std::runtime_error ("glr_serialize_forest failed");
  return SerializedBuffer (data, size);
}

inline Forest
deserialize_forest (const SerializedBuffer &buffer)
{
  glr_forest_t *forest = nullptr;
  if (glr_deserialize_forest (buffer.data (), buffer.size (), &forest) != 0)
    throw std::runtime_error ("glr_deserialize_forest failed");
  return Forest (forest);
}

struct Edit
{
  size_t old_start = 0;
  size_t old_end = 0;
  size_t new_start = 0;
  size_t new_end = 0;
  bool is_insertion = false;
  bool is_deletion = false;
  bool is_replacement = false;
  size_t old_length () const noexcept { return old_end - old_start; }
  size_t new_length () const noexcept { return new_end - new_start; }
  bool empty () const noexcept { return old_start == old_end && new_start == new_end; }
};

class Scannerless
{
public:
  static std::vector<glr_terminal_match_t>
  scan (const Grammar &grammar, std::string_view input, size_t position = 0)
  {
    glr_terminal_match_t *matches = nullptr;
    size_t count = 0;
    if (glr_scannerless_scan (grammar.handle (), input.data (), input.size (),
                              position, &matches, &count)
        != 0)
      throw std::runtime_error ("glr_scannerless_scan failed");
    std::vector<glr_terminal_match_t> result (matches, matches + count);
    std::free (matches);
    return result;
  }

  static size_t match (const Symbol &symbol, std::string_view input)
  {
    glr_symbol_t terminal{};
    terminal.id = symbol.id ();
    terminal.type = GLR_SYMBOL_TERMINAL;
    terminal.name = const_cast<char *> (symbol.name ().data ());
    size_t length = 0;
    if (glr_scannerless_match (&terminal, input.data (), input.size (), &length)
        < 0)
      throw std::runtime_error ("glr_scannerless_match failed");
    return length;
  }
};

inline std::pair<uint32_t, uint32_t>
line_column_of (std::string_view text, size_t offset)
{
  uint32_t line = 0;
  uint32_t column = 0;
  if (glr_live_line_column_of (text.data (), text.size (), offset, &line,
                               &column)
      != 0)
    throw std::out_of_range ("offset is outside text");
  return { line, column };
}

inline size_t
offset_of (std::string_view text, uint32_t line, uint32_t column)
{
  size_t offset = 0;
  if (glr_live_offset_of (text.data (), text.size (), line, column, &offset)
      != 0)
    throw std::out_of_range ("line and column are outside text");
  return offset;
}

inline Edit
compute_edit (std::string_view old_text, std::string_view new_text)
{
  glr_edit_t edit{};
  if (glr_compute_edit (old_text.data (), old_text.size (), new_text.data (),
                        new_text.size (), &edit)
      != 0)
    throw std::runtime_error ("glr_compute_edit failed");
  return { edit.old_start, edit.old_end, edit.new_start, edit.new_end,
           edit.is_insertion, edit.is_deletion, edit.is_replacement };
}

class LiveParser
{
public:
  LiveParser (Grammar &grammar, std::string_view text)
  {
    char error[256] = {};
    handle_.reset (glr_live_parser_create (grammar.handle (), text.data (),
                                            text.size (), error, sizeof error));
    if (!handle_)
      throw std::runtime_error (error[0] ? error : "glr_live_parser_create failed");
  }
  bool pending () const noexcept { return glr_live_parser_has_pending_edits (handle_.get ()); }
  std::string text () const
  {
    size_t length = 0;
    const char *value = glr_live_parser_text (handle_.get (), &length);
    return value ? std::string (value, length) : std::string ();
  }
  void set_text (std::string_view text)
  {
    char error[256] = {};
    if (glr_live_parser_set_text (handle_.get (), text.data (), text.size (),
                                  error, sizeof error)
        != 0)
      throw std::runtime_error (error[0] ? error : "glr_live_parser_set_text failed");
  }
  ParseTree forest () const noexcept
  {
    return ParseTree (const_cast<glr_forest_t *> (glr_live_parser_forest (handle_.get ())));
  }
  void edit (const glr_live_edit_t &edit)
  {
    char error[256] = {};
    if (glr_live_parser_edit (handle_.get (), &edit, error, sizeof error) != 0)
      throw std::runtime_error (error[0] ? error : "glr_live_parser_edit failed");
  }
  void update ()
  {
    char error[256] = {};
    if (glr_live_parser_update (handle_.get (), error, sizeof error) != 0)
      throw std::runtime_error (error[0] ? error : "glr_live_parser_update failed");
  }
  glr_live_parser_stats_t stats () const
  {
    glr_live_parser_stats_t value{};
    if (glr_live_parser_get_stats (handle_.get (), &value) != 0)
      throw std::runtime_error ("glr_live_parser_get_stats failed");
    return value;
  }
  const char *error () const noexcept { return glr_live_parser_error (handle_.get ()); }

private:
  using Handle = detail::ResourceHandle<glr_live_parser_t *, &glr_live_parser_destroy>;
  Handle handle_;
};

class LexerHooks
{
public:
  LexerHooks () : handle_ (glr_lexer_hooks_create ())
  {
    if (!handle_)
      throw std::runtime_error ("glr_lexer_hooks_create failed");
  }
  size_t size () const noexcept { return glr_lexer_hooks_count (handle_.get ()); }
  void clear () noexcept { glr_lexer_hooks_clear (handle_.get ()); }
  void user_data (void *data) noexcept { glr_lexer_hooks_set_user_data (handle_.get (), data); }
  void add (std::string_view name, int priority, glr_lexer_hook_fn fn,
            void *data = nullptr, glr_lexer_hook_destroy_fn destroy = nullptr)
  {
    if (glr_lexer_hooks_add (handle_.get (), std::string (name).c_str (), priority,
                             fn, data, destroy) != 0)
      throw std::runtime_error ("glr_lexer_hooks_add failed");
  }
  bool dispatch (const glr_lexer_event_t &event, glr_lexer_response_t &response) const noexcept
  {
    return glr_lexer_hooks_dispatch (handle_.get (), &event, &response);
  }
  glr_lexer_hooks_t *handle () const noexcept { return handle_.get (); }

private:
  using Handle = detail::ResourceHandle<glr_lexer_hooks_t *, &glr_lexer_hooks_destroy>;
  Handle handle_;
};

class Graph
{
public:
  Graph () : handle_ (glr_graph_create ())
  {
    if (!handle_)
      throw std::runtime_error ("glr_graph_create failed");
  }
  size_t node_count () const noexcept { return glr_graph_node_count (handle_.get ()); }
  size_t edge_count () const noexcept { return glr_graph_edge_count (handle_.get ()); }
  int add_node (void *data = nullptr)
  {
    int id = glr_graph_add_node (handle_.get (), data);
    if (id < 0)
      throw std::runtime_error ("glr_graph_add_node failed");
    return id;
  }
  glr_graph_edge_t *add_edge (size_t from, size_t to, int symbol)
  {
    auto *edge = glr_graph_add_edge (handle_.get (), from, to, symbol);
    if (!edge)
      throw std::runtime_error ("glr_graph_add_edge failed");
    return edge;
  }
  bool has_edge (size_t from, size_t to) const noexcept
  {
    return glr_graph_has_edge (handle_.get (), from, to);
  }
  void clear () noexcept { glr_graph_clear (handle_.get ()); }

private:
  using Handle = detail::ResourceHandle<glr_graph_t *, &glr_graph_destroy>;
  Handle handle_;
};

class Cache
{
public:
  explicit Cache (const glr_cache_config_t &config) : handle_ (glr_cache_open (&config))
  {
    if (!handle_)
      throw std::runtime_error ("glr_cache_open failed");
  }
  explicit Cache (std::string_view path)
  {
    auto config = GLR_CACHE_DEFAULT_CONFIG;
    std::string owned_path (path);
    config.mdbx_path = owned_path.c_str ();
    handle_.reset (glr_cache_open (&config));
    if (!handle_)
      throw std::runtime_error ("glr_cache_open failed");
  }
  int sync () noexcept { return glr_cache_sync (handle_.get ()); }
  size_t dependency_count () const noexcept
  {
    return glr_dependency_count (handle_.get ());
  }
  glr_cache_t *handle () const noexcept { return handle_.get (); }
  static std::array<uint8_t, 32> hash (std::string_view data)
  {
    std::array<uint8_t, 32> result{};
    glr_cache_compute_hash (reinterpret_cast<const uint8_t *> (data.data ()),
                            data.size (), result.data ());
    return result;
  }

private:
  using Handle = detail::ResourceHandle<glr_cache_t *, &glr_cache_close>;
  Handle handle_;
};

class Query
{
public:
  explicit Query (std::string_view source)
  {
    char error[256] = {};
    handle_.reset (glr_query_compile (source.data (), source.size (), nullptr,
                                      error, sizeof error));
    if (!handle_)
      throw std::runtime_error (error[0] ? error : "glr_query_compile failed");
  }
  size_t rule_count () const noexcept { return glr_query_rule_count (handle_.get ()); }
  std::string name () const
  {
    const char *value = glr_query_name (handle_.get ());
    return value ? std::string (value) : std::string ();
  }
  size_t run (const Grammar &grammar, const ParseTree &tree,
              std::string_view input = {}, void *user_data = nullptr) const
  {
    return glr_query_run (handle_.get (), grammar.handle (), tree.handle (),
                          nullptr, input.data (), input.size (), user_data,
                          nullptr);
  }

private:
  using Handle = detail::ResourceHandle<glr_query_t *, &glr_query_destroy>;
  Handle handle_;
};

class Ast
{
public:
  Ast () : handle_ (glr_ast_create ())
  {
    if (!handle_)
      throw std::runtime_error ("glr_ast_create failed");
  }
  explicit Ast (std::string_view source)
  {
    char error[256] = {};
    handle_.reset (glr_ast_from_sexp (source.data (), source.size (), error,
                                      sizeof error));
    if (!handle_)
      throw std::runtime_error (error[0] ? error : "glr_ast_from_sexp failed");
  }
  size_t node_count () const noexcept
  {
    return glr_ast_count_nodes (glr_ast_root (handle_.get ()));
  }
  std::string to_sexp () const
  {
    char *text = nullptr;
    size_t length = 0;
    if (glr_ast_to_sexp (glr_ast_root (handle_.get ()), &text, &length) != 0)
      throw std::runtime_error ("glr_ast_to_sexp failed");
    std::string result (text, length);
    std::free (text);
    return result;
  }
  glr_ast_t *handle () const noexcept { return handle_.get (); }

private:
  using Handle = detail::ResourceHandle<glr_ast_t *, &glr_ast_destroy>;
  Handle handle_;
};

namespace disambiguators
{
inline DisambiguationHook
by_precedence (int priority = 100)
{
  auto fn = [] (glr_disambig_context_t *ctx, size_t *winner,
                void *) -> glr_disambig_result_t
    {
      int best = INT_MIN;
      size_t idx = 0;
      bool found = false;
      for (size_t i = 0; i < ctx->candidate_count; ++i)
        {
          const auto &c = ctx->candidates[i];
          if (!c.rejected && c.precedence > best)
            {
              best = c.precedence;
              idx = i;
              found = true;
            }
        }
      if (!found)
        {
          return GLR_DISAMBIG_NO_MATCH;
        }
      *winner = idx;
      return GLR_DISAMBIG_RESOLVED;
    };
  return DisambiguationHook ("precedence", priority, fn);
}
inline DisambiguationHook
by_associativity (int priority = 90)
{
  auto fn = [] (glr_disambig_context_t *ctx, size_t *winner,
                void *) -> glr_disambig_result_t
    {
      for (size_t i = 0; i < ctx->candidate_count; ++i)
        {
          const auto &cand = ctx->candidates[i];
          if (cand.rejected)
            {
              continue;
            }
          if (cand.associativity == GLR_DISAMBIG_ASSOC_LEFT)
            {
              *winner = i;
              return GLR_DISAMBIG_RESOLVED;
            }
          if (cand.associativity == GLR_DISAMBIG_ASSOC_RIGHT)
            {
              *winner = ctx->candidate_count - 1 - i;
              return GLR_DISAMBIG_RESOLVED;
            }
        }
      return GLR_DISAMBIG_NO_MATCH;
    };
  return DisambiguationHook ("associativity", priority, fn);
}
inline DisambiguationHook
longest_match (int priority = 80)
{
  auto fn = [] (glr_disambig_context_t *ctx, size_t *winner,
                void *) -> glr_disambig_result_t
    {
      size_t max_len = 0;
      size_t best_idx = 0;
      bool found = false;
      for (size_t i = 0; i < ctx->candidate_count; ++i)
        {
          const auto &cand = ctx->candidates[i];
          if (cand.rejected)
            {
              continue;
            }
          size_t len = cand.end_position - cand.start_position;
          if (len > max_len)
            {
              max_len = len;
              best_idx = i;
              found = true;
            }
        }
      if (!found)
        {
          return GLR_DISAMBIG_NO_MATCH;
        }
      *winner = best_idx;
      return GLR_DISAMBIG_RESOLVED;
    };
  return DisambiguationHook ("longest_match", priority, fn);
}
} // namespace disambiguators

} // namespace glrpp

#endif
