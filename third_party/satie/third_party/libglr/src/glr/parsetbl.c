#include <stdlib.h>
#include <string.h>

#include "glr/parsetbl.h"

/* ============================================================
 * Internal helpers
 * ============================================================ */

static void
glr_action_set_init(glr_action_set_t *set)
{
    set->actions = NULL;
    set->action_count = 0;
    set->capacity = 0;
}

static void
glr_action_set_destroy(glr_action_set_t *set)
{
    if (!set) return;

    free(set->actions);
    set->actions = NULL;
    set->action_count = 0;
    set->capacity = 0;
}

static int
glr_action_set_reserve(glr_action_set_t *set, size_t needed)
{
    if (needed <= set->capacity)
        return 0;

    size_t cap = set->capacity ? set->capacity : 4;
    while (cap < needed)
        cap *= 2;

    glr_action_t *new_buf =
        (glr_action_t *)realloc(set->actions, cap * sizeof(glr_action_t));

    if (!new_buf)
        return -1;

    set->actions = new_buf;
    set->capacity = cap;
    return 0;
}

static int
glr_action_set_add(glr_action_set_t *set, glr_action_t action)
{
    if (glr_action_set_reserve(set, set->action_count + 1) != 0)
        return -1;

    set->actions[set->action_count++] = action;
    return 0;
}

static int
glr_state_reserve_goto(glr_state_t *st, size_t needed)
{
    if (needed <= st->goto_capacity)
        return 0;

    size_t cap = st->goto_capacity ? st->goto_capacity : 4;
    while (cap < needed)
        cap *= 2;

    glr_goto_t *new_buf =
        (glr_goto_t *)realloc(st->gotos, cap * sizeof(glr_goto_t));

    if (!new_buf)
        return -1;

    st->gotos = new_buf;
    st->goto_capacity = cap;
    return 0;
}

/* ============================================================
 * Lifecycle API
 * ============================================================ */

glr_parse_table_t *
glr_parse_table_create(size_t state_count,
                       size_t terminal_count,
                       size_t nonterminal_count)
{
    glr_parse_table_t *table =
        (glr_parse_table_t *)calloc(1, sizeof(glr_parse_table_t));
    if (!table)
        return NULL;

    table->state_count = state_count;
    table->terminal_count = terminal_count;
    table->nonterminal_count = nonterminal_count;

    table->states =
        (glr_state_t *)calloc(state_count, sizeof(glr_state_t));
    if (!table->states) {
        free(table);
        return NULL;
    }

    for (size_t i = 0; i < state_count; ++i) {
        glr_state_t *st = &table->states[i];

        st->terminal_count = terminal_count;

        /* Allocate ACTION table */
        st->action_table =
            (glr_action_set_t *)calloc(terminal_count,
                                      sizeof(glr_action_set_t));
        if (!st->action_table)
            goto fail;

        for (size_t t = 0; t < terminal_count; ++t)
            glr_action_set_init(&st->action_table[t]);

        st->gotos = NULL;
        st->goto_count = 0;
        st->goto_capacity = 0;
    }

    return table;

fail:
    glr_parse_table_destroy(table);
    return NULL;
}

void
glr_parse_table_destroy(glr_parse_table_t *table)
{
    if (!table)
        return;

    if (table->states) {
        for (size_t i = 0; i < table->state_count; ++i) {
            glr_state_t *st = &table->states[i];

            if (st->action_table) {
                for (size_t t = 0; t < st->terminal_count; ++t)
                    glr_action_set_destroy(&st->action_table[t]);

                free(st->action_table);
            }

            free(st->gotos);
        }

        free(table->states);
    }

    free(table);
}

/* ============================================================
 * Mutation API
 * ============================================================ */

int
glr_parse_table_add_action(glr_parse_table_t *table,
                          uint32_t state,
                          uint32_t terminal,
                          glr_action_t action)
{
    if (!table)
        return -1;

    if (state >= table->state_count ||
        terminal >= table->terminal_count)
        return -1;

    glr_state_t *st = &table->states[state];
    return glr_action_set_add(&st->action_table[terminal], action);
}

int
glr_parse_table_set_goto(glr_parse_table_t *table,
                         uint32_t state,
                         uint32_t nonterminal,
                         uint32_t next_state)
{
    if (!table)
        return -1;

    if (state >= table->state_count ||
        nonterminal >= table->nonterminal_count)
        return -1;

    glr_state_t *st = &table->states[state];

    /* Check if entry already exists → update */
    for (size_t i = 0; i < st->goto_count; ++i) {
        if (st->gotos[i].nonterminal_id == nonterminal) {
            st->gotos[i].next_state = next_state;
            return 0;
        }
    }

    /* Otherwise append */
    if (glr_state_reserve_goto(st, st->goto_count + 1) != 0)
        return -1;

    glr_goto_t *g = &st->gotos[st->goto_count++];
    g->nonterminal_id = nonterminal;
    g->next_state = next_state;

    return 0;
}

/* ============================================================
 * Query API
 * ============================================================ */

const glr_action_set_t *
glr_parse_table_get_actions(const glr_parse_table_t *table,
                           uint32_t state,
                           uint32_t terminal)
{
    if (!table)
        return NULL;

    if (state >= table->state_count ||
        terminal >= table->terminal_count)
        return NULL;

    return &table->states[state].action_table[terminal];
}

int
glr_parse_table_get_goto(const glr_parse_table_t *table,
                        uint32_t state,
                        uint32_t nonterminal,
                        uint32_t *out_next_state)
{
    if (!table || !out_next_state)
        return -1;

    if (state >= table->state_count)
        return -1;

    const glr_state_t *st = &table->states[state];

    for (size_t i = 0; i < st->goto_count; ++i) {
        if (st->gotos[i].nonterminal_id == nonterminal) {
            *out_next_state = st->gotos[i].next_state;
            return 0;
        }
    }

    return -1;
}

size_t
glr_parse_table_action_count(const glr_parse_table_t *table,
                             uint32_t state,
                             uint32_t terminal)
{
    const glr_action_set_t *set;

    if (!table)
        return 0;

    if (state >= table->state_count ||
        terminal >= table->terminal_count)
        return 0;

    set = &table->states[state].action_table[terminal];
    return set->action_count;
}

bool
glr_parse_table_has_conflict(const glr_parse_table_t *table,
                             uint32_t state,
                             uint32_t terminal)
{
    return glr_parse_table_action_count(table, state, terminal) > 1;
}

size_t
glr_parse_table_conflict_count(const glr_parse_table_t *table)
{
    size_t conflicts = 0;
    size_t s;
    size_t t;

    if (!table)
        return 0;

    for (s = 0; s < table->state_count; ++s) {
        for (t = 0; t < table->terminal_count; ++t) {
            if (glr_parse_table_has_conflict(table, (uint32_t)s,
                                             (uint32_t)t))
                conflicts++;
        }
    }

    return conflicts;
}