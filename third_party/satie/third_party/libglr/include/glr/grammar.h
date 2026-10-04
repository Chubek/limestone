#ifndef GLR_GRAMMAR_H
#define GLR_GRAMMAR_H

#include <glr/parsetbl.h>
#include <glr/stringpool.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @file grammar.h
 * @brief Grammar data structure for GLR parsers
 *
 * This module provides the core grammar data structure used by LibGLR.
 * Grammars are represented as collections of productions, each consisting
 * of a non-terminal head and a sequence of symbols (terminals and/or
 * non-terminals).
 */

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @typedef glr_symbol_type_t
   * @brief Symbol type enumeration
   *
   * Indicates whether a symbol is a terminal or non-terminal.
   */
  typedef enum
  {
    GLR_SYMBOL_TERMINAL,   ///< Terminal symbol
    GLR_SYMBOL_NONTERMINAL ///< Non-terminal symbol
  } glr_symbol_type_t;

  /**
   * @struct glr_symbol_t
   * @brief A single symbol (terminal or non-terminal)
   *
   * Symbols are the basic building blocks of grammar productions.
   */
  typedef struct
  {
    glr_symbol_type_t type; ///< Type of symbol
    int id;                 ///< Unique identifier
    char *name;             ///< Interned name; owned by grammar, immutable
    struct glr_terminal_pattern *pattern; ///< Optional scannerless pattern
  } glr_symbol_t;

  /**
   * @struct glr_production_t
   * @brief A single grammar production
   *
   * A production consists of a head (non-terminal) and a body
   * (sequence of symbols on the right-hand side).
   */
  typedef struct
  {
    int id;              ///< Unique production identifier
    glr_symbol_t *head;  ///< Head non-terminal
    glr_symbol_t **body; ///< Body symbols (array)
    size_t body_length;  ///< Number of symbols in body
    char *annotation;    ///< Optional production annotation
    const char **aliases; ///< Optional one alias per body position
    struct glr_semantic_action *semantic_action; ///< Deferred action
  } glr_production_t;

  /**
   * @struct glr_grammar_t
   * @brief Complete grammar representation
   *
   * Contains all symbols and productions for a grammar,
   * along with the start symbol.
   */
  typedef struct
  {
    glr_symbol_t **symbols; ///< All symbols (terminals + non-terminals)
    size_t symbol_count;    ///< Number of symbols
    glr_production_t **productions; ///< All productions
    size_t production_count;        ///< Number of productions
    glr_symbol_t *start_symbol;     ///< Grammar start symbol
    char *name;                     ///< Grammar name
    glr_parse_table_t *parse_table; ///< Optional parse table for LR/GLR actions
    bool owns_parse_table;          ///< Whether the grammar destroys parse_table
    glr_stringpool_t *strings;      ///< Interned symbol names and aliases
    size_t symbol_capacity;         ///< Internal symbol vector capacity
    size_t production_capacity;     ///< Internal production vector capacity
  } glr_grammar_t;

  /**
   * @brief Create a new empty grammar
   *
   * @return Pointer to new grammar, or NULL on failure
   */
  glr_grammar_t *glr_grammar_create (void);

  /**
   * @brief Destroy a grammar and free all associated memory
   *
   * @param grammar Pointer to grammar to destroy
   */
  void glr_grammar_destroy (glr_grammar_t *grammar);

  /**
   * @brief Add a symbol to the grammar
   *
   * @param grammar Grammar to add symbol to
   * @param type Symbol type (terminal or non-terminal)
   * @param name Symbol name
   * @return Symbol ID (>= 0) on success, -1 on failure
   */
  int glr_grammar_add_symbol (glr_grammar_t *grammar, glr_symbol_type_t type,
                              const char *name);

  /**
   * @brief Get a symbol by ID
   *
   * @param grammar Grammar to search
   * @param id Symbol ID
   * @return Pointer to symbol, or NULL if not found
   */
  glr_symbol_t *glr_grammar_get_symbol (const glr_grammar_t *grammar, int id);

  /**
   * @brief Add a new production to the grammar
   *
   * @param grammar Grammar to add production to
   * @param head_id ID of head non-terminal
   * @param body Symbols in production body (array)
   * @param body_length Number of symbols in body
   * @return Production ID (>= 0) on success, -1 on failure
   */
  int glr_grammar_add_production (glr_grammar_t *grammar, int head_id,
                                  glr_symbol_t **body, size_t body_length);

  /**
   * @brief Set the start symbol for the grammar
   *
   * @param grammar Grammar to configure
   * @param symbol_id ID of start symbol
   * @return 0 on success, -1 on failure
   */
  int glr_grammar_set_start_symbol (glr_grammar_t *grammar, int symbol_id);

  /**
   * @brief Get a production by ID
   *
   * @param grammar Grammar to search
   * @param id Production ID
   * @return Pointer to production, or NULL if not found
   */
  glr_production_t *glr_grammar_get_production (const glr_grammar_t *grammar,
                                                int id);

  /**
   * @brief Attach an optional parse table to the grammar.
   *
   * Grammars can carry a precomputed parse table that parsers created from the
   * grammar will use by default. Passing NULL clears any previously attached
   * table.
   *
   * @param grammar Grammar to update
   * @param parse_table Parse table to attach, or NULL to detach
   * @param take_ownership true if the grammar should destroy the table
   * @return 0 on success, -1 on invalid input
   */
  int glr_grammar_set_parse_table (glr_grammar_t *grammar,
                                   glr_parse_table_t *parse_table,
                                   bool take_ownership);

  /**
   * @brief Retrieve the parse table attached to a grammar.
   *
   * @param grammar Grammar to inspect
   * @return Attached parse table, or NULL if none is configured
   */
  glr_parse_table_t *glr_grammar_get_parse_table (const glr_grammar_t *grammar);

  /**
   * @brief Find a symbol id by name and type.
   *
   * @param grammar Grammar to search (may be NULL)
   * @param name Symbol name (may be NULL)
   * @param type Required symbol type
   * @return Symbol id (>= 0), or -1 when not found
   */
  int glr_grammar_find_symbol (const glr_grammar_t *grammar, const char *name,
                               glr_symbol_type_t type);

  /**
   * @brief Find a symbol id by name regardless of type.
   *
   * @param grammar Grammar to search (may be NULL)
   * @param name Symbol name (may be NULL)
   * @return Symbol id (>= 0), or -1 when not found
   */
  int glr_grammar_find_symbol_any (const glr_grammar_t *grammar,
                                   const char *name);

  /**
   * @brief Validate structural invariants of a grammar.
   *
   * Checks that a start symbol is set, every production head is a
   * non-terminal owned by the grammar, and every body symbol belongs
   * to the grammar.
   *
   * @param grammar Grammar to validate (may be NULL)
   * @param error Optional error buffer (may be NULL)
   * @param error_size Size of the error buffer in bytes
   * @return true when valid, false otherwise
   */
  bool glr_grammar_validate (const glr_grammar_t *grammar, char *error,
                             size_t error_size);

  /**
   * @brief Build an SLR(1) parse table for a grammar.
   *
   * Constructs canonical LR(0) item sets and derives ACTION/GOTO entries
   * with FOLLOW-based reductions. The generated table is indexed by the
   * grammar's symbol ids plus one extra end-of-input column
   * (@ref GLR_PARSE_TABLE_EOF_TERMINAL) that carries the accept action.
   *
   * @param grammar Grammar to analyze (may be NULL)
   * @param error Optional error buffer (may be NULL)
   * @param error_size Size of @p error in bytes
   * @return New parse table owned by the caller, or NULL on failure
   *
   * @note Destroy the result with glr_parse_table_destroy().
   * @see glr_grammar_set_parse_table
   */
  glr_parse_table_t *glr_grammar_build_parse_table (const glr_grammar_t *grammar,
                                                    char *error,
                                                    size_t error_size);

  /**
   * @brief Get the number of symbols in a grammar (null-safe).
   */
  static inline size_t
  glr_grammar_symbol_count (const glr_grammar_t *grammar)
  {
    return grammar != NULL ? grammar->symbol_count : 0;
  }

  /**
   * @brief Get the number of productions in a grammar (null-safe).
   */
  static inline size_t
  glr_grammar_production_count (const glr_grammar_t *grammar)
  {
    return grammar != NULL ? grammar->production_count : 0;
  }

  /**
   * @brief Check if a symbol is a terminal
   *
   * @param symbol Pointer to symbol
   * @return true if terminal, false if non-terminal
   */
  static inline bool
  glr_symbol_is_terminal (glr_symbol_t *symbol)
  {
    return symbol != NULL && symbol->type == GLR_SYMBOL_TERMINAL;
  }

  /**
   * @brief Check if a symbol is a non-terminal
   *
   * @param symbol Pointer to symbol
   * @return true if non-terminal, false if terminal
   */
  static inline bool
  glr_symbol_is_nonterminal (glr_symbol_t *symbol)
  {
    return symbol != NULL && symbol->type == GLR_SYMBOL_NONTERMINAL;
  }

#ifdef __cplusplus
}
#endif

#endif /* GLR_GRAMMAR_H */
