#ifndef GLR_PARSETBL_H
#define GLR_PARSETBL_H

/**
 * @file parsetbl.h
 * @brief Typed parse table representation for GLR parsers.
 *
 * This module defines the data structures used to represent a Generalized LR
 * (GLR) parse table. Unlike traditional LR tables, GLR tables allow multiple
 * actions per (state, terminal) entry to support ambiguity and conflicts.
 *
 * The table is divided into:
 *  - ACTION table (indexed by state × terminal)
 *  - GOTO table (indexed by state × nonterminal)
 *
 * The structures defined here are intended to replace untyped or opaque
 * representations (e.g., void**) with a safe, explicit, and generator-friendly API.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Action Types
 * ============================================================ */

/**
 * @enum glr_action_type_t
 * @brief Enumerates the possible parser actions.
 */
typedef enum {
    GLR_ACTION_SHIFT = 0,  /**< Shift input and transition to another state */
    GLR_ACTION_REDUCE,    /**< Reduce using a production rule */
    GLR_ACTION_ACCEPT,    /**< Accept input (successful parse) */
    GLR_ACTION_ERROR      /**< Error / invalid transition */
} glr_action_type_t;

/* ============================================================
 * Action Representation
 * ============================================================ */

/**
 * @struct glr_action_t
 * @brief Represents a single parser action.
 *
 * Depending on @ref type, the corresponding union field is valid:
 *  - SHIFT  → @ref shift.next_state
 *  - REDUCE → @ref reduce.production_id
 */
typedef struct {
    glr_action_type_t type; /**< Type of action */

    union {
        struct {
            uint32_t next_state; /**< Target state for SHIFT */
        } shift;

        struct {
            uint32_t production_id; /**< Production index for REDUCE */
        } reduce;
    };
} glr_action_t;

/* ============================================================
 * GLR Action Set (Conflict Support)
 * ============================================================ */

/**
 * @struct glr_action_set_t
 * @brief Represents all actions for a (state, terminal) pair.
 *
 * In GLR parsing, a single table cell may contain multiple actions
 * (e.g., shift/reduce or reduce/reduce conflicts). This structure
 * stores all such actions.
 */
typedef struct {
    glr_action_t *actions; /**< Dynamic array of actions */
    size_t action_count;   /**< Number of valid actions */
    size_t capacity;       /**< Allocated capacity */
} glr_action_set_t;

/* ============================================================
 * GOTO Entries
 * ============================================================ */

/**
 * @struct glr_goto_t
 * @brief Represents a GOTO transition for a nonterminal.
 */
typedef struct {
    uint32_t nonterminal_id; /**< Nonterminal symbol ID */
    uint32_t next_state;     /**< Target state */
} glr_goto_t;

/* ============================================================
 * State Representation
 * ============================================================ */

/**
 * @struct glr_state_t
 * @brief Represents a single parser state.
 *
 * Each state contains:
 *  - An ACTION table indexed by terminal ID
 *  - A GOTO table indexed by nonterminal ID
 */
typedef struct {
    /**
     * @brief ACTION table (indexed by terminal ID).
     *
     * Array size is equal to the total number of terminals.
     */
    glr_action_set_t *action_table;

    size_t terminal_count; /**< Number of terminals */

    /**
     * @brief GOTO table entries.
     *
     * Sparse array of transitions for nonterminals.
     */
    glr_goto_t *gotos;

    size_t goto_count;     /**< Number of GOTO entries */
    size_t goto_capacity;  /**< Allocated capacity */
} glr_state_t;

/* ============================================================
 * Full Parse Table
 * ============================================================ */

/**
 * @struct glr_parse_table_t
 * @brief Complete GLR parse table.
 *
 * This structure contains all parser states and associated metadata.
 */
typedef struct {
    glr_state_t *states; /**< Array of parser states */
    size_t state_count;  /**< Number of states */

    size_t terminal_count;    /**< Total number of terminals */
    size_t nonterminal_count; /**< Total number of nonterminals */
} glr_parse_table_t;

/* ============================================================
 * Lifecycle API
 * ============================================================ */

/**
 * @brief Create a new parse table.
 *
 * @param state_count Number of parser states
 * @param terminal_count Number of terminal symbols
 * @param nonterminal_count Number of nonterminal symbols
 * @return Pointer to newly allocated parse table, or NULL on failure
 */
glr_parse_table_t *
glr_parse_table_create(size_t state_count,
                       size_t terminal_count,
                       size_t nonterminal_count);

/**
 * @brief Destroy a parse table and free all associated memory.
 *
 * @param table Parse table to destroy (may be NULL)
 */
void
glr_parse_table_destroy(glr_parse_table_t *table);

/* ============================================================
 * Mutation API
 * ============================================================ */

/**
 * @brief Add an action to a table cell.
 *
 * Appends an action to the set at (state, terminal).
 * Supports multiple actions per cell (GLR conflicts).
 *
 * @param table Parse table
 * @param state State index
 * @param terminal Terminal symbol ID
 * @param action Action to add
 * @return 0 on success, non-zero on error
 */
int
glr_parse_table_add_action(glr_parse_table_t *table,
                          uint32_t state,
                          uint32_t terminal,
                          glr_action_t action);

/**
 * @brief Set or update a GOTO transition.
 *
 * Associates a nonterminal with a target state for a given state.
 *
 * @param table Parse table
 * @param state State index
 * @param nonterminal Nonterminal symbol ID
 * @param next_state Target state
 * @return 0 on success, non-zero on error
 */
int
glr_parse_table_set_goto(glr_parse_table_t *table,
                         uint32_t state,
                         uint32_t nonterminal,
                         uint32_t next_state);

/* ============================================================
 * Query API
 * ============================================================ */

/**
 * @brief Retrieve the action set for a given cell.
 *
 * @param table Parse table
 * @param state State index
 * @param terminal Terminal symbol ID
 * @return Pointer to action set, or NULL on invalid input
 */
const glr_action_set_t *
glr_parse_table_get_actions(const glr_parse_table_t *table,
                           uint32_t state,
                           uint32_t terminal);

/**
 * @brief Lookup a GOTO transition.
 *
 * @param table Parse table
 * @param state State index
 * @param nonterminal Nonterminal symbol ID
 * @param out_next_state Output pointer for next state
 * @return 0 if found, non-zero if not found or invalid
 */
int
glr_parse_table_get_goto(const glr_parse_table_t *table,
                        uint32_t state,
                        uint32_t nonterminal,
                        uint32_t *out_next_state);

/**
 * @brief Count actions stored in one table cell.
 *
 * @param table Parse table (may be NULL)
 * @param state State index
 * @param terminal Terminal symbol ID
 * @return Action count, or 0 on invalid input
 */
size_t
glr_parse_table_action_count(const glr_parse_table_t *table,
                             uint32_t state,
                             uint32_t terminal);

/**
 * @brief Check whether a cell holds conflicting actions.
 *
 * A cell conflicts when it stores more than one action (shift/reduce
 * or reduce/reduce ambiguity in GLR terms).
 *
 * @param table Parse table (may be NULL)
 * @param state State index
 * @param terminal Terminal symbol ID
 * @return true when action count > 1
 */
bool
glr_parse_table_has_conflict(const glr_parse_table_t *table,
                             uint32_t state,
                             uint32_t terminal);

/**
 * @brief Count all conflicts in a parse table.
 *
 * @param table Parse table (may be NULL)
 * @return Number of conflicting cells
 */
size_t
glr_parse_table_conflict_count(const glr_parse_table_t *table);

/** @def GLR_PARSE_TABLE_MAX_STATES
 *  @brief Upper bound on generated state count (safety valve). */
#define GLR_PARSE_TABLE_MAX_STATES 100000u

/**
 * @brief Get the end-of-input column of a table.
 *
 * Tables built by glr_grammar_build_parse_table() reserve exactly one
 * terminal column past the grammar's symbol ids. That column holds the
 * reductions justified by FOLLOW and the single ACCEPT action, so
 * end-of-input behaviour never collides with a real terminal.
 *
 * @param table Parse table (may be NULL)
 * @return EOF column index, or 0 for a NULL/empty table
 */
static inline uint32_t
glr_parse_table_eof_column(const glr_parse_table_t *table)
{
    return (table != NULL && table->terminal_count > 0)
               ? (uint32_t)(table->terminal_count - 1)
               : 0u;
}

#ifdef __cplusplus
}
#endif

#endif /* GLR_PARSETBL_H */