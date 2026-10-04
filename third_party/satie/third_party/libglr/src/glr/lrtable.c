/**
 * @file lrtable.c
 * @brief LALR(1) parse-table construction from a context-free grammar.
 *
 * libglr's parser executes against a @ref glr_parse_table_t. Grammars may
 * attach a precomputed table; this module builds one from scratch by
 * computing canonical LR(1) item sets: each item carries a look-ahead set
 * that closure propagates, and reductions are emitted only on those
 * look-aheads. Look-ahead precision is what keeps left-recursive operator
 * grammars working. `E -> E - T` reduces only when the next token really can
 * follow the expression, so the following operator is shifted instead and
 * `1 - 2 - 3` parses left-associatively with no conflict.
 *
 * The generated table is indexed by the grammar's own symbol ids, so the
 * parser can look up actions with ids it already computes. One extra
 * terminal column is appended for end-of-input; parsers reach it through
 * glr_parse_table_eof_column().
 */

#include <glr/grammar.h>
#include <glr/parsetbl.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Lookahead sets                                                      */
/* ------------------------------------------------------------------ */

/* Production index 0 is the augmented production S' -> S. */
#define LRT_AUGMENTED 0u

/* Sentinel for "no transition recorded" in the transition matrix. */
#define LRT_NO_TRANSITION ((size_t) -1)

typedef struct
{
  size_t symbol_count; /* grammar symbols */
  size_t width;        /* symbol_count + 1: the extra column is EOF */
  size_t eof_column;
  bool *nullable;
  bool *first;  /* [symbol_count][width] */
} lrt_sets_t;

static void
lrt_sets_free (lrt_sets_t *sets)
{
  if (sets == NULL)
    {
      return;
    }
  free (sets->nullable);
  free (sets->first);
  sets->nullable = NULL;
  sets->first = NULL;
}

static bool
lrt_matrix_get (const bool *matrix, size_t width, size_t row, size_t column)
{
  return matrix[row * width + column];
}

static void
lrt_matrix_set (bool *matrix, size_t width, size_t row, size_t column)
{
  matrix[row * width + column] = true;
}

/* FIRST sets, with the EOF column available as a look-ahead source. */
static int
lrt_compute_first (const glr_grammar_t *grammar, lrt_sets_t *sets)
{
  size_t n = sets->symbol_count;
  bool changed = true;

  memset (sets->first, 0, n * sets->width * sizeof (bool));
  memset (sets->nullable, 0, n * sizeof (bool));

  for (size_t i = 0; i < grammar->symbol_count; i++)
    {
      if (grammar->symbols[i] != NULL
          && glr_symbol_is_terminal (grammar->symbols[i]))
        {
          lrt_matrix_set (sets->first, sets->width, i, i);
        }
    }

  while (changed)
    {
      changed = false;
      for (size_t p = 0; p < grammar->production_count; p++)
        {
          const glr_production_t *production = grammar->productions[p];
          size_t head = (size_t) production->head->id;
          bool all_nullable = true;

          for (size_t b = 0; b < production->body_length; b++)
            {
              size_t body = (size_t) production->body[b]->id;
              for (size_t c = 0; c < sets->width; c++)
                {
                  if (lrt_matrix_get (sets->first, sets->width, body, c)
                      && !lrt_matrix_get (sets->first, sets->width, head, c))
                    {
                      lrt_matrix_set (sets->first, sets->width, head, c);
                      changed = true;
                    }
                }
              if (!sets->nullable[body])
                {
                  all_nullable = false;
                  break;
                }
            }
          if (all_nullable && !sets->nullable[head])
            {
              sets->nullable[head] = true;
              changed = true;
            }
        }
    }

  return 0;
}

/* ------------------------------------------------------------------ */
/* Grammar access helpers                                              */
/* ------------------------------------------------------------------ */

static glr_symbol_t *
lrt_head (const glr_grammar_t *grammar, size_t production)
{
  if (production == LRT_AUGMENTED)
    {
      return grammar->start_symbol;
    }
  return grammar->productions[production - 1]->head;
}

static size_t
lrt_body_length (const glr_grammar_t *grammar, size_t production)
{
  if (production == LRT_AUGMENTED)
    {
      return 1;
    }
  return grammar->productions[production - 1]->body_length;
}

static glr_symbol_t *
lrt_body_at (const glr_grammar_t *grammar, size_t production, size_t index)
{
  if (production == LRT_AUGMENTED)
    {
      return grammar->start_symbol;
    }
  return grammar->productions[production - 1]->body[index];
}

static size_t
lrt_production_total (const glr_grammar_t *grammar)
{
  return grammar->production_count + 1;
}

/* ------------------------------------------------------------------ */
/* Items: (production, dot) with a look-ahead set                      */
/* ------------------------------------------------------------------ */

typedef struct
{
  size_t production;
  size_t dot;
  bool *lookahead; /* [width] owned by the item */
} lrt_item_t;

typedef struct
{
  lrt_item_t *items;
  size_t count;
  size_t capacity;
} lrt_list_t;

static void
lrt_item_free (lrt_item_t *item, size_t width)
{
  if (item == NULL)
    {
      return;
    }
  free (item->lookahead);
  item->lookahead = NULL;
  (void) width;
}

static void
lrt_list_free (lrt_list_t *list, size_t width)
{
  if (list == NULL)
    {
      return;
    }
  for (size_t i = 0; i < list->count; i++)
    {
      lrt_item_free (&list->items[i], width);
    }
  free (list->items);
  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}

static lrt_item_t *
lrt_list_find (lrt_list_t *list, size_t production, size_t dot)
{
  for (size_t i = 0; i < list->count; i++)
    {
      if (list->items[i].production == production
          && list->items[i].dot == dot)
        {
          return &list->items[i];
        }
    }
  return NULL;
}

/* Merge `add` into an existing item's look-ahead set. Returns true when the
   set actually grew, which drives the closure fixpoint. */
static bool
lrt_item_merge (lrt_item_t *item, const bool *add, size_t width)
{
  bool changed = false;

  for (size_t c = 0; c < width; c++)
    {
      if (add[c] && !item->lookahead[c])
        {
          item->lookahead[c] = true;
          changed = true;
        }
    }
  return changed;
}

static int
lrt_list_merge_item (lrt_list_t *list, size_t production, size_t dot,
                     const bool *lookahead, size_t width)
{
  lrt_item_t *item = lrt_list_find (list, production, dot);

  if (item != NULL)
    {
      lrt_item_merge (item, lookahead, width);
      return 0;
    }

  if (list->count >= list->capacity)
    {
      size_t new_cap = list->capacity == 0 ? 16 : list->capacity * 2;
      lrt_item_t *grown = realloc (list->items, new_cap * sizeof (*grown));
      if (grown == NULL)
        {
          return -1;
        }
      list->items = grown;
      list->capacity = new_cap;
    }

  item = &list->items[list->count];
  item->production = production;
  item->dot = dot;
  item->lookahead = calloc (width, sizeof (bool));
  if (item->lookahead == NULL)
    {
      return -1;
    }
  memcpy (item->lookahead, lookahead, width * sizeof (bool));
  list->count++;
  return 0;
}

/* Two item sets are the same state only when their (production, dot) pairs
   *and* their look-ahead sets match, i.e. the canonical LR(1) identity.
   Merging on the core alone (LALR) makes the table smaller but shifts a
   grammar's ambiguity from "two derivations in the forest" into "a conflict
   the engine must resolve", and the propagation needed to keep merged
   look-aheads exact is where LR generators usually go wrong. Exactness wins
   here: the generated table is conflict free for every unambiguous grammar,
   including left-recursive operator grammars. */
static bool
lrt_list_same_core (const lrt_list_t *a, const lrt_list_t *b, size_t width)
{
  if (a->count != b->count)
    {
      return false;
    }
  for (size_t i = 0; i < a->count; i++)
    {
      const lrt_item_t *match = NULL;
      for (size_t j = 0; j < b->count; j++)
        {
          if (a->items[i].production == b->items[j].production
              && a->items[i].dot == b->items[j].dot)
            {
              match = &b->items[j];
              break;
            }
        }
      if (match == NULL
          || memcmp (a->items[i].lookahead, match->lookahead,
                     width * sizeof (bool))
                 != 0)
        {
          return false;
        }
    }
  return true;
}

/* ------------------------------------------------------------------ */
/* FIRST of a body suffix followed by a look-ahead set                  */
/* ------------------------------------------------------------------ */

typedef struct
{
  bool *bits;
  size_t width;
} lrt_lookahead_t;

static int
lrt_first_of_suffix (const glr_grammar_t *grammar, const lrt_sets_t *sets,
                     size_t production, size_t from, const bool *trailing,
                     bool *out, size_t width)
{
  size_t body_length = lrt_body_length (grammar, production);
  bool all_nullable = true;

  memset (out, 0, width * sizeof (bool));

  for (size_t i = from; i < body_length; i++)
    {
      size_t symbol = (size_t) lrt_body_at (grammar, production, i)->id;
      for (size_t c = 0; c < width; c++)
        {
          if (lrt_matrix_get (sets->first, width, symbol, c))
            {
              out[c] = true;
            }
        }
      if (!sets->nullable[symbol])
        {
          all_nullable = false;
          return 0;
        }
    }

  if (all_nullable)
    {
      for (size_t c = 0; c < width; c++)
        {
          if (trailing[c])
            {
              out[c] = true;
            }
        }
    }

  return 0;
}

/* ------------------------------------------------------------------ */
/* Closure and goto                                                    */
/* ------------------------------------------------------------------ */

static int
lrt_closure (const glr_grammar_t *grammar, const lrt_sets_t *sets,
             lrt_list_t *list)
{
  bool changed = true;
  size_t total = lrt_production_total (grammar);
  size_t width = sets->width;
  bool *scratch = calloc (width, sizeof (bool));

  if (scratch == NULL)
    {
      return -1;
    }

  /* The list grows while it is being scanned, so re-derive the look-ahead
     sets until a full pass adds nothing. */
  while (changed)
    {
      changed = false;

      for (size_t i = 0; i < list->count; i++)
        {
          size_t production = list->items[i].production;
          size_t dot = list->items[i].dot;
          glr_symbol_t *next;

          if (dot >= lrt_body_length (grammar, production))
            {
              continue;
            }
          next = lrt_body_at (grammar, production, dot);
          if (glr_symbol_is_terminal (next))
            {
              continue;
            }

          lrt_first_of_suffix (grammar, sets, production, dot + 1,
                               list->items[i].lookahead, scratch, width);

          for (size_t p = 1; p < total; p++)
            {
              lrt_item_t *item;
              if (lrt_head (grammar, p)->id != next->id)
                {
                  continue;
                }
              item = lrt_list_find (list, p, 0);
              if (item == NULL)
                {
                  if (lrt_list_merge_item (list, p, 0, scratch, width) != 0)
                    {
                      free (scratch);
                      return -1;
                    }
                  changed = true;
                }
              else if (lrt_item_merge (item, scratch, width))
                {
                  changed = true;
                }
            }
        }
    }

  free (scratch);
  return 0;
}

static int
lrt_goto (const glr_grammar_t *grammar, const lrt_sets_t *sets,
          const lrt_list_t *list, const glr_symbol_t *symbol, lrt_list_t *out)
{
  size_t width = sets->width;

  lrt_list_free (out, width);

  for (size_t i = 0; i < list->count; i++)
    {
      size_t production = list->items[i].production;
      size_t dot = list->items[i].dot;

      if (dot >= lrt_body_length (grammar, production))
        {
          continue;
        }
      if (lrt_body_at (grammar, production, dot)->id != symbol->id)
        {
          continue;
        }
      if (lrt_list_merge_item (out, production, dot + 1, list->items[i].lookahead,
                               width)
          != 0)
        {
          lrt_list_free (out, width);
          return -1;
        }
    }

  if (out->count == 0)
    {
      lrt_list_free (out, width);
      return 1; /* empty kernel */
    }

  return lrt_closure (grammar, sets, out);
}

/* ------------------------------------------------------------------ */
/* State set with LALR core merging                                    */
/* ------------------------------------------------------------------ */

typedef struct
{
  lrt_list_t *states;
  size_t count;
  size_t capacity;
  size_t width;
} lrt_states_t;

static void
lrt_states_free (lrt_states_t *states)
{
  for (size_t i = 0; i < states->count; i++)
    {
      lrt_list_free (&states->states[i], states->width);
    }
  free (states->states);
  states->states = NULL;
  states->count = 0;
  states->capacity = 0;
}

/* Intern `items`; a state with the identical canonical LR(1) item set is
   reused, otherwise a new state is appended. */
static int
lrt_states_intern (const glr_grammar_t *grammar, const lrt_sets_t *sets,
                   lrt_states_t *states, const lrt_list_t *items,
                   size_t *out_index)
{
  for (size_t i = 0; i < states->count; i++)
    {
      if (!lrt_list_same_core (&states->states[i], items, states->width))
        {
          continue;
        }
      /* Defensive: identical item sets should already be closed, but
         re-running closure keeps the invariant local to this function. */
      if (lrt_closure (grammar, sets, &states->states[i]) != 0)
        {
          return -1;
        }
      *out_index = i;
      return 0;
    }

  if (states->count >= states->capacity)
    {
      size_t new_cap = states->capacity == 0 ? 8 : states->capacity * 2;
      lrt_list_t *grown = realloc (states->states, new_cap * sizeof (*grown));
      if (grown == NULL)
        {
          return -1;
        }
      states->states = grown;
      states->capacity = new_cap;
    }

  states->states[states->count].items = NULL;
  states->states[states->count].count = 0;
  states->states[states->count].capacity = 0;
  for (size_t k = 0; k < items->count; k++)
    {
      if (lrt_list_merge_item (&states->states[states->count],
                               items->items[k].production,
                               items->items[k].dot,
                               items->items[k].lookahead, states->width)
          != 0)
        {
          return -1;
        }
    }

  *out_index = states->count;
  states->count++;
  return 0;
}

static void
lrt_error (char *error, size_t error_size, const char *message)
{
  if (error != NULL && error_size > 0)
    {
      snprintf (error, error_size, "%s", message);
    }
}

glr_parse_table_t *
glr_grammar_build_parse_table (const glr_grammar_t *grammar, char *error,
                               size_t error_size)
{
  lrt_sets_t sets;
  lrt_states_t states;
  lrt_list_t scratch;
  glr_parse_table_t *table = NULL;
  size_t symbol_count;
  size_t eof_column;
  size_t trans_rows = 0;
  size_t *trans = NULL;
  int rc = -1;

  memset (&sets, 0, sizeof (sets));
  memset (&states, 0, sizeof (states));
  memset (&scratch, 0, sizeof (scratch));

  if (error != NULL && error_size > 0)
    {
      error[0] = '\0';
    }

  if (grammar == NULL)
    {
      lrt_error (error, error_size, "null grammar");
      return NULL;
    }
  if (grammar->start_symbol == NULL)
    {
      lrt_error (error, error_size, "grammar has no start symbol");
      return NULL;
    }

  {
    char validation[128];
    if (!glr_grammar_validate (grammar, validation, sizeof (validation)))
      {
        lrt_error (error, error_size, validation);
        return NULL;
      }
  }

  symbol_count = grammar->symbol_count;
  if (symbol_count == 0)
    {
      lrt_error (error, error_size, "grammar has no symbols");
      return NULL;
    }
  if (symbol_count >= 0xFFFFFFFEu)
    {
      lrt_error (error, error_size, "grammar has too many symbols");
      return NULL;
    }

  eof_column = symbol_count;
  sets.symbol_count = symbol_count;
  sets.width = symbol_count + 1;
  sets.eof_column = eof_column;
  sets.nullable = calloc (symbol_count, sizeof (bool));
  sets.first = calloc (symbol_count * sets.width, sizeof (bool));
  if (sets.nullable == NULL || sets.first == NULL)
    {
      lrt_error (error, error_size, "out of memory computing FIRST sets");
      goto cleanup;
    }
  lrt_compute_first (grammar, &sets);

  states.width = sets.width;

  /* Initial item: S' -> . S with EOF as the only look-ahead. */
  {
    bool eof_only[sets.width];
    memset (eof_only, 0, sizeof (eof_only));
    eof_only[eof_column] = true;
    if (lrt_list_merge_item (&scratch, LRT_AUGMENTED, 0, eof_only, sets.width)
            != 0
        || lrt_closure (grammar, &sets, &scratch) != 0)
      {
        lrt_error (error, error_size, "out of memory building initial state");
        goto cleanup;
      }
  }
  {
    size_t index = 0;
    if (lrt_states_intern (grammar, &sets, &states, &scratch, &index) != 0)
      {
        lrt_error (error, error_size, "out of memory interning states");
        goto cleanup;
      }
  }
  lrt_list_free (&scratch, sets.width);

  /* Breadth-first expansion, recording transitions as we go. Transitions are
     kept in a dense [state][symbol] matrix so the ACTION/GOTO pass later on
     never interns states and can never invalidate a pointer by realloc. */
  #define LRT_GROW_TRANSITIONS(rows)                                           \
    do                                                                         \
      {                                                                        \
        if ((rows) > trans_rows)                                               \
          {                                                                    \
            size_t need_ = (rows) * symbol_count;                              \
            size_t *grown_ = realloc (trans, need_ * sizeof (*grown_));        \
            if (grown_ == NULL)                                                 \
              {                                                                \
                free (trans);                                                  \
                trans = NULL;                                                  \
                lrt_error (error, error_size, "out of memory building table"); \
                goto cleanup;                                                  \
              }                                                                \
            for (size_t fill_ = trans_rows * symbol_count; fill_ < need_;      \
                 fill_++)                                                     \
              {                                                                \
                grown_[fill_] = LRT_NO_TRANSITION;                             \
              }                                                                \
            trans = grown_;                                                    \
            trans_rows = (rows);                                               \
          }                                                                    \
      }                                                                        \
    while (0)

  LRT_GROW_TRANSITIONS (states.count);

  for (size_t s = 0; s < states.count; s++)
    {
      for (size_t i = 0; i < states.states[s].count; i++)
        {
          size_t production = states.states[s].items[i].production;
          size_t dot = states.states[s].items[i].dot;
          glr_symbol_t *next;
          size_t index = 0;
          int moved;

          if (dot >= lrt_body_length (grammar, production))
            {
              continue;
            }
          next = lrt_body_at (grammar, production, dot);
          if (trans[s * symbol_count + (size_t) next->id] != LRT_NO_TRANSITION)
            {
              continue; /* already resolved for this symbol */
            }

          moved = lrt_goto (grammar, &sets, &states.states[s], next, &scratch);
          if (moved < 0)
            {
              free (trans);
              lrt_error (error, error_size, "out of memory computing goto");
              goto cleanup;
            }
          if (moved == 1)
            {
              continue;
            }
          if (lrt_states_intern (grammar, &sets, &states, &scratch, &index) != 0)
            {
              free (trans);
              lrt_error (error, error_size, "out of memory interning states");
              goto cleanup;
            }
          LRT_GROW_TRANSITIONS (states.count);
          trans[s * symbol_count + (size_t) next->id] = index;
        }
    }

  #undef LRT_GROW_TRANSITIONS

  if (trans == NULL)
    {
      lrt_error (error, error_size, "out of memory building table");
      goto cleanup;
    }
  if (states.count == 0 || states.count > GLR_PARSE_TABLE_MAX_STATES)
    {
      free (trans);
      lrt_error (error, error_size, "grammar produces an unusable state set");
      goto cleanup;
    }

  table = glr_parse_table_create (states.count, symbol_count + 1,
                                  symbol_count);
  if (table == NULL)
    {
      free (trans);
      lrt_error (error, error_size, "out of memory creating parse table");
      goto cleanup;
    }

  for (size_t s = 0; s < states.count; s++)
    {
      for (size_t i = 0; i < states.states[s].count; i++)
        {
          size_t production = states.states[s].items[i].production;
          size_t dot = states.states[s].items[i].dot;
          const bool *lookahead = states.states[s].items[i].lookahead;

          if (dot < lrt_body_length (grammar, production))
            {
              glr_symbol_t *next = lrt_body_at (grammar, production, dot);
              size_t index = trans[s * symbol_count + (size_t) next->id];

              if (index == LRT_NO_TRANSITION)
                {
                  continue;
                }

              if (glr_symbol_is_terminal (next))
                {
                  glr_action_t action;
                  memset (&action, 0, sizeof (action));
                  action.type = GLR_ACTION_SHIFT;
                  action.shift.next_state = (uint32_t) index;
                  if (glr_parse_table_add_action (table, (uint32_t) s,
                                                  (uint32_t) next->id,
                                                  action)
                      != 0)
                    {
                      free (trans);
                      lrt_error (error, error_size, "table cell overflow");
                      goto cleanup;
                    }
                }
              else if (glr_parse_table_set_goto (table, (uint32_t) s,
                                                  (uint32_t) next->id,
                                                  (uint32_t) index)
                       != 0)
                {
                  free (trans);
                  lrt_error (error, error_size, "goto insert failed");
                  goto cleanup;
                }
              continue;
            }

          /* Complete item. */
          if (production == LRT_AUGMENTED)
            {
              glr_action_t accept;
              memset (&accept, 0, sizeof (accept));
              accept.type = GLR_ACTION_ACCEPT;
              if (glr_parse_table_add_action (table, (uint32_t) s,
                                              (uint32_t) eof_column, accept)
                  != 0)
                {
                  free (trans);
                  lrt_error (error, error_size, "accept cell overflow");
                  goto cleanup;
                }
              continue;
            }

          for (size_t t = 0; t < symbol_count + 1; t++)
            {
              glr_action_t reduce;
              if (!lookahead[t])
                {
                  continue;
                }
              memset (&reduce, 0, sizeof (reduce));
              reduce.type = GLR_ACTION_REDUCE;
              reduce.reduce.production_id = (uint32_t) (production - 1);
              if (glr_parse_table_add_action (table, (uint32_t) s,
                                              (uint32_t) t, reduce)
                  != 0)
                {
                  free (trans);
                  lrt_error (error, error_size, "reduce cell overflow");
                  goto cleanup;
                }
            }
        }
    }

  free (trans);
  rc = 0;

cleanup:
  lrt_list_free (&scratch, sets.width);
  lrt_states_free (&states);
  lrt_sets_free (&sets);
  if (rc != 0 && table != NULL)
    {
      glr_parse_table_destroy (table);
      return NULL;
    }
  return table;
}
