#ifndef GLR_REWRITE_H
#define GLR_REWRITE_H

#include <glr/grammar.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @file rewrite.h
 * @brief Grammar rewrite system for LibGLR.
 *
 * The GLR Rewrite Language (GRL) is a small S-expression language for
 * describing grammar-to-grammar transforms. GRL programs compile into a
 * sequence of @ref glr_rewrite_rule_t entries that can then be applied to a
 * @ref glr_grammar_t either from text, from a file, or procedurally.
 *
 * GRL focuses on grammar normalization. The built-in rule kinds cover common
 * textbook rewrites such as epsilon elimination, unit-production elimination,
 * useless-symbol elimination, left-recursion removal, and left factoring.
 * The API also exposes lower-level editing primitives so applications can mix
 * declarative and procedural rewrites in the same pipeline.
 */

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @defgroup rewrite_api Rewrite API
   * @brief Declarative and procedural grammar rewriting support.
   */

  /**
   * @ingroup rewrite_api
   * @brief Rule opcode understood by the runtime and the GRL compiler.
   */
  typedef enum
  {
    GLR_REWRITE_RULE_ADD_SYMBOL = 0,
    GLR_REWRITE_RULE_DROP_SYMBOL,
    GLR_REWRITE_RULE_RENAME_SYMBOL,
    GLR_REWRITE_RULE_SET_START,
    GLR_REWRITE_RULE_ADD_PRODUCTION,
    GLR_REWRITE_RULE_DROP_PRODUCTION,
    GLR_REWRITE_RULE_REMOVE_EPSILON_PRODUCTIONS,
    GLR_REWRITE_RULE_REMOVE_UNIT_PRODUCTIONS,
    GLR_REWRITE_RULE_REMOVE_USELESS_SYMBOLS,
    GLR_REWRITE_RULE_REMOVE_LEFT_RECURSION,
    GLR_REWRITE_RULE_LEFT_FACTOR,
    GLR_REWRITE_RULE_MAKE_LR_COMPATIBLE,
    GLR_REWRITE_RULE_ELIMINATE_AMBIGUITY
  } glr_rewrite_rule_kind_t;

  /**
   * @ingroup rewrite_api
   * @brief Status code returned by rewrite operations.
   */
  typedef enum
  {
    GLR_REWRITE_STATUS_OK = 0,
    GLR_REWRITE_STATUS_INVALID_ARGUMENT = -1,
    GLR_REWRITE_STATUS_PARSE_ERROR = -2,
    GLR_REWRITE_STATUS_MEMORY_ERROR = -3,
    GLR_REWRITE_STATUS_NOT_FOUND = -4,
    GLR_REWRITE_STATUS_CONFLICT = -5,
    GLR_REWRITE_STATUS_UNSUPPORTED = -6
  } glr_rewrite_status_t;

  /**
   * @ingroup rewrite_api
   * @brief Named symbol reference stored inside a compiled rule.
   */
  typedef struct
  {
    glr_symbol_type_t type;
    char *name;
  } glr_rewrite_symbol_spec_t;

  /**
   * @ingroup rewrite_api
   * @brief Production template stored inside a compiled rule.
   */
  typedef struct
  {
    char *head_name;
    glr_rewrite_symbol_spec_t *body;
    size_t body_length;
  } glr_rewrite_production_spec_t;

  /**
   * @ingroup rewrite_api
   * @brief One compiled rewrite rule.
   */
  typedef struct
  {
    glr_rewrite_rule_kind_t kind;
    union
    {
      glr_rewrite_symbol_spec_t symbol;
      struct
      {
        char *old_name;
        char *new_name;
      } rename_symbol;
      char *start_symbol_name;
      glr_rewrite_production_spec_t production;
    } data;
  } glr_rewrite_rule_t;

  /**
   * @ingroup rewrite_api
   * @brief Compiled GRL program.
   */
  typedef struct
  {
    char *name;
    glr_rewrite_rule_t *rules;
    size_t rule_count;
    size_t rule_capacity;
  } glr_rewrite_program_t;

  /**
   * @ingroup rewrite_api
   * @brief Optional execution report.
   */
  typedef struct
  {
    size_t rules_attempted;
    size_t rules_applied;
    glr_rewrite_status_t status;
    char message[256];
  } glr_rewrite_report_t;

  /**
   * @ingroup rewrite_api
   * @brief Create an empty rewrite program for procedural population.
   */
  glr_rewrite_program_t *glr_rewrite_program_create (const char *name);

  /**
   * @ingroup rewrite_api
   * @brief Destroy a rewrite program and all compiled rules.
   */
  void glr_rewrite_program_destroy (glr_rewrite_program_t *program);

  /**
   * @ingroup rewrite_api
   * @brief Append a fully-formed rule to a rewrite program.
   */
  glr_rewrite_status_t glr_rewrite_program_add_rule (
      glr_rewrite_program_t *program, const glr_rewrite_rule_t *rule);

  /**
   * @ingroup rewrite_api
   * @brief Compile a GRL program from an in-memory S-expression string.
   */
  glr_rewrite_program_t *glr_rewrite_program_parse (const char *source,
                                                    size_t length,
                                                    char *error_buffer,
                                                    size_t error_buffer_size);

  /**
   * @ingroup rewrite_api
   * @brief Compile a GRL program from a file on disk.
   */
  glr_rewrite_program_t *glr_rewrite_program_load_file (
      const char *path, char *error_buffer, size_t error_buffer_size);

  /**
   * @ingroup rewrite_api
   * @brief Apply a compiled rewrite program to a grammar in place.
   */
  glr_rewrite_status_t glr_rewrite_program_apply (
      glr_grammar_t *grammar, const glr_rewrite_program_t *program,
      glr_rewrite_report_t *report);

  /**
   * @ingroup rewrite_api
   * @brief Apply a single compiled rule to a grammar in place.
   */
  glr_rewrite_status_t glr_rewrite_apply_rule (glr_grammar_t *grammar,
                                               const glr_rewrite_rule_t *rule);

  /**
   * @ingroup rewrite_api
   * @brief Add a symbol to a grammar if it does not already exist.
   */
  glr_rewrite_status_t glr_rewrite_add_symbol (glr_grammar_t *grammar,
                                               glr_symbol_type_t type,
                                               const char *name);

  /**
   * @ingroup rewrite_api
   * @brief Drop a symbol and any productions that reference it.
   */
  glr_rewrite_status_t glr_rewrite_drop_symbol (glr_grammar_t *grammar,
                                                const char *name);

  /**
   * @ingroup rewrite_api
   * @brief Rename an existing grammar symbol.
   */
  glr_rewrite_status_t glr_rewrite_rename_symbol (glr_grammar_t *grammar,
                                                  const char *old_name,
                                                  const char *new_name);

  /**
   * @ingroup rewrite_api
   * @brief Change the grammar start symbol by name.
   */
  glr_rewrite_status_t glr_rewrite_set_start (glr_grammar_t *grammar,
                                              const char *name);

  /**
   * @ingroup rewrite_api
   * @brief Add a production by symbol names.
   */
  glr_rewrite_status_t glr_rewrite_add_production (
      glr_grammar_t *grammar, const char *head_name, const char *const *body,
      size_t body_length);

  /**
   * @ingroup rewrite_api
   * @brief Remove productions that exactly match the supplied template.
   */
  glr_rewrite_status_t glr_rewrite_drop_production (
      glr_grammar_t *grammar, const char *head_name, const char *const *body,
      size_t body_length);

  /**
   * @ingroup rewrite_api
   * @brief Remove epsilon productions while preserving derivability.
   */
  glr_rewrite_status_t glr_rewrite_remove_epsilon_productions (
      glr_grammar_t *grammar);

  /**
   * @ingroup rewrite_api
   * @brief Remove unit productions while preserving derivability.
   */
  glr_rewrite_status_t glr_rewrite_remove_unit_productions (
      glr_grammar_t *grammar);

  /**
   * @ingroup rewrite_api
   * @brief Remove unreachable or unproductive symbols and productions.
   */
  glr_rewrite_status_t glr_rewrite_remove_useless_symbols (
      glr_grammar_t *grammar);

  /**
   * @ingroup rewrite_api
   * @brief Eliminate left recursion using the standard ordered-substitution pass.
   */
  glr_rewrite_status_t glr_rewrite_remove_left_recursion (
      glr_grammar_t *grammar);

  /**
   * @ingroup rewrite_api
   * @brief Perform repeated single-prefix left factoring.
   */
  glr_rewrite_status_t glr_rewrite_left_factor (glr_grammar_t *grammar);

  /**
   * @ingroup rewrite_api
   * @brief Run the standard LR-normalization pipeline.
   */
  glr_rewrite_status_t glr_rewrite_make_lr_compatible (
      glr_grammar_t *grammar);

  /**
   * @ingroup rewrite_api
   * @brief Ambiguity-reduction pipeline with a verifiable result.
   *
   * Runs the LR-normalization pipeline (epsilon removal, unit removal,
   * left-recursion elimination, left factoring, useless-symbol removal) and
   * then *verifies* the outcome by building a parse table for the result.
   *
   * @return GLR_REWRITE_STATUS_OK when the rewritten grammar has a
   *         conflict-free parse table, GLR_REWRITE_STATUS_CONFLICT when
   *         conflicts remain (the grammar is still ambiguous, and no
   *         normalization pass can fix that in general), or the failing
   *         status of an underlying pass.
   *
   * @note This is a normalization pass, not an ambiguity prover: a grammar
   *       such as `E -> E + E | E * E | n` stays ambiguous, and the
   *       GLR_REWRITE_STATUS_CONFLICT result says so instead of leaving the
   *       caller to guess.
   */
  glr_rewrite_status_t glr_rewrite_eliminate_ambiguity (
      glr_grammar_t *grammar);

#ifdef __cplusplus
}
#endif

#endif /* GLR_REWRITE_H */
