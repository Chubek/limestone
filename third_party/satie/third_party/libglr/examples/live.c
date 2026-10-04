/**
 * @file live.c
 * @brief Editor-facing incremental parsing, end to end.
 *
 * Starts a session on a document, reports a series of edits as line/column
 * ranges the way an editor would, and prints what each update had to re-parse.
 * The point of the example is the last column of the table: an edit near the
 * end of the document costs a couple of bytes rather than the whole file,
 * because the parse resumes from the newest snapshot the edit left valid.
 *
 * Run it with no arguments for the built-in script, or pass a document and up
 * to three edits as `line:col` positions:
 *
 *     ./live "n+n*n" 1:2 1:3
 */

#include <glr/glr.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Expr -> Expr '+' Expr | Expr '-' Expr | Term,
   Term -> Term '*' Factor | Factor,
   Factor -> n | '(' Expr ')'

   The terminals are single characters, so the grammar-driven tokenizer is
   unambiguous and the example is about incremental parsing rather than about
   lexical analysis. */
static glr_grammar_t *
expression_grammar (void)
{
    glr_grammar_t *grammar = glr_grammar_create ();
    int expr;
    int term;
    int factor;
    int n;
    int plus;
    int minus;
    int star;
    int lp;
    int rp;
    glr_symbol_t *body[3];

    if (grammar == NULL)
    {
        return NULL;
    }

    expr = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Expr");
    term = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Term");
    factor
        = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Factor");
    n = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "n");
    plus = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
    minus = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "-");
    star = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "*");
    lp = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "(");
    rp = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, ")");

    body[0] = glr_grammar_get_symbol (grammar, term);
    glr_grammar_add_production (grammar, expr, body, 1);
    body[0] = glr_grammar_get_symbol (grammar, expr);
    body[1] = glr_grammar_get_symbol (grammar, plus);
    body[2] = glr_grammar_get_symbol (grammar, expr);
    glr_grammar_add_production (grammar, expr, body, 3);
    body[1] = glr_grammar_get_symbol (grammar, minus);
    glr_grammar_add_production (grammar, expr, body, 3);
    body[0] = glr_grammar_get_symbol (grammar, term);
    body[1] = glr_grammar_get_symbol (grammar, star);
    body[2] = glr_grammar_get_symbol (grammar, factor);
    glr_grammar_add_production (grammar, term, body, 3);
    body[0] = glr_grammar_get_symbol (grammar, factor);
    glr_grammar_add_production (grammar, term, body, 1);
    body[0] = glr_grammar_get_symbol (grammar, n);
    glr_grammar_add_production (grammar, factor, body, 1);
    body[0] = glr_grammar_get_symbol (grammar, lp);
    body[1] = glr_grammar_get_symbol (grammar, expr);
    body[2] = glr_grammar_get_symbol (grammar, rp);
    glr_grammar_add_production (grammar, factor, body, 3);

    glr_grammar_set_start_symbol (grammar, expr);
    return grammar;
}

/* Count the terminals in a packed forest, which is a cheap way to show that the
   tree really covers the document after an edit. */
static size_t
count_leaves (const glr_forest_node_t *node, const glr_forest_node_t **seen,
              size_t *seen_count)
{
    size_t total = 0;

    if (node == NULL)
    {
        return 0;
    }
    /* The forest is a DAG, so a node is counted once even when several parents
       share it. */
    for (size_t i = 0; i < *seen_count; i++)
    {
        if (seen[i] == node)
        {
            return 0;
        }
    }
    if (*seen_count < 4096)
    {
        seen[(*seen_count)++] = node;
    }
    if (node->type == GLR_NODE_TERMINAL)
    {
        return 1;
    }
    for (size_t i = 0; i < node->child_count; i++)
    {
        total += count_leaves (node->children[i], seen, seen_count);
    }
    return total;
}

static void
report_status (const glr_live_parser_t *live, const char *label)
{
    glr_live_parser_stats_t stats;

    if (glr_live_parser_get_stats (live, &stats) != 0)
    {
        return;
    }
    printf ("  %-22s text=%-14s nodes=%-4zu reused=%-4llu reparsed=%-4llu%s\n",
            label, glr_live_parser_text (live, NULL), stats.total_nodes,
            (unsigned long long) stats.bytes_reused,
            (unsigned long long) stats.bytes_reparsed,
            stats.last_parse_succeeded ? "" : "   (parse failed)");
}

/* Apply one edit, then show what was marked and what the update cost. */
static int
apply_edit (glr_live_parser_t *live, uint32_t line, uint32_t column,
            const char *replacement)
{
    glr_live_edit_t edit;
    char error[128];
    size_t marked;
    size_t start = 0;
    size_t end = 0;

    memset (error, 0, sizeof (error));

    /* The reported range is [line:column, line:column + length), which for a
       replacement covers the bytes being replaced and for an insertion covers
       nothing at all. */
    edit.start_line = line;
    edit.start_column = column;
    edit.end_line = line;
    edit.end_column = column + (uint32_t) strlen (replacement);
    edit.replacement = replacement;
    edit.replacement_length = strlen (replacement);

    if (glr_live_parser_edit (live, &edit, error, sizeof (error)) != 0)
    {
        printf ("  edit at %u:%u rejected: %s\n", line, column, error);
        return -1;
    }

    marked = glr_live_parser_dirty_count (live);
    if (glr_live_parser_last_edit_range (live, &start, &end))
    {
        printf ("  edit %u:%u replaced with \"%s\": marked %zu branch(es), "
                "new bytes [%zu, %zu)\n",
                line, column, replacement, marked, start, end);
    }

    if (glr_live_parser_update (live, error, sizeof (error)) != 0)
    {
        printf ("  update failed: %s\n", error);
        printf ("  the text no longer parses, so the tree was dropped rather "
                "than left describing stale input\n");
        return 0; /* a failed update is a normal outcome, not a crash */
    }
    return 0;
}

int
main (int argc, char **argv)
{
    static const char *script[]
        = { "1:2-", "1:4*", "1:6+" }; /* what the built-in demo does */
    const char *document = argc > 1 ? argv[1] : "n+n*n+n";
    glr_grammar_t *grammar;
    glr_live_parser_t *live;
    char error[128];
    const glr_forest_node_t *seen[4096];
    size_t seen_count = 0;

    grammar = expression_grammar ();
    if (grammar == NULL)
    {
        fprintf (stderr, "could not build the grammar\n");
        return EXIT_FAILURE;
    }

    memset (error, 0, sizeof (error));
    live = glr_live_parser_create (grammar, document, strlen (document), error,
                                   sizeof (error));
    if (live == NULL)
    {
        fprintf (stderr, "could not parse \"%s\": %s\n", document, error);
        glr_grammar_destroy (grammar);
        return EXIT_FAILURE;
    }

    printf ("document: \"%s\"\n", document);
    report_status (live, "initial parse");

    if (argc > 2)
    {
        /* Edits come from the command line as line:column and an optional
           replacement, e.g. `./live "n+n" 1:2 -`. */
        for (int i = 2; i < argc; i += 2)
        {
            unsigned long line = 0;
            unsigned long column = 0;
            const char *replacement = (i + 1 < argc) ? argv[i + 1] : "-";

            if (sscanf (argv[i], "%lu:%lu", &line, &column) != 2
                || line == 0 || column == 0)
            {
                printf ("  ignoring \"%s\": expected line:column\n", argv[i]);
                continue;
            }
            (void) apply_edit (live, (uint32_t) line, (uint32_t) column,
                               replacement);
            report_status (live, "after update");
        }
    }
    else
    {
        for (size_t i = 0; i < sizeof (script) / sizeof (script[0]); i++)
        {
            size_t length = strlen (script[i]);
            char spec[8];
            char replacement[4];
            unsigned long line = 1;
            unsigned long column = 1;

            memcpy (spec, script[i], length - 1);
            spec[length - 1] = '\0';
            replacement[0] = script[i][length - 1];
            replacement[1] = '\0';
            if (sscanf (spec, "%lu:%lu", &line, &column) != 2)
            {
                continue;
            }
            (void) apply_edit (live, (uint32_t) line, (uint32_t) column,
                               replacement);
            report_status (live, "after update");
        }
    }

    if (glr_live_parser_forest (live) != NULL
        && glr_live_parser_forest (live)->root != NULL)
    {
        printf ("\nfinal tree: %zu terminals over bytes [%zu, %zu)\n",
                count_leaves (glr_live_parser_forest (live)->root, seen,
                              &seen_count),
                glr_live_parser_forest (live)->root->position,
                glr_live_parser_forest (live)->root->end_position);
    }
    else
    {
        printf ("\nno tree: the document does not parse\n");
    }

    glr_live_parser_destroy (live);
    glr_grammar_destroy (grammar);
    return EXIT_SUCCESS;
}