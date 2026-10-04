/**
 * @file query-internal.h
 * @brief Declarations shared between querylisp.c and queryexec.c.
 *
 * The query layer is split so the S-expression syntax (querylisp.c) can be
 * read and changed without touching the behaviour (queryexec.c). This header
 * is the seam between the two; nothing here is public API.
 *
 * @internal
 */

#ifndef GLR_QUERY_INTERNAL_H
#define GLR_QUERY_INTERNAL_H

#include <glr/query.h>

#include "../../third_party/sfsexp/src/sexp.h"

/* Typed variants of the public element helpers, so internal code does not
   cast through void* at every call site. */
const char *glr_query_atom_text_of (const sexp_t *element);
bool glr_query_is_atom_of (const sexp_t *element, const char *name);
bool glr_query_is_list_of (const sexp_t *element, const char *name);

/* A compiled pattern, opaque outside querylisp.c. */
struct glr_query_pattern_t;

/* Rule accessors, so queryexec.c can run a rule without seeing how it is
   stored. All are NULL-safe. */
const struct glr_query_pattern_t *glr_query_rule_node (const glr_query_t *query,
                                                      size_t rule_index);
bool glr_query_rule_matches (const glr_query_t *query, size_t rule_index,
                             const glr_grammar_t *grammar,
                             const glr_forest_node_t *node,
                             const glr_forest_node_t **out_bindings,
                             const char **out_names, size_t binding_capacity,
                             size_t *out_count);
size_t glr_query_rule_action_count (const glr_query_t *query,
                                    size_t rule_index);
const glr_query_action_t *glr_query_rule_action (const glr_query_t *query,
                                                 size_t rule_index,
                                                 size_t action_index);
const char *const *glr_query_rule_action_args (const glr_query_t *query,
                                               size_t rule_index,
                                               size_t action_index);
size_t glr_query_rule_action_arg_count (const glr_query_t *query,
                                         size_t rule_index,
                                         size_t action_index);

/* Report an error into a caller-supplied buffer; shared with querylisp.c. */
void glr_query_set_error (char *buffer, size_t size, const char *message);

/* AST node plumbing shared between the two files. */
glr_ast_node_t *glr_ast_node_new (const char *name, const char *value,
                                  size_t start, size_t end);
void glr_ast_node_free (glr_ast_node_t *node);
int glr_ast_node_add_child (glr_ast_node_t *parent, glr_ast_node_t *child);

/* The walk entry point used by glr_query_run_ex(). */
size_t glr_query_walk (glr_query_t *query, const glr_grammar_t *grammar,
                       glr_forest_t *forest, const glr_forest_node_t *root,
                       const char *input, size_t input_length,
                       const glr_query_options_t *options,
                       glr_query_stats_t *stats);

#endif /* GLR_QUERY_INTERNAL_H */
