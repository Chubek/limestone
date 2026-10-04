%module libglr

%{
#include <glr/glr.h>
#include <glr/disambiguate.h>
#include <glr/forest.h>
#include <glr/fork.h>
#include <glr/grammar.h>
#include <glr/graph.h>
#include <glr/lexer-hooks.h>
#include <glr/parser.h>
#include <glr/reader.h>
#include <glr/reduction.h>
#ifdef HAVE_LMDB
#include <glr/cache.h>
#endif
#include <glr/rewrite.h>
#include <glr/stack.h>
%}

%include <stdint.i>
%include <exception.i>

%define GLR_SWIG_LANG_NOTE(TEXT)
/* TEXT */
%enddef

#ifdef SWIGPYTHON
%feature("autodoc", "1");
GLR_SWIG_LANG_NOTE("Python bindings keep raw C names so higher-level wrappers can layer on top.")
#endif

#ifdef SWIGRUBY
GLR_SWIG_LANG_NOTE("Ruby wrappers rely on helper accessors instead of direct struct-array field access.")
#endif

#ifdef SWIGJAVA
%pragma(java) jniclasscode=%{
  static {
    try {
      System.loadLibrary("libglr");
    } catch (UnsatisfiedLinkError ex) {
      // Wrapper projects may load the shared library themselves.
    }
  }
%}
#endif

#ifdef SWIGCSHARP
%pragma(csharp) moduleimports=%{
using System;
using System.Runtime.InteropServices;
%}
#endif

/* Keep internal pointer arrays and unions behind explicit helper functions. */
%ignore glr_grammar_t::symbols;
%ignore glr_grammar_t::productions;
%ignore glr_production_t::body;
%ignore glr_forest_node_t::children;
%ignore glr_graph_node_t::edges_out;
%ignore glr_graph_node_t::edges_in;
%ignore glr_graph_t::nodes;
%ignore glr_graph_t::edges;
%ignore glr_graph_edge_t::from;
%ignore glr_graph_edge_t::to;
%ignore glr_item_set_t::items;
%ignore glr_parser::stacks;
%ignore glr_parser::state_table;
%ignore glr_parser::disambig_hooks;
%ignore glr_parser::input;
%ignore glr_parser::reader;
%ignore glr_parser::lexer_hooks;
%ignore glr_rewrite_rule_t::data;

%newobject glr_grammar_create;
%newobject glr_forest_create;
%newobject glr_fork_create;
%newobject glr_graph_create;
%newobject glr_item_set_create;
%newobject glr_reduction_create;
%newobject glr_parser_create;
%newobject glr_reader_create;
%newobject glr_lexer_hooks_create;

#ifdef HAVE_LMDB
%newobject glr_cache_open;
#endif

%newobject glr_rewrite_program_parse;
%newobject glr_rewrite_program_load_file;
%newobject glr_disambig_hook_create;
%newobject glr_disambig_precedence_hook_create;
%newobject glr_disambig_associativity_hook_create;
%newobject glr_disambig_semantic_hook_create;
%newobject glr_disambig_dynamic_programming_hook_create;
%newobject glr_disambig_probability_hook_create;
%newobject glr_disambig_predicate_hook_create;

%inline %{
static const char *
glr_binding_bool_string (bool value)
{
  return value ? "true" : "false";
}

static size_t
glr_binding_grammar_symbol_count (const glr_grammar_t *grammar)
{
  return grammar != NULL ? grammar->symbol_count : 0;
}

static size_t
glr_binding_grammar_production_count (const glr_grammar_t *grammar)
{
  return grammar != NULL ? grammar->production_count : 0;
}

static glr_symbol_t *
glr_binding_grammar_symbol_at (const glr_grammar_t *grammar, size_t index)
{
  if (grammar == NULL || index >= grammar->symbol_count)
    {
      return NULL;
    }
  return grammar->symbols[index];
}

static glr_production_t *
glr_binding_grammar_production_at (const glr_grammar_t *grammar, size_t index)
{
  if (grammar == NULL || index >= grammar->production_count)
    {
      return NULL;
    }
  return grammar->productions[index];
}

static const char *
glr_binding_symbol_name (const glr_symbol_t *symbol)
{
  return symbol != NULL ? symbol->name : NULL;
}

static size_t
glr_binding_production_body_length (const glr_production_t *production)
{
  return production != NULL ? production->body_length : 0;
}

static glr_symbol_t *
glr_binding_production_body_symbol_at (const glr_production_t *production,
                                       size_t index)
{
  if (production == NULL || index >= production->body_length)
    {
      return NULL;
    }
  return production->body[index];
}

static size_t
glr_binding_forest_child_count (const glr_forest_node_t *node)
{
  return node != NULL ? node->child_count : 0;
}

static glr_forest_node_t *
glr_binding_forest_child_at (const glr_forest_node_t *node, size_t index)
{
  if (node == NULL || index >= node->child_count)
    {
      return NULL;
    }
  return node->children[index];
}

static size_t
glr_binding_graph_node_count (const glr_graph_t *graph)
{
  return graph != NULL ? graph->node_count : 0;
}

static size_t
glr_binding_graph_edge_count (const glr_graph_t *graph)
{
  return graph != NULL ? graph->edge_count : 0;
}

static glr_graph_node_t *
glr_binding_graph_node_at (const glr_graph_t *graph, size_t index)
{
  if (graph == NULL || index >= graph->node_count)
    {
      return NULL;
    }
  return graph->nodes[index];
}

static glr_graph_edge_t *
glr_binding_graph_edge_at (const glr_graph_t *graph, size_t index)
{
  if (graph == NULL || index >= graph->edge_count)
    {
      return NULL;
    }
  return graph->edges[index];
}

static size_t
glr_binding_graph_node_out_degree (const glr_graph_node_t *node)
{
  return node != NULL ? node->edge_out_count : 0;
}

static size_t
glr_binding_graph_node_in_degree (const glr_graph_node_t *node)
{
  return node != NULL ? node->edge_in_count : 0;
}

static glr_graph_edge_t *
glr_binding_graph_node_out_edge_at (const glr_graph_node_t *node, size_t index)
{
  if (node == NULL || index >= node->edge_out_count)
    {
      return NULL;
    }
  return node->edges_out[index];
}

static glr_graph_edge_t *
glr_binding_graph_node_in_edge_at (const glr_graph_node_t *node, size_t index)
{
  if (node == NULL || index >= node->edge_in_count)
    {
      return NULL;
    }
  return node->edges_in[index];
}

static glr_graph_node_t *
glr_binding_graph_edge_from_node (const glr_graph_edge_t *edge)
{
  return edge != NULL ? edge->from : NULL;
}

static glr_graph_node_t *
glr_binding_graph_edge_to_node (const glr_graph_edge_t *edge)
{
  return edge != NULL ? edge->to : NULL;
}

static size_t
glr_binding_item_set_count (const glr_item_set_t *set)
{
  return set != NULL ? set->item_count : 0;
}

static glr_item_t *
glr_binding_item_set_item_at (const glr_item_set_t *set, size_t index)
{
  if (set == NULL || index >= set->item_count)
    {
      return NULL;
    }
  return set->items[index];
}

static const char *
glr_binding_reader_token_name (const glr_reader_token_t *token)
{
  return token != NULL ? token->terminal_name : NULL;
}

static const char *
glr_binding_parse_error_string (glr_parse_error_t error)
{
  switch (error)
    {
    case GLR_PARSE_SUCCESS:
      return "success";
    case GLR_PARSE_ERROR_SYNTAX:
      return "syntax";
    case GLR_PARSE_ERROR_MEMORY:
      return "memory";
    case GLR_PARSE_ERROR_GRAMMAR:
      return "grammar";
    case GLR_PARSE_ERROR_UNRECOVERABLE:
      return "unrecoverable";
    }
  return "unknown";
}

static const char *
glr_binding_rewrite_status_string (glr_rewrite_status_t status)
{
  switch (status)
    {
    case GLR_REWRITE_STATUS_OK:
      return "ok";
    case GLR_REWRITE_STATUS_INVALID_ARGUMENT:
      return "invalid-argument";
    case GLR_REWRITE_STATUS_PARSE_ERROR:
      return "parse-error";
    case GLR_REWRITE_STATUS_MEMORY_ERROR:
      return "memory-error";
    case GLR_REWRITE_STATUS_NOT_FOUND:
      return "not-found";
    case GLR_REWRITE_STATUS_CONFLICT:
      return "conflict";
    case GLR_REWRITE_STATUS_UNSUPPORTED:
      return "unsupported";
    }
  return "unknown";
}

static const char *
glr_binding_disambig_result_string (glr_disambig_result_t result)
{
  switch (result)
    {
    case GLR_DISAMBIG_NO_MATCH:
      return "no-match";
    case GLR_DISAMBIG_RESOLVED:
      return "resolved";
    case GLR_DISAMBIG_ERROR:
      return "error";
    }
  return "unknown";
}

static const char *
glr_binding_reader_status_name (glr_reader_status_t status)
{
  return glr_reader_status_string (status);
}

static glr_rewrite_status_t
glr_binding_rewrite_program_add_add_symbol (glr_rewrite_program_t *program,
                                            glr_symbol_type_t type,
                                            const char *name)
{
  glr_rewrite_rule_t rule;

  if (program == NULL || name == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  memset (&rule, 0, sizeof (rule));
  rule.kind = GLR_REWRITE_RULE_ADD_SYMBOL;
  rule.data.symbol.type = type;
  rule.data.symbol.name = (char *)name;
  return glr_rewrite_program_add_rule (program, &rule);
}

static glr_rewrite_status_t
glr_binding_rewrite_program_add_drop_symbol (glr_rewrite_program_t *program,
                                             const char *name)
{
  glr_rewrite_rule_t rule;

  if (program == NULL || name == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  memset (&rule, 0, sizeof (rule));
  rule.kind = GLR_REWRITE_RULE_DROP_SYMBOL;
  rule.data.symbol.name = (char *)name;
  return glr_rewrite_program_add_rule (program, &rule);
}

static glr_rewrite_status_t
glr_binding_rewrite_program_add_rename_symbol (glr_rewrite_program_t *program,
                                               const char *old_name,
                                               const char *new_name)
{
  glr_rewrite_rule_t rule;

  if (program == NULL || old_name == NULL || new_name == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  memset (&rule, 0, sizeof (rule));
  rule.kind = GLR_REWRITE_RULE_RENAME_SYMBOL;
  rule.data.rename_symbol.old_name = (char *)old_name;
  rule.data.rename_symbol.new_name = (char *)new_name;
  return glr_rewrite_program_add_rule (program, &rule);
}

static glr_rewrite_status_t
glr_binding_rewrite_program_add_set_start (glr_rewrite_program_t *program,
                                           const char *name)
{
  glr_rewrite_rule_t rule;

  if (program == NULL || name == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  memset (&rule, 0, sizeof (rule));
  rule.kind = GLR_REWRITE_RULE_SET_START;
  rule.data.start_symbol_name = (char *)name;
  return glr_rewrite_program_add_rule (program, &rule);
}

static glr_rewrite_status_t
glr_binding_rewrite_program_add_rule_kind (glr_rewrite_program_t *program,
                                           glr_rewrite_rule_kind_t kind)
{
  glr_rewrite_rule_t rule;

  if (program == NULL)
    {
      return GLR_REWRITE_STATUS_INVALID_ARGUMENT;
    }

  memset (&rule, 0, sizeof (rule));
  rule.kind = kind;
  return glr_rewrite_program_add_rule (program, &rule);
}
%}


#ifdef HAVE_LMDB
/* Cache helper functions */
static glr_cache_config_t
glr_binding_cache_config_create (const char *mdbx_path,
                                 size_t map_size,
                                 uint32_t max_readers,
                                 bool use_async,
                                 uint64_t ttl_seconds)
{
  glr_cache_config_t config;
  config.mdbx_path = mdbx_path;
  config.map_size = map_size;
  config.max_readers = max_readers;
  config.use_async = use_async;
  config.ttl_seconds = ttl_seconds;
  return config;
}

static glr_cache_config_t
glr_binding_cache_config_default (void)
{
  return GLR_CACHE_DEFAULT_CONFIG;
}

static double
glr_binding_cache_stats_hit_rate (const glr_cache_stats_t *stats)
{
  if (stats == NULL)
    {
      return 0.0;
    }
  
  uint64_t total_hits = stats->forest_hits + stats->gss_hits + stats->subtree_hits;
  uint64_t total_misses = stats->forest_misses + stats->gss_misses + stats->subtree_misses;
  uint64_t total = total_hits + total_misses;
  
  return total > 0 ? (double)total_hits / (double)total : 0.0;
}
#endif /* HAVE_LMDB */
/* Public headers. */
%include "glr/grammar.h"
%include "glr/forest.h"
%include "glr/fork.h"
%include "glr/graph.h"
%include "glr/reduction.h"
%include "glr/lexer-hooks.h"
%include "glr/reader.h"
%include "glr/disambiguate.h"
%include "glr/rewrite.h"
%include "glr/parser.h"
%include "glr/live-parsing.h"
%include "glr/query.h"
%include "glr/glr.h"

#ifdef HAVE_LMDB
/* Cache API - only available when LMDB is enabled */
%include "glr/cache.h"
#endif
