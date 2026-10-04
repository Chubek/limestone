#ifndef GLR_QUERY_H
#define GLR_QUERY_H

#include <glr/disambiguate.h>
#include <glr/forest.h>
#include <glr/grammar.h>
#include <glr/parser.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file query.h
 * @brief S-expression queries over a parse forest, and AST building with them.
 *
 * A query is a list of rules. Each rule pairs a pattern, matched against the
 * parse forest, with an action to run on every match:
 *
 * @code
 * (query
 *   (name "extract-operands")
 *   (rules
 *     (match (constructor _ (terminal @op) (nonterminal _))
 *            (print-symbol @op))))
 * @endcode
 *
 * Patterns are S-expressions, parsed with the same third_party/sfsexp reader
 * the rewrite language uses. A pattern is a node shape:
 *
 * - `(terminal SYMBOL)` matches a terminal with that symbol name.
 * - `(nonterminal SYMBOL)` matches a non-terminal node.
 * - `(constructor PRODUCTION)` matches a constructor node.
 * - `_` as a symbol matches any node of the expected kind.
 * - `(any SYMBOL)` matches a node of any kind with that symbol.
 * - `$NAME` or `@NAME` binds the matched node to @c NAME, which an action can
 *   then refer to by passing @c NAME as one of its arguments.
 *
 * Nested lists are patterns for the children, and they match **in order**: each
 * nested pattern takes the next child that fits, and every nested pattern has to
 * be taken. So `(constructor _ (terminal @op) (nonterminal _))` matches a node
 * with an operator followed by a non-terminal, not one that merely has both.
 *
 * Actions are looked up by name in a @ref glr_query_actions table, so an
 * application adds its own without touching the library. The builtin table is
 * @ref glr_global_query_actions, defined in queryexec.c, and covers
 * extraction, rewriting, disambiguation, AST building, IPC, signalling, and
 * exiting. @ref glr_query_register_builtin_style_actions is a convenience for
 * the common case of running one builtin set.
 *
 * AST building is the most-used action set. An AST is described in
 * S-expression form, either inline in the query or loaded from a file:
 *
 * @code
 * (ast
 *   (node Expr
 *     (child @left)
 *     (child (operator @op @right))))
 * @endcode
 *
 * The `@node()` and `@leaf()` actions build a tree of @ref glr_ast_node_t out
 * of matches, and the result can be written back as S-expressions with
 * @ref glr_ast_to_sexp or read back with @ref glr_ast_from_sexp.
 */

/**
 * @brief An opaque query program.
 *
 * Owns the parsed rules and the action table. Build with
 * @ref glr_query_compile and release with @ref glr_query_destroy.
 */
typedef struct glr_query_t glr_query_t;

/**
 * @brief An opaque AST definition.
 *
 * Produced by @ref glr_query_ast_from_sexp or
 * @ref glr_query_ast_load_file, and used by the `@node()` action to name the
 * nodes an AST is allowed to contain.
 */
typedef struct glr_ast_def_t glr_ast_def_t;

/**
 * @brief A node of a built AST.
 *
 * Unlike the SPPF, an AST is a plain tree: a node either has children or
 * carries a leaf value, never both. The value is owned by the node.
 */
typedef struct glr_ast_node_t
{
    char *name;        ///< Node kind, e.g. "Expr"; NULL for a value node. */
    char *value;       ///< Leaf text, e.g. "n"; NULL for a structural node. */
    size_t start;      ///< Start offset of the matched text in the input. */
    size_t end;        ///< End offset of the matched text in the input. */
    struct glr_ast_node_t **children; ///< Child nodes, or NULL. */
    size_t child_count;                ///< Number of children. */
    size_t capacity;                   ///< Allocated child capacity. */
} glr_ast_node_t;

/**
 * @brief An AST under construction.
 *
 * Created by @ref glr_ast_create or @ref glr_ast_from_sexp, and filled by the
 * AST-building actions. Release with @ref glr_ast_destroy.
 */
typedef struct glr_ast_t glr_ast_t;

/**
 * @brief One matched pattern, as seen by an action.
 *
 * Bindings are the nodes the pattern named with `$NAME`; `matched` is the node
 * the whole pattern matched.
 */
typedef struct
{
    const glr_forest_node_t *matched;      ///< Node the pattern matched. */
    const glr_forest_node_t **bindings;    ///< Bound nodes, in pattern order. */
    const char **binding_names;            ///< Names parallel to @c bindings. */
    size_t binding_count;                  ///< Number of bindings. */
    const char *text;                      ///< Matched text, never NULL. */
    size_t text_length;                    ///< Length of @c text in bytes. */
    size_t start;                          ///< Start offset of the match. */
    size_t end;                            ///< End offset of the match. */
} glr_query_match_t;

/**
 * @brief Sink an action uses to report something to the application.
 *
 * Extraction, printing and IPC actions all funnel through one callback so the
 * application decides what "report" means: print to stdout, append to a file,
 * push onto a protocol buffer, or write to a pipe.
 *
 * @param text Reported text; not NUL-terminated and only valid for the call.
 * @param length Length of @p text in bytes.
 * @param user_data Context supplied with the run options.
 */
typedef void (*glr_query_report_fn) (const char *text, size_t length,
                                     void *user_data);

/**
 * @brief Context handed to every action.
 *
 * An action may read the input, the grammar and the forest, and may also
 * mutate them: that is how rewriting and disambiguation actions work. The
 * session owns the memory; nothing in it should be freed by an action.
 */
typedef struct
{
    const glr_grammar_t *grammar;        ///< Grammar the forest was built with. */
    glr_grammar_t *mutable_grammar;      ///< Grammar to rewrite; NULL unless the caller offered one. */
    glr_forest_t *forest;                ///< Forest being queried. */
    const glr_forest_node_t *root;       ///< Root the walk started from, may be NULL. */
    const char *input;                   ///< Source text the forest was built from. */
    size_t input_length;                 ///< Length of @c input in bytes. */
    glr_query_t *query;                  ///< Query being executed. */
    void *user_data;                     ///< Context supplied by the caller. */
    size_t match_index;                  ///< Zero-based index of this match. */
    glr_ast_t *ast;                      ///< AST under construction, may be NULL. */
    glr_query_report_fn report;          ///< Reporting sink, may be NULL. */
    void *report_data;                   ///< Context for @c report. */
    size_t report_count;                 ///< Reports made so far in this run. */
    bool exit_requested;                 ///< Set by the `(@exit ...)` action. */
    int exit_code;                       ///< Code carried by an exit request. */
    const glr_forest_node_t **rejected;  ///< Nodes the `(@reject)` action dropped. */
    size_t rejected_count;               ///< Number of rejected nodes. */
    bool halted;                         ///< Set by an action to stop the walk. */
} glr_query_context_t;

/**
 * @brief Result of running an action.
 *
 * Zero keeps walking. GLR_QUERY_ACTION_STOP ends the walk over the current
 * root, which is how `@halt()` and a failing `(@exit code)` behave.
 */
typedef enum
{
    GLR_QUERY_ACTION_CONTINUE = 0, /**< Keep matching and walking. */
    GLR_QUERY_ACTION_STOP = 1       /**< Stop walking this root. */
} glr_query_action_result_t;

/**
 * @brief An action invoked for every match of the rule it belongs to.
 *
 * @param context Query context; never NULL.
 * @param match The match; never NULL.
 * @param args The action's arguments, exactly as written in the query; never NULL.
 * @param arg_count Number of arguments; may be 0.
 * @param user_data Action data registered with the action.
 * @return GLR_QUERY_ACTION_CONTINUE or GLR_QUERY_ACTION_STOP.
 */
typedef glr_query_action_result_t (*glr_query_action_fn) (
    glr_query_context_t *context, const glr_query_match_t *match,
    const char *const *args, size_t arg_count, void *user_data);

/**
 * @brief A table of actions addressable by name.
 *
 * The builtin table is @ref glr_global_query_actions. An application adds its
 * own with @ref glr_query_actions_register; a query resolves action names
 * against its own table first, then against the globals, so an application can
 * shadow a builtin when it needs different behaviour.
 */
typedef struct
{
    const char *name;        /**< Action name as written in a query; NULL ends the table. */
    glr_query_action_fn action; /**< Implementation, or NULL when @c user_data carries it. */
    void *user_data;         /**< Action data passed to @c action. */
    void (*destroy) (void *user_data); /**< Called when the table is destroyed, may be NULL. */
    const char *summary;     /**< One-line description, may be NULL. */
} glr_query_action_t;

/**
 * @brief The builtin action table.
 *
 * Defined in queryexec.c and never NULL, so a program that only wants the
 * builtins does not have to build a table at all. Applications must not modify
 * it; use @ref glr_query_actions_register on a local table instead.
 */
extern const glr_query_action_t glr_global_query_actions[];

/**
 * @brief Outcome of a query run.
 */
typedef struct
{
    size_t rules;            /**< Rules the query contained. */
    size_t matches;          /**< Matches fired across all rules. */
    size_t actions_run;      ///< Action invocations that completed. */
    size_t nodes_visited;    ///< Forest nodes examined. */
    size_t errors;           ///< Action invocations that failed. */
} glr_query_stats_t;

/* ========================================================================
 * Programs
 * ====================================================================== */

/**
 * @brief Compile a query from S-expression text.
 *
 * The text is a `(query (name "...") (rules (RULE ...) ...))` form. The
 * `name` and `rules` wrappers are optional: a bare list of rules is accepted
 * too.
 *
 * @param source S-expression text (required).
 * @param length Length of @p source in bytes.
 * @param actions Action table used to resolve names, or NULL for the builtins.
 * @param error Optional error buffer; receives a message on failure.
 * @param error_size Size of @p error in bytes.
 * @return New query, or NULL on a syntax error, an unknown action, or OOM.
 */
glr_query_t *glr_query_compile (const char *source, size_t length,
                                const glr_query_action_t *actions,
                                char *error, size_t error_size);

/**
 * @brief Destroy a query and everything it owns.
 * @param query Query to destroy (NULL is a no-op).
 */
void glr_query_destroy (glr_query_t *query);

/**
 * @brief Get the name a query was compiled with.
 * @param query Query (may be NULL).
 * @return Borrowed name, or NULL when the query is unnamed.
 */
const char *glr_query_name (const glr_query_t *query);

/**
 * @brief Count the rules in a query.
 * @param query Query (may be NULL).
 * @return Rule count, or 0.
 */
size_t glr_query_rule_count (const glr_query_t *query);

/* ========================================================================
 * Action tables
 * ====================================================================== */

/**
 * @brief Register an action in a caller-owned table.
 *
 * The table must have room for one more entry; a NULL-terminated table with
 * spare capacity is the usual shape. @p destroy is called when the owning
 * query is destroyed, so an action can own heap data.
 *
 * @param table Table to extend (required).
 * @param capacity Number of entries @p table can hold.
 * @param name Action name as queries will spell it (required, non-empty).
 * @param action Implementation, or NULL to use @p user_data as the function.
 * @param user_data Data passed to @p action.
 * @param destroy Called on @p user_data at destruction; may be NULL.
 * @param summary One-line description; may be NULL.
 * @return 0 on success, -1 on invalid input or a full table.
 */
int glr_query_actions_register (glr_query_action_t *table, size_t capacity,
                                const char *name,
                                glr_query_action_fn action, void *user_data,
                                void (*destroy) (void *user_data),
                                const char *summary);

/**
 * @brief Find an action by name.
 * @param table Table to search (may be NULL).
 * @param name Name to find (required).
 * @return Borrowed table entry, or NULL when the name is unknown.
 */
const glr_query_action_t *glr_query_actions_find (const glr_query_action_t *table,
                                                 const char *name);

/**
 * @brief Look up an action the way a query does: local table first, then the
 *        builtins.
 * @param local Local table, or NULL.
 * @param name Action name (required).
 * @return Borrowed table entry, or NULL when the name is unknown.
 */
const glr_query_action_t *glr_query_resolve_action (
    const glr_query_action_t *local, const char *name);

/**
 * @brief Count the entries in a NULL-terminated action table.
 * @param table Table to count (may be NULL).
 * @return Number of entries.
 */
size_t glr_query_actions_count (const glr_query_action_t *table);

/* ========================================================================
 * Running queries
 * ====================================================================== */

/**
 * @brief Run a query over a parse forest.
 *
 * Walks the forest once and applies every rule to every node it reaches, so a
 * query with several rules costs one traversal rather than one per rule.
 *
 * @param query Compiled query (required).
 * @param grammar Grammar the forest was built with (required).
 * @param forest Forest to query (required).
 * @param root Node to start from, or NULL for the whole forest.
 * @param input Source text (may be NULL, in which case matches have no text).
 * @param input_length Length of @p input in bytes.
 * @param user_data Context passed to actions through @c glr_query_context_t.
 * @param stats Optional counters; may be NULL.
 * @return Number of matches fired, or 0 on invalid input.
 */
size_t glr_query_run (glr_query_t *query, const glr_grammar_t *grammar,
                      glr_forest_t *forest, const glr_forest_node_t *root,
                      const char *input, size_t input_length, void *user_data,
                      glr_query_stats_t *stats);

/**
 * @brief Optional per-run configuration.
 */
typedef struct
{
    void *user_data;              ///< Passed to actions and to @c report. */
    glr_query_report_fn report;   /**< Reporting sink; may be NULL. */
    glr_ast_t *ast;               /**< AST to fill; the run does not own it. */
    glr_grammar_t *mutable_grammar; /**< Grammar a rewriting action may change. */
} glr_query_options_t;

/**
 * @brief Run a query with explicit options.
 *
 * Same walk as @ref glr_query_run, with the reporting sink and the AST to fill
 * under the caller's control.
 *
 * @param query Compiled query (required).
 * @param grammar Grammar the forest was built with (required).
 * @param forest Forest to query (required).
 * @param root Node to start from, or NULL for the whole forest.
 * @param input Source text (may be NULL).
 * @param input_length Length of @p input in bytes.
 * Rewriting actions such as @c @rename only run when
 * @ref glr_query_options_t::mutable_grammar is set: the grammar handed to this
 * function is const, and a query that silently cast it away would be changing
 * a caller's grammar behind its back.
 *
 * @param options Optional configuration; may be NULL.
 * @param stats Optional counters; may be NULL.
 * @return Number of matches fired, or 0 on invalid input.
 */
size_t glr_query_run_ex (glr_query_t *query, const glr_grammar_t *grammar,
                         glr_forest_t *forest, const glr_forest_node_t *root,
                         const char *input, size_t input_length,
                         const glr_query_options_t *options,
                         glr_query_stats_t *stats);

/**
 * @brief Get the AST definition attached to a query.
 * @param query Query (may be NULL).
 * @return Borrowed definition, or NULL when the query has none.
 */
const glr_ast_def_t *glr_query_ast (const glr_query_t *query);

/**
 * @brief Attach an AST definition to a query, replacing any previous one.
 * @param query Query (required).
 * @param ast Definition to adopt; may be NULL. The query takes ownership.
 */
void glr_query_set_ast (glr_query_t *query, glr_ast_def_t *ast);

/**
 * @brief Text of an S-expression atom.
 * @param element Element to inspect (may be NULL).
 * @return Atom text, or NULL when the element is a list.
 */
const char *glr_query_atom_text (const void *element);

/**
 * @brief Whether an element is the atom @p name.
 * @param element Element to inspect (may be NULL).
 * @param name Atom to compare against (required).
 * @return true on a match.
 */
bool glr_query_is_atom (const void *element, const char *name);

/**
 * @brief Whether an element is a list headed by @p name.
 * @param element Element to inspect (may be NULL).
 * @param name Head to compare against (required).
 * @return true on a match.
 */
bool glr_query_is_list (const void *element, const char *name);

/**
 * @brief Whether a node was rejected during a run.
 *
 * The `(@reject)` action records the matched node here instead of reaching
 * into the parser, so an application that wants to drop a reading asks the
 * query what it rejected and acts on that.
 *
 * @param context Context from a completed run; may be NULL.
 * @param node Node to test (may be NULL).
 * @return true when the node was rejected.
 */
bool glr_query_was_rejected (const glr_query_context_t *context,
                             const glr_forest_node_t *node);

/**
 * @brief Count the nodes a pattern would match, without running actions.
 *
 * Useful for checking a pattern before wiring it to an action, and as the
 * cheapest way to find the matches of a query.
 *
 * @param query Compiled query (required).
 * @param rule_index Rule to count against.
 * @param grammar Grammar the forest was built with (required).
 * @param forest Forest the root belongs to (required).
 * @param root Node to search below, or NULL to search the whole forest.
 * @return Number of matches, or 0 for an invalid rule index.
 *
 * @note Each node is counted once even when several paths reach it, which is
 *       what the executor's walk does.
 */
size_t glr_query_count_matches (const glr_query_t *query, size_t rule_index,
                                const glr_grammar_t *grammar,
                                const glr_forest_t *forest,
                                const glr_forest_node_t *root);

/* ========================================================================
 * Patterns
 * ====================================================================== */

/**
 * @brief Compile a standalone pattern, mostly for testing and tooling.
 *
 * @param source S-expression text describing one pattern (required).
 * @param length Length of @p source in bytes.
 * @param error Optional error buffer.
 * @param error_size Size of @p error in bytes.
 * @return Opaque pattern, or NULL on failure. Release with
 *         @ref glr_query_pattern_destroy.
 */
typedef struct glr_query_pattern_t glr_query_pattern_t;

glr_query_pattern_t *glr_query_pattern_compile (const char *source,
                                                size_t length, char *error,
                                                size_t error_size);

/**
 * @brief Destroy a compiled pattern.
 * @param pattern Pattern to destroy (NULL is a no-op).
 */
void glr_query_pattern_destroy (glr_query_pattern_t *pattern);

/**
 * @brief Test a pattern against one node, filling in its bindings.
 *
 * @param pattern Compiled pattern (required).
 * @param grammar Grammar the node was built with (required).
 * @param node Node to test (required).
 * @param out_bindings Receives the bound nodes, in pattern order; may be NULL.
 * @param binding_capacity Number of entries @p out_bindings can hold.
 * @param out_count Receives the number of bindings; may be NULL.
 * @return true when the pattern matches.
 */
bool glr_query_pattern_match (const glr_query_pattern_t *pattern,
                              const glr_grammar_t *grammar,
                              const glr_forest_node_t *node,
                              const glr_forest_node_t **out_bindings,
                              size_t binding_capacity, size_t *out_count);

/* ========================================================================
 * ASTs
 * ====================================================================== */

/**
 * @brief Parse an AST definition from S-expression text.
 *
 * A definition maps AST node names to the query pattern that produces them:
 *
 * @code
 * (ast
 *   (node Expr (child @left) (child (operator @op @right)))
 *   (node operator (field "+") (field @right)))
 * @endcode
 *
 * @param source S-expression text (required).
 * @param length Length of @p source in bytes.
 * @param error Optional error buffer.
 * @param error_size Size of @p error in bytes.
 * @return New definition, or NULL on failure.
 */
glr_ast_def_t *glr_query_ast_from_sexp (const char *source, size_t length,
                                        char *error, size_t error_size);

/**
 * @brief Load an AST definition from a file.
 * @param path Path to a file holding S-expression text (required).
 * @param error Optional error buffer.
 * @param error_size Size of @p error in bytes.
 * @return New definition, or NULL on failure.
 */
glr_ast_def_t *glr_query_ast_load_file (const char *path, char *error,
                                        size_t error_size);

/**
 * @brief Destroy an AST definition.
 * @param definition Definition to destroy (NULL is a no-op).
 */
void glr_query_ast_destroy (glr_ast_def_t *definition);

/**
 * @brief Look up the rule that builds an AST node kind.
 * @param definition Definition (may be NULL).
 * @param name Node kind (required).
 * @return Borrowed pattern text, or NULL when the kind is not defined.
 */
const char *glr_query_ast_rule (const glr_ast_def_t *definition,
                                const char *name);

/**
 * @brief List the node kinds an AST definition declares.
 * @param definition Definition (may be NULL).
 * @param out_names Receives borrowed names; may be NULL to only count.
 * @param capacity Number of entries @p out_names can hold.
 * @return Number of node kinds.
 */
size_t glr_query_ast_node_kinds (const glr_ast_def_t *definition,
                                 const char **out_names, size_t capacity);

/**
 * @brief Create an empty AST.
 * @return New AST, or NULL on OOM.
 */
glr_ast_t *glr_ast_create (void);

/**
 * @brief Destroy an AST and every node in it.
 * @param ast AST to destroy (NULL is a no-op).
 */
void glr_ast_destroy (glr_ast_t *ast);

/**
 * @brief Get the root of an AST.
 * @param ast AST (may be NULL).
 * @return Borrowed root node, or NULL when the AST is empty.
 */
const glr_ast_node_t *glr_ast_root (const glr_ast_t *ast);

/**
 * @brief Replace the root of an AST.
 * @param ast AST (required).
 * @param node Node to adopt (required; the AST takes ownership).
 * @return 0 on success, -1 on invalid input.
 */
int glr_ast_set_root (glr_ast_t *ast, glr_ast_node_t *node);

/**
 * @brief Count the nodes in an AST.
 * @param node Node to count from (may be NULL).
 * @return Node count, or 0.
 */
size_t glr_ast_count_nodes (const glr_ast_node_t *node);

/**
 * @brief Find the first node of a given kind in an AST.
 * @param node Node to search from (may be NULL).
 * @param name Kind to look for (required).
 * @return Borrowed node, or NULL.
 */
const glr_ast_node_t *glr_ast_find (const glr_ast_node_t *node,
                                    const char *name);

/**
 * @brief Render an AST as S-expression text.
 *
 * The result is allocated by the function and NUL-terminated; the caller frees
 * it. Round-trips through @ref glr_ast_from_sexp.
 *
 * @param node Node to render (required).
 * @param out Receives the allocated text.
 * @param out_length Receives the length; may be NULL.
 * @return 0 on success, -1 on invalid input or OOM.
 */
int glr_ast_to_sexp (const glr_ast_node_t *node, char **out,
                     size_t *out_length);

/**
 * @brief Parse an AST from S-expression text.
 *
 * @param source S-expression text (required).
 * @param length Length of @p source in bytes.
 * @param error Optional error buffer.
 * @param error_size Size of @p error in bytes.
 * @return New AST, or NULL on failure. Release with @ref glr_ast_destroy.
 */
glr_ast_t *glr_ast_from_sexp (const char *source, size_t length, char *error,
                              size_t error_size);

/**
 * @brief Sink for @ref glr_ast_write.
 * @param text Text to write; not NUL-terminated.
 * @param length Length of @p text in bytes.
 * @param user_data Caller context.
 * @return 0 to continue, non-zero to stop the walk.
 */
typedef int (*glr_ast_write_fn) (const char *text, size_t length,
                                void *user_data);

/**
 * @brief Write an AST as S-expressions through a sink.
 *
 * One text callback per token keeps the writer independent of stdio, so the
 * same walk can render into a buffer, a file, or a pipe.
 *
 * @param node Node to write (required).
 * @param write Sink invoked per token (required).
 * @param user_data Context passed to @p write.
 * @return 0 on success, -1 when the sink reported a failure.
 */
int glr_ast_write (const glr_ast_node_t *node, glr_ast_write_fn write,
                   void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* GLR_QUERY_H */
