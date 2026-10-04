#ifndef GLR_PARSER_H
#define GLR_PARSER_H

#include <glr/forest.h>
#include <glr/grammar.h>
#include <glr/lexer-hooks.h>
#include <glr/parsetbl.h>
#include <glr/reader.h>
#include <glr/reduction.h>
#include <glr/stack.h>
#include <glr/semantic-action.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @file parser.h
 * @brief GLR parser core infrastructure
 *
 * This module provides the main GLR (Generalized LR) parser implementation
 * based on Tomita's algorithm. It supports:
 * - Graph-Structured Stack (GSS) for handling multiple parse paths
 * - Shared Packed Parse Forest (SPPF) for representing ambiguous parses
 * - Pluggable disambiguation strategies
 * - UTF-8 and UTF-16 input support via reader hooks
 * - Optional incremental parsing with caching (when compiled with libmdbx support)
 *
 * The parser maintains multiple stacks simultaneously to handle grammar
 * ambiguities and conflicts, merging results into a shared parse forest.
 */

#ifdef __cplusplus
extern "C"
{
#endif

  /* Forward declarations */
  typedef struct glr_parser glr_parser_t;

#include <glr/disambiguate.h>

  /**
   * @typedef glr_parse_error_t
   * @brief Error codes returned by parsing operations
   */
  typedef enum
  {
    GLR_PARSE_SUCCESS = 0,        ///< Parsing completed successfully
    GLR_PARSE_ERROR_SYNTAX,       ///< Syntax error in input
    GLR_PARSE_ERROR_MEMORY,       ///< Memory allocation failure
    GLR_PARSE_ERROR_GRAMMAR,      ///< Invalid or malformed grammar
    GLR_PARSE_ERROR_UNRECOVERABLE, ///< Unrecoverable parse error (no valid paths)
    GLR_PARSE_ERROR_SEMANTIC       ///< Deferred semantic evaluation failed
  } glr_parse_error_t;

  /**
   * @struct glr_parse_result_t
   * @brief Complete result of a parsing operation
   *
   * Contains the parse forest (on success), error information,
   * and the position reached in the input stream.
   */
  typedef struct
  {
    glr_parse_error_t error; ///< Error code (GLR_PARSE_SUCCESS on success)
    glr_forest_t *forest;    ///< Resulting parse forest (NULL on error)
    size_t position;         ///< Number of bytes consumed from input
    void *user_data;         ///< User-provided context data
    void *semantic_value;    ///< Start-production value (see semantic-action.h)
    glr_semantic_error_t semantic_error; ///< Semantic evaluation status
  } glr_parse_result_t;

  /**
   * @brief Sink for parse snapshots.
   *
   * Invoked for each distinct stack configuration that reaches a token
   * boundary; ambiguous parses may invoke it multiple times at one position.
   * A snapshot is the whole configuration,
   * not just the top of it: an LR reduction pops as many entries as its
   * production is long and reads the state below them, so resuming from a
   * single entry would silently lose the rest of the stack.
   *
   * The stack is owned by the parser and valid only for the duration of the
   * callback, so a sink that wants to keep one must copy it with
   * glr_stack_copy().
   *
   * @param stack Parse stack at @p position; never NULL.
   * @param position Input byte offset the stack has consumed.
   * @param user_data Caller context.
   */
  typedef void (*glr_parser_snapshot_fn) (glr_stack_t *stack, size_t position,
                                          void *user_data);

  /**
   * @struct glr_parser_t
   * @brief Main GLR parser instance
   *
   * Encapsulates all state required for GLR parsing:
   * - Grammar and state transition table
   * - Multiple stacks (Graph-Structured Stack) for parallel parse paths
   * - Shared parse forest for representing all valid parse trees
   * - Input buffer and position tracking
   * - Lexer hooks for custom tokenization
   * - Disambiguation hooks for resolving conflicts
   *
   * The parser can be reused across multiple parse operations by calling
   * glr_parser_reset() or glr_parse() (which resets automatically).
   */
  struct glr_parser
  {
    glr_grammar_t *grammar;              ///< Grammar specification
    glr_stack_t **stacks;                ///< Retained accepted parse stacks (GSS)
    size_t stack_count;                  ///< Number of distinct accepted roots
    size_t stack_capacity;               ///< Allocated capacity for stacks array
    /**
     * Legacy compatibility storage parallel to @ref stacks. The immutable
     * configuration scheduler does not use these flags.
     */
    bool *stack_skip_reduce;
    glr_forest_t *forest;                ///< Shared Packed Parse Forest (SPPF)
    void **state_table;                  ///< LR state transition table
    size_t state_table_size;             ///< Number of states in table
    glr_parse_table_t *parse_table;      ///< Optional typed parse table override
    bool owns_parse_table;               ///< Whether parser destroys parse_table
    glr_disambig_hook_t *disambig_hooks; ///< Chain of disambiguation hooks
    const char *input;                   ///< Current input buffer
    size_t input_pos;                    ///< Current byte position in input
    size_t input_length;                 ///< Total input buffer length in bytes
    glr_reader_t *reader;                ///< Token reader for UTF-16 input
    glr_lexer_hooks_t *lexer_hooks;      ///< Custom lexer hooks (optional)
    glr_reader_token_t lookahead;        ///< Current lookahead token
    glr_reader_token_t last_token;       ///< Most recent accepted token
    glr_parse_error_t error;             ///< Most recent error code
    void *user_data;                     ///< User-provided context pointer
    struct glr_cache_t *cache;           ///< Incremental cache (NULL = off)
    char *trivia;                        ///< Literal skipped between tokens
    glr_parser_snapshot_fn snapshot_hook; ///< Optional per-position snapshot sink */
    void *snapshot_data;                  ///< Context passed to the snapshot sink */
    bool scannerless;                     ///< Explore literal lexical alternatives
    glr_semantic_resolver_fn semantic_resolver; ///< Resolve packed alternatives
    bool generated_parse_table;           ///< Internal generated-table marker
    uint64_t grammar_fingerprint;         ///< Structure used by the generated table
  };

  /* ========================================================================
   * Core Parser Lifecycle Functions
   * ======================================================================== */

  /**
   * @brief Create a new GLR parser
   *
   * Allocates and initializes a new parser instance for the given grammar.
   * The parser maintains a reference to the grammar but does not take ownership.
   *
   * @param grammar Grammar to parse with
   * @return Newly allocated parser instance, or NULL on allocation failure
   *
   * @note The caller must eventually call glr_parser_destroy() to free resources.
   * @see glr_parser_destroy
   */
  glr_parser_t *glr_parser_create (glr_grammar_t *grammar);

  /**
   * @brief Destroy a parser instance and free all associated resources
   *
   * Releases all memory held by the parser, including stacks, parse forest,
   * state tables, and disambiguation hooks. The grammar itself is not freed.
   *
   * @param parser Parser instance to destroy (may be NULL)
   *
   * @note After calling this function, the parser pointer is invalid.
   * @see glr_parser_create
   */
  void glr_parser_destroy (glr_parser_t *parser);

  /**
   * @brief Reset parser state for a new parse operation
   *
   * Clears all stacks, resets the parse forest, and reinitializes internal state.
   * This is called automatically by glr_parse() but can be invoked manually
   * if you need to abort a parse and start fresh.
   *
   * @param parser Parser instance to reset
   * @return 0 on success, -1 on failure
   *
   * @note The grammar and configuration (lexer hooks, disambiguation hooks) are preserved.
   */
  int glr_parser_reset (glr_parser_t *parser);

  /* ========================================================================
   * Parsing Functions
   * ======================================================================== */

  /**
   * @brief Parse input buffer using the GLR algorithm (non-incremental)
   *
   * Performs a complete, non-incremental parse of the input buffer from start
   * to finish. The parser is automatically reset before parsing begins, so any
   * previous parse state is discarded.
   *
   * This function implements Tomita's GLR algorithm:
   * 1. Maintains multiple parse stacks (Graph-Structured Stack)
   * 2. Performs shift and reduce operations on all active stacks
   * 3. Handles shift/reduce and reduce/reduce conflicts by forking stacks
   * 4. Merges results into a Shared Packed Parse Forest (SPPF)
   * 5. Applies disambiguation hooks to resolve ambiguities
   *
   * @param parser Initialized parser instance
   * @param input  Input buffer to parse (UTF-8 or UTF-16)
   * @param length Length of the input buffer in bytes
   * @return Parse result structure containing error code, forest, and position
   *
   * @note For incremental parsing, use glr_parser_parse_incremental() instead.
   * @note The returned forest is owned by the parser and remains valid until
   *       the next parse operation or glr_parser_destroy().
   *
   * @see glr_parser_parse_incremental
   * @see glr_parse_result_t
   */
  glr_parse_result_t glr_parse (glr_parser_t *parser, const char *input,
                                size_t length);

  /** Enable scannerless alternatives for literal-only grammars. Grammars with
      terminal patterns enable it automatically. Patterns operate on byte input
      (including UTF-8), bypassing UTF-16 auto-detection. */
  int glr_parser_set_scannerless (glr_parser_t *parser, bool enabled);

  /** Actions run automatically after acceptance when registered on a grammar.
      A NULL resolver requires an unambiguous forest. */
  int glr_parser_set_semantic_resolver (glr_parser_t *parser,
                                       glr_semantic_resolver_fn resolver);

  /* ========================================================================
   * Resumable parsing
   * ======================================================================== */

  /**
   * @brief Take the parser's current forest, leaving it with an empty one.
   *
   * Ownership moves to the caller. Any snapshot taken during the parse refers
   * to nodes inside this forest, so a caller that keeps snapshots has to keep
   * the forest they came from as well; taking it over is what makes that
   * possible.
   *
   * @param parser Parser instance (required).
    * @return The forest, now owned by the caller, or NULL on allocation failure.
   */
  glr_forest_t *glr_parser_take_forest (glr_parser_t *parser);

  /**
    * @brief Install a sink that receives snapshots at token boundaries.
   *
    * A deterministic parse can resume from the last snapshot whose prefix and
    * lexical boundary the edit did not touch. Ambiguous parses can produce
    * several configurations at one offset; resuming every reading requires
    * retaining all those configurations or reparsing the input.
   *
   * @param parser Parser instance (required).
   * @param snapshot Sink to invoke, or NULL to stop snapshotting.
   * @param user_data Context passed to @p snapshot.
   * @return 0 on success, -1 on invalid input.
   */
  int glr_parser_set_snapshot_hook (glr_parser_t *parser,
                                    glr_parser_snapshot_fn snapshot,
                                    void *user_data);

  /**
   * @brief Continue a parse from a saved snapshot over new input.
   *
   * Resumes with @p entry as the initial stack, continues from @p position
   * into @p input, and packs everything it builds into @p forest. This is the
   * incremental counterpart of glr_parse(): the caller supplies the state and
   * the forest, so the prefix described by @p entry is neither re-parsed nor
   * re-allocated.
   *
   * The stack is copied in, so the caller's snapshot stays valid and can be
   * resumed from again after a further edit.
   *
   * @param parser Initialized parser instance (required).
   * @param stack Snapshot to resume from; must have been produced by the
   *              snapshot hook of this parser (required).
   * @param forest Forest to pack into; required and owned by the caller. It
   *               must already contain the nodes the snapshot's entries pack,
   *               since those pointers are shared, not remapped.
   * @param position Offset into @p input where @p stack applies.
   * @param input Input buffer covering the whole document, so that positions
   *              recorded in @p forest stay comparable with it.
   * @param length Length of @p input in bytes.
   * @param out_result Receives the parse result; may be NULL.
   * @return 0 when the parse accepted, -1 on failure.
   *
   * @note A snapshot is only valid for the text that preceded it and for its
   *       lexical boundaries. An edit that can extend a pattern across the
   *       boundary requires an earlier snapshot or a full parse. Resuming
   *       from a position the caller has since edited yields whatever the
   *       automaton makes of the new text, which is exactly the recomputation
   *       the caller wanted to avoid; the session layer in live-parsing.h
   *       picks the snapshot it knows is still valid and drops the nodes the
   *       edit invalidated before handing the forest over.
   *
   * @see glr_parser_set_snapshot_hook
   */
  int glr_parser_parse_from (glr_parser_t *parser, const glr_stack_t *stack,
                             glr_forest_t *forest, size_t position,
                             const char *input, size_t length,
                             glr_parse_result_t *out_result);

  /* ========================================================================
   * Lexer Configuration
   * ======================================================================== */

  /**
   * @brief Set custom lexer hooks for tokenization
   *
   * Configures custom lexer hooks that override default tokenization behavior.
   * This is useful for languages with complex lexical rules or when integrating
   * with external lexers.
   *
   * @param parser Parser instance
   * @param hooks Lexer hooks structure, or NULL to use default tokenization
   * @return 0 on success, -1 on failure
   *
   * @note The parser does not take ownership of the hooks structure.
   * @note Hooks are applied to the internal reader for UTF-16 input processing.
   *
   * @see glr_parser_get_lexer_hooks
   * @see glr_lexer_hooks_t
   */
  int glr_parser_set_lexer_hooks (glr_parser_t *parser, glr_lexer_hooks_t *hooks);

  /**
   * @brief Get currently configured lexer hooks
   *
   * @param parser Parser instance
   * @return Currently configured lexer hooks, or NULL if none set
   *
   * @see glr_parser_set_lexer_hooks
   */
  glr_lexer_hooks_t *glr_parser_get_lexer_hooks (const glr_parser_t *parser);

  /**
   * @brief Override the parse table used by a parser.
   *
   * When no parser-specific table is configured, the parser falls back to the
   * optional table attached to its grammar. Passing NULL clears the override
   * and restores grammar-driven lookup.
   *
   * @param parser Parser instance
   * @param parse_table Parse table override, or NULL to clear it
   * @param take_ownership true if the parser should destroy the table
   * @return 0 on success, -1 on invalid input
   */
  int glr_parser_set_parse_table (glr_parser_t *parser,
                                  glr_parse_table_t *parse_table,
                                  bool take_ownership);

  /**
   * @brief Get the active parser-specific parse table override.
   *
   * @param parser Parser instance
   * @return Parser override table, or NULL if the parser uses the grammar table
   */
  glr_parse_table_t *glr_parser_get_parse_table (const glr_parser_t *parser);

  /* ========================================================================
   * Parser State Inspection
   * ======================================================================== */

  /**
   * @brief Configure a literal that the tokenizer skips between tokens.
   *
   * The parser tokenizes byte input by matching the grammar's own terminal
   * names against the cursor (longest match wins). Trivia is the exception:
   * any run of the configured literal is discarded before the next terminal
   * is looked up, which is how languages express optional whitespace or
   * comment delimiters without making the grammar ambiguous.
   *
   * @param parser Parser instance
   * @param trivia Literal to skip, or NULL to disable skipping
   * @return 0 on success, -1 on invalid input or allocation failure
   *
   * @note The literal must be non-empty and must not also be a terminal the
   *       parser needs to match.
   * @see glr_parser_get_trivia
   */
  int glr_parser_set_trivia (glr_parser_t *parser, const char *trivia);

  /**
   * @brief Get the trivia literal currently skipped by the tokenizer.
   *
   * @param parser Parser instance
   * @return Owned literal owned by the parser, or NULL when disabled
   */
  const char *glr_parser_get_trivia (const glr_parser_t *parser);

  /**
   * @brief Get the most recent token read by the parser
   *
   * Returns the current lookahead token that was last consumed from the input.
   * This is useful for error reporting and debugging.
   *
   * @param parser Parser instance
   * @return Pointer to last token, or NULL if no token has been read yet
   *
   * @note The returned pointer is valid until the next parse operation.
   */
  const glr_reader_token_t *glr_parser_get_last_token (const glr_parser_t *parser);

  /**
   * @brief Associate user data with the parser
   *
   * Stores an arbitrary user pointer that can be retrieved later. This is
   * useful for passing context to disambiguation hooks or callbacks.
   *
   * @param parser Parser instance
   * @param data User-provided context pointer
   *
   * @see glr_parser_get_user_data
   */
  static inline void
  glr_parser_set_user_data (glr_parser_t *parser, void *data)
  {
    if (parser != NULL)
      {
        parser->user_data = data;
      }
  }

  /**
   * @brief Retrieve user data associated with the parser
   *
   * @param parser Parser instance
   * @return User data pointer previously set via glr_parser_set_user_data()
   *
   * @see glr_parser_set_user_data
   */
  static inline void *
  glr_parser_get_user_data (glr_parser_t *parser)
  {
    return parser != NULL ? parser->user_data : NULL;
  }

  /**
   * @brief Get the most recent error code
   *
   * @param parser Parser instance
   * @return Last error code, or GLR_PARSE_ERROR_MEMORY if parser is NULL
   */
  static inline glr_parse_error_t
  glr_parser_get_error (glr_parser_t *parser)
  {
    return parser != NULL ? parser->error : GLR_PARSE_ERROR_MEMORY;
  }

  /**
   * @brief Get the current parse forest
   *
   * Returns the Shared Packed Parse Forest (SPPF) containing all valid
   * parse trees for the input. The forest may contain multiple derivations
   * if the grammar is ambiguous.
   *
   * @param parser Parser instance
   * @return Parse forest, or NULL if no successful parse has occurred
   *
   * @note The forest is owned by the parser and remains valid until the
   *       next parse operation or glr_parser_destroy().
   */
  static inline glr_forest_t *
  glr_parser_get_forest (glr_parser_t *parser)
  {
    return parser != NULL ? parser->forest : NULL;
  }

  /**
    * @brief Get the number of retained accepted parse stacks
   *
    * Accepted stacks sharing the same packed root are retained once. Inspect
    * glr_forest_is_ambiguous() to detect alternatives within an accepted root.
   *
   * @param parser Parser instance
    * @return Number of accepted roots, or 0 if parser is NULL
   */
  static inline size_t
  glr_parser_stack_count (glr_parser_t *parser)
  {
    return parser != NULL ? parser->stack_count : 0;
  }

  /* ========================================================================
   * Incremental Parsing Support (requires libmdbx)
   * ======================================================================== */

#ifdef HAVE_LMDB
  /* Forward declarations for cache types */
  struct glr_cache_t;
  struct glr_cache_stats_t;

  /**
   * @brief Set cache handle for incremental parsing
   *
   * Associates a cache instance with the parser to enable incremental parsing.
   * The cache stores parse results for input regions, allowing fast re-parsing
   * when only small portions of the input change.
   *
   * @param parser Parser instance
   * @param cache Cache handle, or NULL to disable caching
   *
   * @see glr_parser_get_cache
   * @see glr_parser_enable_incremental
   */
  void glr_parser_set_cache (glr_parser_t *parser, struct glr_cache_t *cache);

  /**
   * @brief Get the current cache handle
   *
   * @param parser Parser instance
   * @return Cache handle, or NULL if caching is not enabled
   *
   * @see glr_parser_set_cache
   */
  struct glr_cache_t *glr_parser_get_cache (const glr_parser_t *parser);

  /**
   * @brief Enable incremental parsing with automatic cache setup
   *
   * Creates and configures a cache at the specified path, enabling incremental
   * parsing for subsequent operations. The cache persists across parser instances.
   *
   * @param parser Parser instance
   * @param cache_path Filesystem path to cache directory
   * @return 0 on success, -1 on error
   *
   * @see glr_parser_disable_incremental
   * @see glr_parser_parse_incremental
   */
  int glr_parser_enable_incremental (glr_parser_t *parser,
                                      const char *cache_path);

  /**
   * @brief Disable incremental parsing and close cache
   *
   * Closes the cache and disables incremental parsing. Subsequent parse
   * operations will be non-incremental.
   *
   * @param parser Parser instance
   *
   * @see glr_parser_enable_incremental
   */
  void glr_parser_disable_incremental (glr_parser_t *parser);

  /**
   * @brief Get cache performance statistics
   *
   * Retrieves statistics about cache usage, including hit/miss rates and
   * storage metrics.
   *
   * @param parser Parser instance
   * @param stats Output structure to receive statistics
   * @return 0 on success, -1 on error
   *
   * @see glr_parser_enable_incremental
   */
  int glr_parser_get_cache_stats (glr_parser_t *parser,
                                   struct glr_cache_stats_t *stats);

#endif /* HAVE_LMDB */

  /**
   * @brief Parse incrementally by reusing previous parse results
   *
   * Performs incremental parsing by comparing the new input with the old input
   * and reusing parse results from unchanged regions. This is significantly
   * faster than full parsing for small edits, making it ideal for IDE and LSP
   * integration.
   *
   * The algorithm:
   * 1. Computes a diff between old_content and new_content
   * 2. Identifies unchanged regions and reuses their parse trees from old_forest
   * 3. Re-parses only the changed regions and their affected context
   * 4. Merges the results into a new parse forest
   *
   * @param parser Parser instance
   * @param old_forest Previous parse forest (NULL for full parse)
   * @param old_content Previous source content (NULL for full parse)
   * @param old_len Length of old content in bytes
   * @param new_content New source content to parse
   * @param new_len Length of new content in bytes
   * @param edit_start Start byte offset of edit (0 if unknown, will auto-detect)
   * @param edit_end End byte offset of edit in old content (0 if unknown)
   * @param out_forest Output parameter to receive new parse forest
   * @return 0 on success, -1 on error
   *
    * @note If old_forest or old_content is NULL, performs a full parse.
    * @note Scannerless grammars and grammars with semantic actions perform a
    *       full parse to preserve token boundaries and complete evaluations.
   * @note The caller is responsible for freeing the output forest.
   *
   * @see glr_parse
   * @see glr_parser_enable_incremental
   */
  int glr_parser_parse_incremental (glr_parser_t *parser,
                                     const glr_forest_t *old_forest,
                                     const char *old_content, size_t old_len,
                                     const char *new_content, size_t new_len,
                                     size_t edit_start, size_t edit_end,
                                     glr_forest_t **out_forest);

#ifdef __cplusplus
}
#endif

#endif /* GLR_PARSER_H */
