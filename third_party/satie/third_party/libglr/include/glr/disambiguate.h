#ifndef GLR_DISAMBIGUATE_H
#define GLR_DISAMBIGUATE_H

#include <glr/forest.h>
#include <glr/grammar.h>
#include <glr/reduction.h>
#include <glr/stack.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @file disambiguate.h
 * @brief SPPF disambiguation hooks for LibGLR
 *
 * This interface defines a parser-oriented disambiguation pipeline for
 * Shared Packed Parse Forest (SPPF) conflicts. The parser provides a set of
 * alternative candidates for a single ambiguous forest node, and registered
 * hooks may reject, score, or select a winner.
 *
 * @defgroup disambiguation_api Disambiguation API
 * @brief Parser-integrated machinery for resolving SPPF ambiguity.
 *
 * The API is centered around a @ref glr_disambig_context_t that describes one
 * ambiguous SPPF choice point and a chain of @ref glr_disambig_hook_t objects
 * registered on a parser. Hooks can eliminate candidates, score them, or
 * choose a final winner for precedence, associativity, semantic validation,
 * dynamic programming, probability scoring, and related policies.
 *
 * @defgroup disambstd_api Standard Disambiguators
 * @ingroup disambiguation_api
 * @brief Factory helpers for the built-in disambiguation strategies.
 */

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct glr_parser glr_parser_t;

  /**
   * @typedef glr_disambig_associativity_t
   * @brief Associativity class used by standard operator disambiguators
   */
  typedef enum
  {
    GLR_DISAMBIG_ASSOC_NONE = 0,
    GLR_DISAMBIG_ASSOC_LEFT,
    GLR_DISAMBIG_ASSOC_RIGHT,
    GLR_DISAMBIG_ASSOC_NONASSOC
  } glr_disambig_associativity_t;

  /**
   * @typedef glr_disambig_result_t
   * @brief Result returned by a disambiguation hook
   */
  typedef enum
  {
    GLR_DISAMBIG_NO_MATCH = 0, ///< Hook did not resolve the ambiguity
    GLR_DISAMBIG_RESOLVED = 1, ///< Hook selected a winner
    GLR_DISAMBIG_ERROR = -1    ///< Hook detected an unrecoverable conflict
  } glr_disambig_result_t;

  /**
   * @struct glr_disambig_candidate_t
   * @brief One ambiguous candidate for an SPPF node
   */
  typedef struct
  {
    glr_forest_node_t *node;     ///< Forest node represented by this choice
    glr_reduction_t *reduction;  ///< Reduction that created the candidate
    glr_production_t *production; ///< Grammar production behind the choice
    glr_stack_t *stack;          ///< Stack active when the candidate appeared
    size_t start_position;       ///< Span start in the input
    size_t end_position;         ///< Span end in the input
    size_t split_position;       ///< Binary split or operator pivot
    bool has_split_position;     ///< Whether split_position is meaningful
    int precedence;              ///< Static or dynamic precedence
    glr_disambig_associativity_t associativity; ///< Associativity class
    double score;                ///< Generic additive score
    double probability;          ///< Generic multiplicative probability
    void *semantic_value;        ///< Semantic payload, if any
    void *user_data;             ///< Caller-owned candidate data
    bool rejected;               ///< Hook-maintained elimination flag
  } glr_disambig_candidate_t;

  /**
   * @struct glr_disambig_context_t
   * @brief Full ambiguity context passed to hooks
   */
  typedef struct
  {
    glr_parser_t *parser;                  ///< Parser currently resolving
    glr_grammar_t *grammar;                ///< Grammar associated with parser
    glr_forest_t *forest;                  ///< Forest being disambiguated
    glr_forest_node_t *parent;             ///< Ambiguous parent node
    glr_disambig_candidate_t *candidates;  ///< Candidate array
    size_t candidate_count;                ///< Candidate count
    int lookahead_symbol_id;               ///< Current lookahead symbol
    size_t start_position;                 ///< Ambiguity span start
    size_t end_position;                   ///< Ambiguity span end
    void *user_data;                       ///< Caller-owned context payload
  } glr_disambig_context_t;

  /**
   * @typedef glr_disambig_fn
   * @brief Hook function invoked for one ambiguity
   *
   * Hooks may mark candidates as rejected and optionally report a resolved
   * winner through @p winner_index.
   */
  typedef glr_disambig_result_t (*glr_disambig_fn) (
      glr_disambig_context_t *context, size_t *winner_index, void *user_data);

  /**
   * @typedef glr_disambig_destroy_fn
   * @brief Optional destructor for hook state
   */
  typedef void (*glr_disambig_destroy_fn) (void *user_data);

  /**
   * @struct glr_disambig_hook_t
   * @brief One disambiguation hook registered on a parser
   */
  typedef struct glr_disambig_hook
  {
    char *name;                       ///< Hook name
    unsigned int priority;           ///< Larger values run first
    glr_disambig_fn fn;              ///< Hook callback
    glr_disambig_destroy_fn destroy; ///< Hook state destructor
    void *user_data;                 ///< Hook-owned state
    struct glr_disambig_hook *next;  ///< Linked-list successor
  } glr_disambig_hook_t;

  /**
   * @ingroup disambiguation_api
   * @brief Create a generic disambiguation hook.
   */
  glr_disambig_hook_t *glr_disambig_hook_create (
      const char *name, unsigned int priority, glr_disambig_fn fn,
      void *user_data, glr_disambig_destroy_fn destroy);

  /**
   * @ingroup disambiguation_api
   * @brief Destroy one disambiguation hook.
   */
  void glr_disambig_hook_destroy (glr_disambig_hook_t *hook);

  /**
   * @ingroup disambiguation_api
   * @brief Register a disambiguator on a parser.
   */
  int glr_parser_add_disambiguator (glr_parser_t *parser,
                                    glr_disambig_hook_t *hook);

  /**
   * @ingroup disambiguation_api
   * @brief Remove and destroy all parser disambiguators.
   */
  void glr_parser_clear_disambiguators (glr_parser_t *parser);

  /**
   * @ingroup disambiguation_api
   * @brief Run parser disambiguators against one ambiguity context.
   */
  glr_disambig_result_t glr_parser_run_disambiguators (
      glr_parser_t *parser, glr_disambig_context_t *context,
      size_t *winner_index);

  /**
   * @ingroup disambiguation_api
   * @brief Count the candidates that have not been rejected.
   */
  size_t glr_disambig_context_active_count (
      const glr_disambig_context_t *context);

  /**
   * @ingroup disambiguation_api
   * @brief Return the last surviving candidate index, if unique.
   *
   * Scans the full candidate array and returns the highest surviving
   * index, or SIZE_MAX when every candidate was rejected.
   */
  size_t glr_disambig_context_last_active (
      const glr_disambig_context_t *context);

  /**
   * @ingroup disambiguation_api
   * @brief Return the first surviving candidate index.
   */
  size_t glr_disambig_context_first_active (
      const glr_disambig_context_t *context);

  /**
   * @ingroup disambiguation_api
   * @brief Count hooks registered on a parser.
   */
  size_t glr_parser_disambiguator_count (const glr_parser_t *parser);

  /**
   * @ingroup disambiguation_api
   * @brief Remove one hook by name and destroy it.
   *
   * @return 0 when a hook was removed, -1 when no hook matches.
   */
  int glr_parser_remove_disambiguator (glr_parser_t *parser,
                                       const char *name);

  /**
   * @ingroup disambiguation_api
   * @brief Reject a candidate in-place.
   */
  int glr_disambig_context_reject_candidate (glr_disambig_context_t *context,
                                             size_t index);

  /**
   * @ingroup disambiguation_api
   * @brief Select one candidate and reject the rest.
   */
  int glr_disambig_context_select_candidate (glr_disambig_context_t *context,
                                             size_t index);

  static inline bool
  glr_disambig_candidate_is_active (const glr_disambig_candidate_t *candidate)
  {
    return candidate != NULL && !candidate->rejected;
  }

  /**
   * Standard-library callback shapes
   */
  typedef int (*glr_disambig_int_resolver_fn) (
      const glr_disambig_context_t *context,
      const glr_disambig_candidate_t *candidate, void *user_data);

  typedef glr_disambig_associativity_t (*glr_disambig_assoc_resolver_fn) (
      const glr_disambig_context_t *context,
      const glr_disambig_candidate_t *candidate, void *user_data);

  typedef bool (*glr_disambig_predicate_fn) (
      const glr_disambig_context_t *context,
      const glr_disambig_candidate_t *candidate, void *user_data);

  typedef double (*glr_disambig_score_fn) (
      const glr_disambig_context_t *context, const glr_forest_node_t *node,
      const glr_disambig_candidate_t *candidate, void *user_data);

  /**
   * @ingroup disambstd_api
   * @brief Build a precedence-based disambiguator.
   */
  glr_disambig_hook_t *glr_disambig_precedence_hook_create (
      const char *name, unsigned int priority,
      glr_disambig_int_resolver_fn resolver, void *user_data,
      glr_disambig_destroy_fn destroy);

  /**
   * @ingroup disambstd_api
   * @brief Build an associativity-aware operator disambiguator.
   */
  glr_disambig_hook_t *glr_disambig_associativity_hook_create (
      const char *name, unsigned int priority,
      glr_disambig_int_resolver_fn precedence_resolver,
      glr_disambig_assoc_resolver_fn associativity_resolver, void *user_data,
      glr_disambig_destroy_fn destroy);

  /**
   * @ingroup disambstd_api
   * @brief Build a syntactic predicate disambiguator.
   */
  glr_disambig_hook_t *glr_disambig_predicate_hook_create (
      const char *name, unsigned int priority, glr_disambig_predicate_fn fn,
      void *user_data, glr_disambig_destroy_fn destroy);

  /**
   * @ingroup disambstd_api
   * @brief Build a semantic-constraint disambiguator.
   */
  glr_disambig_hook_t *glr_disambig_semantic_hook_create (
      const char *name, unsigned int priority, glr_disambig_predicate_fn fn,
      void *user_data, glr_disambig_destroy_fn destroy);

  /**
   * @ingroup disambstd_api
   * @brief Build a dynamic-programming disambiguator over forest cost.
   */
  glr_disambig_hook_t *glr_disambig_dynamic_programming_hook_create (
      const char *name, unsigned int priority, glr_disambig_score_fn fn,
      void *user_data, glr_disambig_destroy_fn destroy);

  /**
   * @ingroup disambstd_api
   * @brief Build a probability-scoring disambiguator.
   */
  glr_disambig_hook_t *glr_disambig_probability_hook_create (
      const char *name, unsigned int priority, glr_disambig_score_fn fn,
      void *user_data, glr_disambig_destroy_fn destroy);

#ifdef __cplusplus
}
#endif

#endif /* GLR_DISAMBIGUATE_H */
