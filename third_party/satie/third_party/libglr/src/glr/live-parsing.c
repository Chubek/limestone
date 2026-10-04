/**
 * @file live-parsing.c
 * @brief Editor-facing incremental parsing.
 *
 * A session owns the text, the forest, and the metadata that ties them
 * together. An edit is reported in line/column coordinates, spliced into the
 * text immediately, and turned into a marked set of tree branches: the nodes
 * whose span overlaps the edit, plus every ancestor that could therefore
 * change. The session also keeps parse snapshots, which is what makes the
 * update cheap.
 *
 * The reason snapshots matter is that the parse of a prefix does not depend on
 * what follows it. A single splice at byte offset k leaves every byte before k
 * untouched, so the parser can be resumed from the newest snapshot at or
 * before k instead of starting over: the work is bounded by the text after the
 * edit rather than by the whole document. The session reports both the reused
 * and the re-parsed byte counts so the saving is visible rather than implied.
 *
 * Marking is what tells the application what the update intends to redo. The
 * tree is walked down to the nodes overlapping the edit and back up to their
 * ancestors, and both sets are kept: the overlapping nodes first, so the marked
 * list doubles as an explanation of the change.
 */

#include <glr/live-parsing.h>
#include <glr/scannerless.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

/* A snapshot as kept by the session: a copy of the parser's whole stack plus
   the position it reached. The forest nodes the entries pack are shared, not
   copied, which is what keeps a snapshot cheap and lets a resumed parse keep
   referring to the prefix it reused. */
struct live_snapshot
{
    glr_stack_t *stack;
    size_t position;
};

/* A node of the current tree together with the parents the marking pass needs
   and whether it is already part of the marked set. */
struct live_node_ref
{
    glr_forest_node_t *node;
    glr_forest_node_t **parents;
    size_t parent_count;
    size_t parent_capacity;
    bool marked;
};

struct glr_live_parser_t
{
    glr_grammar_t *grammar; /* borrowed */
    glr_parser_t *parser;   /* owned */

    char *text;      /* owned, NUL-terminated for convenience */
    size_t text_length;
    size_t text_capacity;

    glr_forest_t *forest; /* owned; the cached tree */

    struct live_snapshot *snapshots;
    size_t snapshot_count;
    size_t snapshot_capacity;

    struct live_node_ref *nodes;
    size_t node_count;
    size_t node_capacity;

    /* The region the last edit touched: `edit_start`..`edit_old_end` in the
       coordinates of the text the cached tree was built from, and
       `edit_start`..`edit_end` in the coordinates of the new text. */
    size_t edit_start;
    size_t edit_old_end;
    size_t edit_end;

    /* Where the last update resumed from, for the statistics. */
    size_t resume_position;
    bool has_edit;

    glr_live_parser_stats_t stats;
    char *error; /* owned, NULL when the last update succeeded */
};

/* The signature matches the query layer's so the two report failures the same
   way; the implementation is local because live parsing does not depend on
   queries. */
static void
live_set_error (char *buffer, size_t size, const char *message)
{
    if (buffer != NULL && size > 0)
    {
        snprintf (buffer, size, "%s", message);
    }
}

static void
live_remember_error (glr_live_parser_t *live, const char *message)
{
    char *copy;

    if (message == NULL)
    {
        free (live->error);
        live->error = NULL;
        return;
    }
    copy = malloc (strlen (message) + 1);
    if (copy == NULL)
    {
        return;
    }
    strcpy (copy, message);
    free (live->error);
    live->error = copy;
}

static const char *
live_error_text (glr_parse_error_t error)
{
    switch (error)
    {
    case GLR_PARSE_ERROR_MEMORY:
        return "out of memory while parsing";
    case GLR_PARSE_ERROR_SYNTAX:
        return "input does not match the grammar";
    case GLR_PARSE_ERROR_GRAMMAR:
        return "grammar is not usable for parsing";
    case GLR_PARSE_ERROR_UNRECOVERABLE:
        return "no reading of the input survives";
    case GLR_PARSE_ERROR_SEMANTIC:
        return "semantic evaluation failed";
    default:
        break;
    }
    return "parse failed";
}

/* ------------------------------------------------------------------ */
/* Snapshot sink                                                       */
/* ------------------------------------------------------------------ */

static void
live_drop_snapshots (glr_live_parser_t *live)
{
    for (size_t i = 0; i < live->snapshot_count; i++)
    {
        glr_stack_destroy (live->snapshots[i].stack);
    }
    live->snapshot_count = 0;
}

/* Called by the parser once per position. Only the furthest position reached
   so far is kept per parse, since that is the only snapshot a later edit
   benefits from when it lands anywhere earlier. */
static void
live_snapshot_sink (glr_stack_t *stack, size_t position, void *user_data)
{
    glr_live_parser_t *live = user_data;
    glr_stack_t *copy;

    if (live == NULL || stack == NULL)
    {
        return;
    }
    if (live->snapshot_count > 0
        && live->snapshots[live->snapshot_count - 1].position == position)
    {
        return; /* same position from another stack: one snapshot is enough */
    }
    if (live->snapshot_count >= live->snapshot_capacity)
    {
        size_t new_capacity
            = live->snapshot_capacity == 0 ? 32 : live->snapshot_capacity * 2;
        struct live_snapshot *grown = realloc (
            live->snapshots, new_capacity * sizeof (*grown));
        if (grown == NULL)
        {
            return; /* snapshots are an optimization, not a requirement */
        }
        live->snapshots = grown;
        live->snapshot_capacity = new_capacity;
    }

    copy = glr_stack_copy (stack);
    if (copy == NULL)
    {
        return;
    }
    live->snapshots[live->snapshot_count].stack = copy;
    live->snapshots[live->snapshot_count].position = position;
    live->snapshot_count++;
}

/* ------------------------------------------------------------------ */
/* Text buffer and position conversion                                 */
/* ------------------------------------------------------------------ */

static bool
live_text_reserve (glr_live_parser_t *live, size_t needed)
{
    size_t capacity;
    char *grown;

    if (needed + 1 <= live->text_capacity)
    {
        return true;
    }
    capacity = live->text_capacity == 0 ? 256 : live->text_capacity;
    while (capacity < needed + 1)
    {
        capacity *= 2;
    }
    grown = realloc (live->text, capacity);
    if (grown == NULL)
    {
        return false;
    }
    live->text = grown;
    live->text_capacity = capacity;
    return true;
}

static int
live_text_set (glr_live_parser_t *live, const char *text, size_t length)
{
    if (text == NULL && length > 0)
    {
        return -1;
    }
    if (!live_text_reserve (live, length))
    {
        return -1;
    }
    if (length > 0)
    {
        memcpy (live->text, text, length);
    }
    live->text[length] = '\0';
    live->text_length = length;
    return 0;
}

static int
live_text_splice (glr_live_parser_t *live, size_t start, size_t end,
                  const char *replacement, size_t replacement_length)
{
    size_t tail_length;
    size_t new_length;

    if (replacement == NULL && replacement_length > 0)
    {
        return -1;
    }
    if (start > end || end > live->text_length)
    {
        return -1;
    }

    tail_length = live->text_length - end;
    new_length = live->text_length - (end - start) + replacement_length;
    if (!live_text_reserve (live, new_length))
    {
        return -1;
    }

    if (replacement_length > 0)
    {
        memmove (live->text + start + replacement_length, live->text + end,
                 tail_length);
        memcpy (live->text + start, replacement, replacement_length);
    }
    else
    {
        memmove (live->text + start, live->text + end, tail_length);
    }
    live->text[new_length] = '\0';
    live->text_length = new_length;
    return 0;
}

int
glr_live_offset_of (const char *text, size_t length, uint32_t line,
                    uint32_t column, size_t *out_offset)
{
    size_t offset = 0;
    uint32_t current_line = 1;

    if (out_offset == NULL || line == 0 || column == 0)
    {
        return -1;
    }
    if (text == NULL && length > 0)
    {
        return -1;
    }

    while (current_line < line)
    {
        while (offset < length && text[offset] != '\n')
        {
            offset++;
        }
        if (offset >= length)
        {
            return -1; /* no such line */
        }
        offset++; /* step over the newline */
        current_line++;
    }

    if (offset > length || (size_t)(column - 1) > length - offset)
    {
        return -1; /* column past the end of the line */
    }
    *out_offset = offset + (column - 1);
    return 0;
}

int
glr_live_line_column_of (const char *text, size_t length, size_t offset,
                         uint32_t *out_line, uint32_t *out_column)
{
    size_t line_start = 0;
    uint32_t line = 1;

    if (out_line == NULL || out_column == NULL)
    {
        return -1;
    }
    if (text == NULL || offset > length)
    {
        return -1;
    }

    for (size_t cursor = 0; cursor < offset; cursor++)
    {
        if (text[cursor] == '\n')
        {
            line++;
            line_start = cursor + 1;
        }
    }
    *out_line = line;
    *out_column = (uint32_t)(offset - line_start) + 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Marking the branches an edit can reach                              */
/* ------------------------------------------------------------------ */

static void
live_clear_marks (glr_live_parser_t *live)
{
    for (size_t i = 0; i < live->node_count; i++)
    {
        free (live->nodes[i].parents);
    }
    live->node_count = 0;
}

static struct live_node_ref *
live_node_record (glr_live_parser_t *live, glr_forest_node_t *node)
{
    size_t i;

    for (i = 0; i < live->node_count; i++)
    {
        if (live->nodes[i].node == node)
        {
            return &live->nodes[i];
        }
    }
    if (live->node_count >= live->node_capacity)
    {
        size_t new_capacity
            = live->node_capacity == 0 ? 32 : live->node_capacity * 2;
        struct live_node_ref *grown = realloc (
            live->nodes, new_capacity * sizeof (*grown));
        if (grown == NULL)
        {
            return NULL;
        }
        live->nodes = grown;
        live->node_capacity = new_capacity;
    }

    live->nodes[live->node_count].node = node;
    live->nodes[live->node_count].parents = NULL;
    live->nodes[live->node_count].parent_count = 0;
    live->nodes[live->node_count].parent_capacity = 0;
    live->nodes[live->node_count].marked = false;
    return &live->nodes[live->node_count++];
}

/* Register every node of the forest, then link each to its parents. The SPPF
   has no parent pointers, and the upwards half of the marking walk needs them,
   so the index is rebuilt per edit rather than kept inside the forest, which
   keeps editor bookkeeping out of the parser's data structure. */
static bool
live_index_parents (glr_live_parser_t *live)
{
    live_clear_marks (live);
    if (live->forest == NULL)
    {
        return true;
    }

    for (size_t pos = 0; pos < live->forest->node_count; pos++)
    {
        for (glr_forest_node_t *node = live->forest->nodes[pos]; node != NULL;
             node = node->next)
        {
            if (live_node_record (live, node) == NULL)
            {
                return false;
            }
        }
    }

    for (size_t pos = 0; pos < live->forest->node_count; pos++)
    {
        for (glr_forest_node_t *node = live->forest->nodes[pos]; node != NULL;
             node = node->next)
        {
            for (size_t c = 0; c < node->child_count; c++)
            {
                glr_forest_node_t *child = node->children[c];
                struct live_node_ref *ref = NULL;

                if (child == NULL)
                {
                    continue;
                }
                for (size_t i = 0; i < live->node_count; i++)
                {
                    if (live->nodes[i].node == child)
                    {
                        ref = &live->nodes[i];
                        break;
                    }
                }
                if (ref == NULL)
                {
                    continue;
                }
                if (ref->parent_count >= ref->parent_capacity)
                {
                    size_t new_capacity = ref->parent_capacity == 0
                                              ? 2
                                              : ref->parent_capacity * 2;
                    glr_forest_node_t **grown = realloc (
                        ref->parents, new_capacity * sizeof (*grown));
                    if (grown == NULL)
                    {
                        return false;
                    }
                    ref->parents = grown;
                    ref->parent_capacity = new_capacity;
                }
                ref->parents[ref->parent_count++] = node;
            }
        }
    }
    return true;
}

static bool
live_node_overlaps (const glr_forest_node_t *node, size_t start, size_t end)
{
    if (node == NULL)
    {
        return false;
    }
    /* A node is affected when the edited bytes fall inside its span. The
       zero-width case covers a reduction that occupies no bytes: it is
       affected when the edit touches the position it sits at. */
    if (node->position == node->end_position)
    {
        return node->position >= start && node->position <= end;
    }
    return node->position < end && node->end_position > start;
}

static void
live_mark_ancestors (glr_live_parser_t *live, glr_forest_node_t *node,
                     size_t depth)
{
    struct live_node_ref *ref;

    if (node == NULL || depth > live->node_count + 1)
    {
        return; /* the forest is a DAG; the bound breaks any cycle */
    }
    ref = live_node_record (live, node);
    if (ref == NULL || ref->marked)
    {
        return;
    }
    ref->marked = true;

    for (size_t i = 0; i < ref->parent_count; i++)
    {
        live_mark_ancestors (live, ref->parents[i], depth + 1);
    }
}

static bool
live_mark_dirty (glr_live_parser_t *live, size_t start, size_t end)
{
    if (live->forest == NULL)
    {
        return true;
    }
    if (!live_index_parents (live))
    {
        return false;
    }

    /* Downwards: every node whose span the edit can have changed. */
    for (size_t pos = 0; pos < live->forest->node_count; pos++)
    {
        for (glr_forest_node_t *node = live->forest->nodes[pos]; node != NULL;
             node = node->next)
        {
            if (live_node_overlaps (node, start, end))
            {
                live_node_record (live, node);
            }
        }
    }

    /* Upwards: from each overlapping node to the root. The count is captured
       before the walk because marking adds records. */
    {
        size_t overlapping = live->node_count;
        for (size_t i = 0; i < overlapping; i++)
        {
            live_mark_ancestors (live, live->nodes[i].node, 0);
        }
    }
    return true;
}

/* Unlink every node whose span overlaps the edited range.
   A node that spans the edit is stale by definition, and so is any node that
   contains one, which is exactly the marked set. Removing them from the
   position chains is enough: the only pointers into them live in other stale
   nodes, and the resumed parse packs fresh ones. */
static void
live_prune_range (glr_forest_t *forest, size_t start, size_t end)
{
    glr_forest_node_t *root;
    size_t root_start = 0;
    size_t root_end = 0;

    if (forest == NULL)
    {
        return;
    }

    /* A root that spans the edit is stale, so note its span and detach it
       before the walk: the walk may free the node itself, and the resumed
       parse installs a fresh root when it accepts. */
    root = forest->root;
    forest->root = NULL;
    if (root != NULL)
    {
        root_start = root->position;
        root_end = root->end_position;
    }

    for (size_t pos = 0; pos < forest->node_count; pos++)
    {
        glr_forest_node_t **slot = &forest->nodes[pos];

        while (*slot != NULL)
        {
            glr_forest_node_t *node = *slot;

            /* A node is affected when the edited bytes fall inside its span.
               A node that merely ends where the edit begins is untouched,
               which is what keeps a resumable snapshot's prefix valid. */
            if ((node->position < end && node->end_position > start)
                || (node->position == node->end_position
                    && node->position >= start && node->position <= end))
            {
                *slot = node->next;
                node->next = NULL;
                glr_forest_node_destroy (node);
                continue;
            }
            slot = &node->next;
        }
    }

    /* The root survives only when the edit does not fall inside its span. */
    if (root != NULL
        && !((root_start < end && root_end > start)
             || (root_start == root_end && root_start >= start
                 && root_start <= end)))
    {
        forest->root = root;
    }
}

/* Move the nodes that sit entirely after the old end of the edit to where the
   new text puts them. */
static void
live_shift_suffix (glr_forest_t *forest, size_t old_end, ssize_t delta)
{
    if (forest == NULL || delta == 0)
    {
        return;
    }
    for (size_t pos = 0; pos < forest->node_count; pos++)
    {
        for (glr_forest_node_t *node = forest->nodes[pos]; node != NULL;
             node = node->next)
        {
            if (node->position >= old_end)
            {
                node->position = (size_t)((ssize_t) node->position + delta);
            }
            if (node->end_position >= old_end)
            {
                node->end_position
                    = (size_t)((ssize_t) node->end_position + delta);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Parsing                                                             */
/* ------------------------------------------------------------------ */

static bool
live_parse_all (glr_live_parser_t *live, glr_forest_t **out_forest,
                glr_parse_error_t *out_error)
{
    glr_parse_result_t result;
    glr_forest_t *forest;

    live_drop_snapshots (live);

    result = glr_parse (live->parser, live->text, live->text_length);
    if (result.error != GLR_PARSE_SUCCESS)
    {
        if (out_error != NULL)
        {
            *out_error = result.error;
        }
        return false;
    }

    /* Take the forest rather than copying it: the snapshots just taken point
       into it, and a copy would leave the session's tree and its snapshots
       describing two different forests. */
    forest = glr_parser_take_forest (live->parser);
    if (forest == NULL)
    {
        if (out_error != NULL)
        {
            *out_error = GLR_PARSE_ERROR_MEMORY;
        }
        return false;
    }
    *out_forest = forest;
    return true;
}

glr_live_parser_t *
glr_live_parser_create (glr_grammar_t *grammar, const char *text,
                        size_t length, char *error, size_t error_size)
{
    glr_live_parser_t *live;
    glr_forest_t *forest = NULL;
    glr_parse_error_t parse_error = GLR_PARSE_SUCCESS;

    live_set_error (error, error_size, "");

    if (grammar == NULL)
    {
        live_set_error (error, error_size, "grammar is required");
        return NULL;
    }
    if (grammar->start_symbol == NULL)
    {
        live_set_error (error, error_size, "grammar has no start symbol");
        return NULL;
    }
    if (text == NULL && length > 0)
    {
        live_set_error (error, error_size, "text buffer is null");
        return NULL;
    }

    live = calloc (1, sizeof (*live));
    if (live == NULL)
    {
        live_set_error (error, error_size, "out of memory creating session");
        return NULL;
    }
    live->grammar = grammar;
    live->parser = glr_parser_create (grammar);
    if (live->parser == NULL)
    {
        free (live);
        live_set_error (error, error_size, "out of memory creating parser");
        return NULL;
    }
    glr_parser_set_snapshot_hook (live->parser, live_snapshot_sink, live);

    if (live_text_set (live, text, length) != 0)
    {
        glr_parser_destroy (live->parser);
        free (live);
        live_set_error (error, error_size, "out of memory copying text");
        return NULL;
    }

    if (!live_parse_all (live, &forest, &parse_error))
    {
        /* The failed parse may already have recorded snapshots, so the session
           is torn down the same way destroy() would. */
        glr_parser_set_snapshot_hook (live->parser, NULL, NULL);
        live_drop_snapshots (live);
        free (live->snapshots);
        glr_parser_destroy (live->parser);
        free (live->text);
        free (live);
        live_set_error (error, error_size, live_error_text (parse_error));
        return NULL;
    }

    live->forest = forest;
    live->stats.total_nodes = glr_forest_total_nodes (forest);
    live->stats.input_length = length;
    live->stats.last_parse_succeeded = true;
    live->stats.bytes_reparsed = length;
    return live;
}

void
glr_live_parser_destroy (glr_live_parser_t *live)
{
    if (live == NULL)
    {
        return;
    }
    glr_parser_set_snapshot_hook (live->parser, NULL, NULL);
    live_clear_marks (live);
    free (live->nodes);
    live_drop_snapshots (live);
    free (live->snapshots);
    glr_forest_destroy (live->forest);
    glr_parser_destroy (live->parser);
    free (live->text);
    free (live->error);
    free (live);
}

int
glr_live_parser_set_text (glr_live_parser_t *live, const char *text,
                          size_t length, char *error, size_t error_size)
{
    glr_forest_t *forest = NULL;
    glr_parse_error_t parse_error = GLR_PARSE_SUCCESS;

    live_set_error (error, error_size, "");
    if (live == NULL)
    {
        live_set_error (error, error_size, "session is null");
        return -1;
    }
    if (text == NULL && length > 0)
    {
        live_set_error (error, error_size, "text buffer is null");
        return -1;
    }
    if (live_text_set (live, text, length) != 0)
    {
        live_set_error (error, error_size, "out of memory copying text");
        return -1;
    }

    if (!live_parse_all (live, &forest, &parse_error))
    {
        live_remember_error (live, live_error_text (parse_error));
        glr_forest_destroy (live->forest);
        live->forest = NULL;
        live_clear_marks (live);
        live->has_edit = false;
        live->stats.last_parse_succeeded = false;
        live->stats.total_nodes = 0;
        live->stats.input_length = live->text_length;
        live_set_error (error, error_size, live_error_text (parse_error));
        return -1;
    }

    glr_forest_destroy (live->forest);
    live->forest = forest;
    live_clear_marks (live);
    live->has_edit = false;
    live->stats.dirty_node_count = 0;
    live->stats.total_nodes = glr_forest_total_nodes (forest);
    live->stats.input_length = live->text_length;
    live->stats.last_parse_succeeded = true;
    live->stats.bytes_reparsed = live->text_length;
    live->stats.bytes_reused = 0;
    live_remember_error (live, NULL);
    return 0;
}

int
glr_live_parser_edit (glr_live_parser_t *live, const glr_live_edit_t *edit,
                      char *error, size_t error_size)
{
    size_t start = 0;
    size_t end = 0;
    char message[160];

    live_set_error (error, error_size, "");
    if (live == NULL)
    {
        live_set_error (error, error_size, "session is null");
        return -1;
    }
    if (edit == NULL)
    {
        live_set_error (error, error_size, "edit is null");
        return -1;
    }
    if (edit->replacement == NULL && edit->replacement_length > 0)
    {
        live_set_error (error, error_size, "edit replacement is null");
        return -1;
    }

    /* The end position is exclusive, so an insertion is spelled with two equal
       positions and a deletion with a non-empty span and no replacement. */
    if (glr_live_offset_of (live->text, live->text_length, edit->start_line,
                            edit->start_column, &start)
        != 0)
    {
        snprintf (message, sizeof (message),
                  "edit start %u:%u is outside the text", edit->start_line,
                  edit->start_column);
        live_set_error (error, error_size, message);
        return -1;
    }
    if (glr_live_offset_of (live->text, live->text_length, edit->end_line,
                            edit->end_column, &end)
        != 0)
    {
        snprintf (message, sizeof (message),
                  "edit end %u:%u is outside the text", edit->end_line,
                  edit->end_column);
        live_set_error (error, error_size, message);
        return -1;
    }
    if (end < start)
    {
        live_set_error (error, error_size, "edit ends before it starts");
        return -1;
    }
    if (end > live->text_length)
    {
        live_set_error (error, error_size, "edit ends past the text");
        return -1;
    }
    if (start == end && edit->replacement_length == 0)
    {
        live_set_error (error, error_size, "edit replaces nothing with nothing");
        return -1;
    }

    /* Mark against the old tree, then splice, so the overlap test sees the
       coordinates the cached tree was built with. */
    if (!live_mark_dirty (live, start, end))
    {
        live_set_error (error, error_size, "out of memory marking branches");
        return -1;
    }

    if (live_text_splice (live, start, end, edit->replacement,
                          edit->replacement_length)
        != 0)
    {
        live_set_error (error, error_size, "out of memory applying edit");
        return -1;
    }

    live->edit_start = start;
    live->edit_old_end = end;
    live->edit_end = start + edit->replacement_length;
    live->has_edit = true;
    live->stats.edit_count++;
    live->stats.dirty_node_count = live->node_count;
    live->stats.input_length = live->text_length;
    return 0;
}

/* The newest snapshot the edit leaves valid. A single splice at the edit start
   keeps every byte before it, so any snapshot at or before that offset still
   describes the text it was taken from. */
static const struct live_snapshot *
live_pick_resume (const glr_live_parser_t *live, size_t limit,
                  size_t *out_position)
{
    const struct live_snapshot *best = NULL;

    for (size_t i = 0; i < live->snapshot_count; i++)
    {
        if (live->snapshots[i].position > limit)
        {
            break; /* snapshots are recorded in increasing position order */
        }
        if (best == NULL || live->snapshots[i].position > best->position)
        {
            best = &live->snapshots[i];
        }
    }
    if (best != NULL && out_position != NULL)
    {
        *out_position = best->position;
    }
    return best;
}

int
glr_live_parser_update (glr_live_parser_t *live, char *error,
                        size_t error_size)
{
    glr_stack_t *resume = NULL;
    glr_parse_result_t result;
    ssize_t delta;
    size_t resume_position = 0;
    size_t remaining;
    int rc;

    live_set_error (error, error_size, "");
    if (live == NULL)
    {
        live_set_error (error, error_size, "session is null");
        return -1;
    }
    if (!live->has_edit)
    {
        return 0; /* the tree already matches the text */
    }

    /* Pick the resume point *before* dropping the snapshots: they are the only
       record of where the previous parse got to, and the parse that follows
       re-emits its own. The chosen one is copied out first so the rest can go. */
    {
        /* A pattern may grow across the edit boundary, and a single saved
           stack cannot represent every lexical/packed alternative. Rebuild
           those parses so edits preserve the complete language/forest. */
        const struct live_snapshot *picked = NULL;
        if (!glr_scannerless_has_patterns (live->grammar)
            && !live->parser->scannerless
            && (live->forest == NULL || !glr_forest_is_ambiguous (live->forest->root)))
            picked = live_pick_resume (live, live->edit_start, &resume_position);
        if (picked != NULL)
        {
            resume = glr_stack_copy (picked->stack);
        }
        live->resume_position = resume_position;
    }
    live_drop_snapshots (live);
    if (live->resume_position > 0 && resume == NULL)
    {
        resume_position = 0; /* the copy failed: fall back to a full parse */
        live->resume_position = 0;
    }

    if (resume == NULL || live->forest == NULL)
    {
        /* Nothing valid to resume from: parse the whole document. */
        glr_forest_t *forest = NULL;
        glr_parse_error_t parse_error = GLR_PARSE_SUCCESS;

        if (!live_parse_all (live, &forest, &parse_error))
        {
            glr_forest_destroy (live->forest);
            live->forest = NULL;
            live_clear_marks (live);
            live->has_edit = false;
            live->stats.last_parse_succeeded = false;
            live->stats.total_nodes = 0;
            live->stats.input_length = live->text_length;
            live_remember_error (live, live_error_text (parse_error));
            live_set_error (error, error_size,
                                 live_error_text (parse_error));
            return -1;
        }
        glr_forest_destroy (live->forest);
        live->forest = forest;
    }
    else
    {
        /* The cached forest keeps its identity: the snapshot's entries point
           into it, so the nodes the edit invalidated are pruned and the ones
           after the edit are shifted, then the parse continues into the same
           forest. */
        delta = (ssize_t) live->edit_end - (ssize_t) live->edit_old_end;
        live_prune_range (live->forest, live->edit_start, live->edit_old_end);
        live_shift_suffix (live->forest, live->edit_old_end, delta);

        /* The resumed parse copies the stack it is given, so this one is only
           needed for the call. */
        rc = glr_parser_parse_from (live->parser, resume, live->forest,
                                    resume_position, live->text,
                                    live->text_length, &result);
        glr_stack_destroy (resume);
        if (rc != 0)
        {
            glr_forest_destroy (live->forest);
            live->forest = NULL;
            live_clear_marks (live);
            live->has_edit = false;
            live->stats.last_parse_succeeded = false;
            live->stats.total_nodes = 0;
            live->stats.input_length = live->text_length;
            live_remember_error (live, live_error_text (result.error));
            live_set_error (error, error_size,
                                 live_error_text (result.error));
            return -1;
        }

        /* The resumed parse re-emits snapshots for the text it covered, so the
           next edit can resume from them too. */
    }

    live_clear_marks (live);
    live->has_edit = false;
    remaining = live->text_length > resume_position
                    ? live->text_length - resume_position
                    : live->text_length;

    live->stats.update_count++;
    live->stats.bytes_reparsed = remaining;
    live->stats.bytes_reused = resume_position;
    live->stats.dirty_node_count = 0;
    live->stats.total_nodes = glr_forest_total_nodes (live->forest);
    live->stats.input_length = live->text_length;
    live->stats.last_parse_succeeded = true;
    live_remember_error (live, NULL);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Observation                                                         */
/* ------------------------------------------------------------------ */

bool
glr_live_parser_has_pending_edits (const glr_live_parser_t *live)
{
    return live != NULL && live->has_edit;
}

const glr_forest_t *
glr_live_parser_forest (const glr_live_parser_t *live)
{
    return live != NULL ? live->forest : NULL;
}

const char *
glr_live_parser_text (const glr_live_parser_t *live, size_t *length)
{
    if (live == NULL)
    {
        if (length != NULL)
        {
            *length = 0;
        }
        return NULL;
    }
    if (length != NULL)
    {
        *length = live->text_length;
    }
    return live->text;
}

const char *
glr_live_parser_error (const glr_live_parser_t *live)
{
    return live != NULL ? live->error : NULL;
}

int
glr_live_parser_get_stats (const glr_live_parser_t *live,
                           glr_live_parser_stats_t *stats)
{
    if (live == NULL || stats == NULL)
    {
        return -1;
    }
    *stats = live->stats;
    return 0;
}

size_t
glr_live_parser_dirty_count (const glr_live_parser_t *live)
{
    return live != NULL ? live->node_count : 0;
}

const glr_forest_node_t *
glr_live_parser_dirty_node (const glr_live_parser_t *live, size_t index,
                            size_t *out_start, size_t *out_end)
{
    const glr_forest_node_t *node;

    if (live == NULL || index >= live->node_count)
    {
        return NULL;
    }
    node = live->nodes[index].node;
    if (out_start != NULL)
    {
        *out_start = node != NULL ? node->position : 0;
    }
    if (out_end != NULL)
    {
        *out_end = node != NULL ? node->end_position : 0;
    }
    return node;
}

bool
glr_live_parser_last_edit_range (const glr_live_parser_t *live, size_t *start,
                                 size_t *end)
{
    if (live == NULL)
    {
        return false;
    }
    if (start != NULL)
    {
        *start = live->edit_start;
    }
    if (end != NULL)
    {
        *end = live->edit_end;
    }
    return live->has_edit;
}
