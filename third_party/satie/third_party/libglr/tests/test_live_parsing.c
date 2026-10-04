/**
 * @file test_live_parsing.c
 * @brief Unit tests for editor-facing incremental parsing.
 *
 * Covers the four-step flow the session is designed around: create with an
 * initial parse, report an edit by line/column, observe which branches were
 * marked, then update and check that the tree matches the new text. Also
 * covers the position conversions, the validation contract, and the XML event
 * stream the session can hand out.
 */

#include "test_common.h"

#include <glr/glr.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Expr -> Expr '+' Term | Expr '-' Term | Term, Term -> Factor,
   Factor -> n | '(' Expr ')'. The terminals are literal text so the parser's
   grammar-driven tokenizer matches them. */
static glr_grammar_t *
live_grammar (void)
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
    factor = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Factor");
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
    body[2] = glr_grammar_get_symbol (grammar, term);
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

GLR_TEST_CASE (test_live_create_and_observe)
{
    glr_grammar_t *grammar = live_grammar ();
    glr_live_parser_t *live;
    glr_live_parser_stats_t live_stats;
    char error[128];
    size_t length = 0;
    const char *text = "n+n";

    glr_test_begin ("live session create and observe");
    GLR_TEST_ASSERT_NOT_NULL (grammar, "grammar should be created");
    memset (error, 0, sizeof (error));
    live = glr_live_parser_create (grammar, text, strlen (text), error,
                                   sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (live, error);
    if (live == NULL)
    {
        glr_grammar_destroy (grammar);
        return;
    }

    GLR_TEST_ASSERT (glr_live_parser_text (live, &length) != NULL,
                     "text should be available");
    GLR_TEST_ASSERT_EQ (length, 3, "text length should be reported");
    GLR_TEST_ASSERT (glr_test_string_eq (glr_live_parser_text (live, NULL), "n+n"),
                     "text should match the input");
    GLR_TEST_ASSERT_NOT_NULL (glr_live_parser_forest (live),
                              "forest should be available");
    GLR_TEST_ASSERT_NOT_NULL (glr_live_parser_forest (live)->root,
                              "forest should have a root");
    GLR_TEST_ASSERT_NULL (glr_live_parser_error (live),
                          "a fresh session has no error");
    GLR_TEST_ASSERT (!glr_live_parser_has_pending_edits (live),
                     "a fresh session has nothing pending");
    GLR_TEST_ASSERT_EQ (glr_live_parser_dirty_count (live), 0,
                        "a fresh session has no marked branches");
    GLR_TEST_ASSERT (!glr_live_parser_last_edit_range (live, NULL, NULL),
                     "no edit has been reported yet");

    GLR_TEST_ASSERT_EQ (glr_live_parser_get_stats (live, &live_stats), 0,
                        "stats should be readable");
    GLR_TEST_ASSERT (live_stats.last_parse_succeeded,
                     "the initial parse should succeed");
    GLR_TEST_ASSERT (live_stats.total_nodes > 0, "the forest should hold nodes");

    glr_live_parser_destroy (live);
    glr_grammar_destroy (grammar);
    glr_test_end ();
}

GLR_TEST_CASE (test_live_create_rejects_bad_input)
{
    glr_grammar_t *grammar = live_grammar ();
    glr_grammar_t *no_start = glr_grammar_create ();
    char error[128];

    glr_test_begin ("live session creation validates its input");
    memset (error, 0, sizeof (error));
    GLR_TEST_ASSERT_NULL (glr_live_parser_create (NULL, "n", 1, error,
                                                  sizeof (error)),
                          "a null grammar should be rejected");
    GLR_TEST_ASSERT (error[0] != '\0', "the failure should be reported");

    glr_grammar_add_symbol (no_start, GLR_SYMBOL_NONTERMINAL, "S");
    GLR_TEST_ASSERT_NULL (glr_live_parser_create (no_start, "n", 1, error,
                                                  sizeof (error)),
                          "a grammar without a start symbol should be rejected");

    GLR_TEST_ASSERT_NULL (glr_live_parser_create (grammar, NULL, 4, error,
                                                  sizeof (error)),
                          "a null buffer with a length should be rejected");

    /* "+" alone is not derivable, so there is no tree to update. */
    GLR_TEST_ASSERT_NULL (glr_live_parser_create (grammar, "+", 1, error,
                                                  sizeof (error)),
                          "an unparseable initial text should be rejected");

    glr_grammar_destroy (grammar);
    glr_grammar_destroy (no_start);
    glr_test_end ();
}

GLR_TEST_CASE (test_live_edit_marks_branches)
{
    glr_grammar_t *grammar = live_grammar ();
    glr_live_parser_t *live;
    glr_live_edit_t edit;
    char error[128];
    size_t start = 0;
    size_t end = 0;

    glr_test_begin ("live edit marks the branches it reaches");
    memset (error, 0, sizeof (error));
    live = glr_live_parser_create (grammar, "n+n*n", 5, error, sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (live, error);
    if (live == NULL)
    {
        glr_grammar_destroy (grammar);
        return;
    }

    /* Replace the '+' at byte 1 with a '-': the region is [1, 2). */
    edit.start_line = 1;
    edit.start_column = 2;
    edit.end_line = 1;
    edit.end_column = 3;
    edit.replacement = "-";
    edit.replacement_length = 1;

    GLR_TEST_ASSERT_EQ (glr_live_parser_edit (live, &edit, error,
                                              sizeof (error)),
                        0, "the edit should be accepted");
    GLR_TEST_ASSERT (glr_live_parser_has_pending_edits (live),
                     "an edit leaves work pending");
    GLR_TEST_ASSERT (glr_test_string_eq (glr_live_parser_text (live, NULL),
                                         "n-n*n"),
                     "the text should reflect the edit immediately");
    GLR_TEST_ASSERT (glr_live_parser_dirty_count (live) > 0,
                     "an edit should mark branches");
    GLR_TEST_ASSERT (glr_live_parser_last_edit_range (live, &start, &end),
                     "the edit range should be remembered");
    GLR_TEST_ASSERT_EQ (start, 1, "the edit starts at byte 1");
    GLR_TEST_ASSERT_EQ (end, 2, "the edit covers one byte");

    /* The marked nodes are reported in the order they were found, and every
       one of them has a span. */
    for (size_t i = 0; i < glr_live_parser_dirty_count (live); i++)
    {
        size_t node_start = 0;
        size_t node_end = 0;
        GLR_TEST_ASSERT_NOT_NULL (glr_live_parser_dirty_node (live, i,
                                                                &node_start,
                                                                &node_end),
                                  "marked node should be readable");
        GLR_TEST_ASSERT (node_end >= node_start,
                         "a node's span should not be inverted");
    }
    GLR_TEST_ASSERT_NULL (glr_live_parser_dirty_node (live, 9999, NULL, NULL),
                          "an out-of-range index yields nothing");

    /* An update with nothing to do is a no-op. */
    GLR_TEST_ASSERT_EQ (glr_live_parser_update (live, error, sizeof (error)), 0,
                        "the update should succeed");
    GLR_TEST_ASSERT_NOT_NULL (glr_live_parser_forest (live),
                              "the forest should survive the update");

    glr_live_parser_destroy (live);
    glr_grammar_destroy (grammar);
    glr_test_end ();
}

GLR_TEST_CASE (test_live_update_reuses_the_prefix)
{
    glr_grammar_t *grammar = live_grammar ();
    glr_live_parser_t *live;
    glr_live_parser_stats_t live_stats;
    glr_live_edit_t edit;
    char error[128];
    size_t nodes_before;

    glr_test_begin ("live update reuses the untouched prefix");
    memset (error, 0, sizeof (error));
    live = glr_live_parser_create (grammar, "n+n+n+n", 7, error,
                                   sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (live, error);
    if (live == NULL)
    {
        glr_grammar_destroy (grammar);
        return;
    }
    nodes_before = glr_live_parser_get_stats (live, &live_stats) == 0
                       ? live_stats.total_nodes
                       : 0;

    /* Replace the final '+' at byte 5 with '-': the region is [5, 6). */
    edit.start_line = 1;
    edit.start_column = 6;
    edit.end_line = 1;
    edit.end_column = 7;
    edit.replacement = "-";
    edit.replacement_length = 1;
    GLR_TEST_ASSERT_EQ (glr_live_parser_edit (live, &edit, error,
                                              sizeof (error)),
                        0, "the edit should be accepted");
    GLR_TEST_ASSERT_EQ (glr_live_parser_update (live, error, sizeof (error)), 0,
                        "the update should succeed");
    GLR_TEST_ASSERT (glr_test_string_eq (glr_live_parser_text (live, NULL),
                                         "n+n+n-n"),
                     "the text should hold the new operator");

    GLR_TEST_ASSERT_EQ (glr_live_parser_get_stats (live, &live_stats), 0,
                        "stats should be readable");
    GLR_TEST_ASSERT (live_stats.last_parse_succeeded,
                     "the updated text should still parse");
    GLR_TEST_ASSERT (live_stats.bytes_reused > 0,
                     "the prefix before the edit should be reused");
    GLR_TEST_ASSERT (live_stats.bytes_reparsed < 7,
                     "an edit near the end should re-parse less than the "
                     "whole document");
    GLR_TEST_ASSERT_EQ (live_stats.bytes_reused + live_stats.bytes_reparsed, 7,
                        "reused and re-parsed bytes should cover the text");
    GLR_TEST_ASSERT (live_stats.total_nodes > 0, "the forest should still hold nodes");
    GLR_TEST_ASSERT (live_stats.total_nodes != 0 && nodes_before > 0,
                     "both trees should be populated");

    /* The updated forest still describes the whole new text. */
    GLR_TEST_ASSERT_NOT_NULL (glr_live_parser_forest (live)->root,
                              "the root should be rebuilt");
    GLR_TEST_ASSERT_EQ (glr_live_parser_forest (live)->root->end_position, 7,
                        "the root should span the new text");

    glr_live_parser_destroy (live);
    glr_grammar_destroy (grammar);
    glr_test_end ();
}

GLR_TEST_CASE (test_live_edit_validation)
{
    glr_grammar_t *grammar = live_grammar ();
    glr_live_parser_t *live;
    glr_live_edit_t edit;
    char error[128];

    glr_test_begin ("live edit validation");
    memset (error, 0, sizeof (error));
    live = glr_live_parser_create (grammar, "n+n", 3, error, sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (live, error);
    if (live == NULL)
    {
        glr_grammar_destroy (grammar);
        return;
    }

    GLR_TEST_ASSERT_EQ (glr_live_parser_edit (live, NULL, error,
                                              sizeof (error)),
                        -1, "a null edit should be rejected");
    GLR_TEST_ASSERT_EQ (glr_live_parser_edit (NULL, &edit, error,
                                              sizeof (error)),
                        -1, "a null session should be rejected");

    /* Line 0 does not exist. */
    edit.start_line = 0;
    edit.start_column = 1;
    edit.end_line = 1;
    edit.end_column = 2;
    edit.replacement = "n";
    edit.replacement_length = 1;
    GLR_TEST_ASSERT_EQ (glr_live_parser_edit (live, &edit, error,
                                              sizeof (error)),
                        -1, "line 0 should be rejected");

    /* Column past the end of the line. */
    edit.start_line = 1;
    edit.start_column = 99;
    GLR_TEST_ASSERT_EQ (glr_live_parser_edit (live, &edit, error,
                                              sizeof (error)),
                        -1, "a column past the line should be rejected");

    /* An edit that ends before it starts. */
    edit.start_line = 1;
    edit.start_column = 3;
    edit.end_line = 1;
    edit.end_column = 1;
    GLR_TEST_ASSERT_EQ (glr_live_parser_edit (live, &edit, error,
                                              sizeof (error)),
                        -1, "an inverted range should be rejected");

    /* Replacing nothing with nothing changes nothing, so it is refused rather
       than reported as an edit. An insertion is two equal positions with
       something to insert. */
    edit.start_line = 1;
    edit.start_column = 2;
    edit.end_line = 1;
    edit.end_column = 2;
    edit.replacement = NULL;
    edit.replacement_length = 0;
    GLR_TEST_ASSERT_EQ (glr_live_parser_edit (live, &edit, error,
                                              sizeof (error)),
                        -1, "an empty edit should be rejected");
    edit.replacement = "*";
    edit.replacement_length = 1;
    GLR_TEST_ASSERT_EQ (glr_live_parser_edit (live, &edit, error,
                                              sizeof (error)),
                        0, "an insertion should be accepted");
    GLR_TEST_ASSERT (glr_test_string_eq (glr_live_parser_text (live, NULL),
                                         "n*+n"),
                     "the inserted text should be spliced in");

    glr_live_parser_destroy (live);
    glr_grammar_destroy (grammar);
    glr_test_end ();
}

GLR_TEST_CASE (test_live_update_preserves_ambiguous_readings)
{
    glr_grammar_t *grammar = glr_grammar_create ();
    int expr = glr_grammar_add_symbol (grammar, GLR_SYMBOL_NONTERMINAL, "Expr");
    int n = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "n");
    int plus = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "+");
    int minus = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "-");
    glr_symbol_t *body[] = { grammar->symbols[expr], grammar->symbols[plus],
                             grammar->symbols[expr] };
    glr_live_edit_t edit = { 1, 6, 1, 7, "-", 1 };
    glr_live_parser_stats_t live_stats;
    char error[128] = { 0 };

    glr_test_begin ("live edits retain every packed reading of ambiguous expressions");
    glr_grammar_add_production (grammar, expr, body, 3);
    body[1] = grammar->symbols[minus];
    glr_grammar_add_production (grammar, expr, body, 3);
    body[0] = grammar->symbols[n];
    glr_grammar_add_production (grammar, expr, body, 1);
    glr_grammar_set_start_symbol (grammar, expr);

    glr_live_parser_t *live = glr_live_parser_create (grammar, "n+n+n+n", 7,
                                                     error, sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (live, error);
    if (live == NULL)
    {
        glr_grammar_destroy (grammar);
        return;
    }
    GLR_TEST_ASSERT (glr_forest_is_ambiguous (glr_live_parser_forest (live)->root),
                     "the initial expression has multiple associations");
    GLR_TEST_ASSERT_EQ (glr_live_parser_edit (live, &edit, error, sizeof (error)),
                        0, "the operator edit should be accepted");
    GLR_TEST_ASSERT_EQ (glr_live_parser_update (live, error, sizeof (error)),
                        0, "the updated expression should parse");

    glr_parser_t *fresh = glr_parser_create (grammar);
    glr_parse_result_t result = glr_parse (fresh, "n+n+n-n", 7);
    const glr_forest_t *updated = glr_live_parser_forest (live);
    GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "a fresh parse should accept the edit");
    GLR_TEST_ASSERT_NOT_NULL (updated, "the updated forest should be available");
    if (updated != NULL && result.forest != NULL)
    {
        GLR_TEST_ASSERT_EQ (updated->root->child_count, 3,
                            "all three outer production splits should survive");
        GLR_TEST_ASSERT_EQ (updated->root->child_count, result.forest->root->child_count,
                            "live and fresh parses should preserve the same alternatives");
        GLR_TEST_ASSERT_EQ (updated->root->end_position, 7,
                            "the rebuilt root covers the complete edited input");
    }
    GLR_TEST_ASSERT_EQ (glr_live_parser_get_stats (live, &live_stats), 0,
                        "update statistics should be available");
    GLR_TEST_ASSERT_EQ (live_stats.bytes_reused, 0,
                        "a single-stack snapshot must not discard ambiguous readings");
    GLR_TEST_ASSERT_EQ (live_stats.bytes_reparsed, 7,
                        "all edited input is reconsidered for an ambiguous forest");
    glr_parser_destroy (fresh);
    glr_live_parser_destroy (live);
    glr_grammar_destroy (grammar);
    glr_test_end ();
}

GLR_TEST_CASE (test_live_broken_edit_is_reported)
{
    glr_grammar_t *grammar = live_grammar ();
    glr_live_parser_t *live;
    glr_live_parser_stats_t live_stats;
    glr_live_edit_t edit;
    char error[128];

    glr_test_begin ("an edit that breaks the text is reported, not hidden");
    memset (error, 0, sizeof (error));
    live = glr_live_parser_create (grammar, "n+n", 3, error, sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (live, error);
    if (live == NULL)
    {
        glr_grammar_destroy (grammar);
        return;
    }

    /* Turn "n+n" into "n++", which no longer derives: the region [1, 2) is
       replaced by two operators. */
    edit.start_line = 1;
    edit.start_column = 2;
    edit.end_line = 1;
    edit.end_column = 3;
    edit.replacement = "++";
    edit.replacement_length = 2;
    GLR_TEST_ASSERT_EQ (glr_live_parser_edit (live, &edit, error,
                                              sizeof (error)),
                        0, "the edit itself should be accepted");

    GLR_TEST_ASSERT_EQ (glr_live_parser_update (live, error, sizeof (error)),
                        -1, "the update should report the failure");
    GLR_TEST_ASSERT (error[0] != '\0', "the failure should be explained");
    GLR_TEST_ASSERT_NOT_NULL (glr_live_parser_error (live),
                              "the session should remember the error");
    GLR_TEST_ASSERT_EQ (glr_live_parser_get_stats (live, &live_stats), 0,
                        "stats should still be readable");
    GLR_TEST_ASSERT (!live_stats.last_parse_succeeded,
                     "the last update should be recorded as failed");
    GLR_TEST_ASSERT_NULL (glr_live_parser_forest (live),
                          "a stale tree should be dropped rather than kept");

    glr_live_parser_destroy (live);
    glr_grammar_destroy (grammar);
    glr_test_end ();
}

GLR_TEST_CASE (test_live_set_text)
{
    glr_grammar_t *grammar = live_grammar ();
    glr_live_parser_t *live;
    char error[128];

    glr_test_begin ("live set_text replaces the whole document");
    memset (error, 0, sizeof (error));
    live = glr_live_parser_create (grammar, "n+n", 3, error, sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (live, error);
    if (live == NULL)
    {
        glr_grammar_destroy (grammar);
        return;
    }

    GLR_TEST_ASSERT_EQ (glr_live_parser_set_text (live, "n*n", 3, error,
                                                   sizeof (error)),
                        0, "set_text should succeed");
    GLR_TEST_ASSERT (glr_test_string_eq (glr_live_parser_text (live, NULL),
                                         "n*n"),
                     "the text should be replaced");
    GLR_TEST_ASSERT_EQ (glr_live_parser_forest (live)->root->end_position, 3,
                        "the new text should be parsed");

    GLR_TEST_ASSERT_EQ (glr_live_parser_set_text (live, "n++", 3, error,
                                                   sizeof (error)),
                        -1, "unparseable text should be reported");
    GLR_TEST_ASSERT_NULL (glr_live_parser_forest (live),
                          "a failed set_text should not leave a stale tree");

    GLR_TEST_ASSERT_EQ (glr_live_parser_set_text (NULL, "n", 1, error,
                                                   sizeof (error)),
                        -1, "a null session should be rejected");

    glr_live_parser_destroy (live);
    glr_grammar_destroy (grammar);
    glr_test_end ();
}

GLR_TEST_CASE (test_live_position_conversion)
{
    static const char text[] = "ab\ncd\n\nef";
    size_t offset = 0;
    uint32_t line = 0;
    uint32_t column = 0;

    glr_test_begin ("live position conversion");
    GLR_TEST_ASSERT_EQ (glr_live_offset_of (text, strlen (text), 1, 1,
                                            &offset),
                        0, "line 1 column 1 is byte 0");
    GLR_TEST_ASSERT_EQ (offset, 0, "the first byte is offset 0");
    GLR_TEST_ASSERT_EQ (glr_live_offset_of (text, strlen (text), 2, 1,
                                            &offset),
                        0, "line 2 column 1 should be found");
    GLR_TEST_ASSERT_EQ (offset, 3, "line 2 starts after the newline");
    GLR_TEST_ASSERT_EQ (glr_live_offset_of (text, strlen (text), 3, 1,
                                            &offset),
                        0, "the empty third line still has a start");
    GLR_TEST_ASSERT_EQ (offset, 6, "the empty line starts at the newline");
    GLR_TEST_ASSERT_EQ (glr_live_offset_of (text, strlen (text), 4, 1,
                                            &offset),
                        0, "the fourth line starts after it");
    GLR_TEST_ASSERT_EQ (offset, 7, "the fourth line starts at byte 7");
    GLR_TEST_ASSERT_EQ (glr_live_offset_of (text, strlen (text), 4, 3,
                                            &offset),
                        0, "the last column should be reachable");
    GLR_TEST_ASSERT_EQ (offset, 9, "the last byte should be reachable");

    GLR_TEST_ASSERT_EQ (glr_live_offset_of (text, strlen (text), 0, 1,
                                            &offset),
                        -1, "line 0 should be rejected");
    GLR_TEST_ASSERT_EQ (glr_live_offset_of (text, strlen (text), 1, 0,
                                            &offset),
                        -1, "column 0 should be rejected");
    GLR_TEST_ASSERT_EQ (glr_live_offset_of (text, strlen (text), 99, 1,
                                            &offset),
                        -1, "a line past the end should be rejected");
    GLR_TEST_ASSERT_EQ (glr_live_offset_of (text, strlen (text), 1, 99,
                                            &offset),
                        -1, "a column past the end should be rejected");
    GLR_TEST_ASSERT_EQ (glr_live_offset_of (text, strlen (text), 1, 1, NULL),
                        -1, "a null output should be rejected");

    GLR_TEST_ASSERT_EQ (glr_live_line_column_of (text, strlen (text), 4,
                                                 &line, &column),
                        0, "an offset should convert back");
    GLR_TEST_ASSERT_EQ (line, 2, "byte 4 is on line 2");
    GLR_TEST_ASSERT_EQ (column, 2, "byte 4 is the second column of line 2");
    GLR_TEST_ASSERT_EQ (glr_live_line_column_of (text, strlen (text), 6, &line,
                                                 &column),
                        0, "an offset on an empty line should convert");
    GLR_TEST_ASSERT_EQ (line, 3, "byte 6 starts the empty line 3");
    GLR_TEST_ASSERT_EQ (column, 1, "an empty line has one column");
    GLR_TEST_ASSERT_EQ (glr_live_line_column_of (text, strlen (text), 7, &line,
                                                 &column),
                        0, "byte 7 should convert");
    GLR_TEST_ASSERT_EQ (line, 4, "byte 7 starts line 4");
    GLR_TEST_ASSERT_EQ (column, 1, "byte 7 is the first column of line 4");
    GLR_TEST_ASSERT_EQ (glr_live_line_column_of (text, strlen (text),
                                                 strlen (text) + 1, &line,
                                                 &column),
                        -1, "an offset past the end should be rejected");
    GLR_TEST_ASSERT_EQ (glr_live_line_column_of (text, strlen (text), 0, NULL,
                                                 &column),
                        -1, "a null output should be rejected");
    GLR_TEST_ASSERT_EQ (glr_live_line_column_of (NULL, 0, 0, &line, &column), -1,
                        "a null buffer should be rejected");
    glr_test_end ();
}

typedef struct
{
    size_t starts;
    size_t leaves;
    size_t ends;
    size_t documents;
} xml_counter_t;

static void
count_event (glr_xml_event_t event, const glr_xml_event_info_t *info,
             void *user_data)
{
    xml_counter_t *counter = user_data;

    (void) info;
    switch (event)
    {
    case GLR_XML_EVENT_DOCUMENT_START:
    case GLR_XML_EVENT_DOCUMENT_END:
        counter->documents++;
        break;
    case GLR_XML_EVENT_NODE_START:
        counter->starts++;
        break;
    case GLR_XML_EVENT_LEAF:
        counter->leaves++;
        break;
    case GLR_XML_EVENT_NODE_END:
        counter->ends++;
        break;
    default:
        break;
    }
}

GLR_TEST_CASE (test_xml_event_stream)
{
    glr_grammar_t *grammar = live_grammar ();
    glr_parser_t *parser;
    static const char text[] = "n+n";
    glr_parse_result_t result;
    xml_counter_t counter;
    char *xml = NULL;
    size_t length = 0;

    glr_test_begin ("parse forest as an XML event stream");
    parser = glr_parser_create (grammar);
    GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
    result = glr_parse (parser, text, strlen (text));
    GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "text should parse");

    memset (&counter, 0, sizeof (counter));
    GLR_TEST_ASSERT (glr_forest_write_xml_events (result.forest, grammar, text,
                                                  strlen (text), count_event,
                                                  &counter)
                         > 0,
                     "the event stream should report nodes");
    GLR_TEST_ASSERT_EQ (counter.documents, 2,
                        "the stream opens and closes once");
    GLR_TEST_ASSERT (counter.starts > 0, "there should be opening tags");
    GLR_TEST_ASSERT_EQ (counter.starts, counter.ends,
                        "every opening tag should be closed");
    GLR_TEST_ASSERT (counter.leaves > 0, "there should be leaves");

    GLR_TEST_ASSERT_EQ (glr_forest_to_xml (result.forest, grammar, text,
                                           strlen (text), &xml, &length),
                        0, "the document should render");
    GLR_TEST_ASSERT_NOT_NULL (xml, "the document should be allocated");
    GLR_TEST_ASSERT (length > 0, "the document should not be empty");
    GLR_TEST_ASSERT (strstr (xml, "<?xml") != NULL,
                     "the document should start with a declaration");
    GLR_TEST_ASSERT (strstr (xml, "<parse-forest") != NULL,
                     "the document should have a root element");
    GLR_TEST_ASSERT (strstr (xml, "<leaf") != NULL,
                     "terminals should be leaves");
    GLR_TEST_ASSERT (strstr (xml, "symbol=\"Expr\"") != NULL,
                     "symbols should be named");
    GLR_TEST_ASSERT (strstr (xml, ">n<") != NULL,
                     "leaf text should be carried");
    free (xml);

    /* The subtree variant walks from one node instead of the whole forest. */
    memset (&counter, 0, sizeof (counter));
    GLR_TEST_ASSERT (glr_forest_node_write_xml_events (result.forest->root,
                                                       grammar, text,
                                                       strlen (text),
                                                       count_event, &counter)
                         > 0,
                     "a subtree should stream too");
    GLR_TEST_ASSERT_EQ (counter.documents, 2, "it opens and closes once");

    GLR_TEST_ASSERT_EQ (glr_forest_write_xml_events (NULL, grammar, text, 3,
                                                     count_event, &counter),
                        0, "a null forest still opens and closes");
    GLR_TEST_ASSERT_EQ (glr_forest_write_xml_events (result.forest, grammar,
                                                     text, 3, NULL, NULL),
                        0, "a null sink is rejected quietly");
    GLR_TEST_ASSERT_EQ (glr_forest_to_xml (result.forest, grammar, text, 3,
                                           NULL, NULL),
                        -1, "a null output buffer is rejected");

    glr_parser_destroy (parser);
    glr_grammar_destroy (grammar);
    glr_test_end ();
}

int
main (void)
{
    GLR_TEST_INIT;

    printf ("=== LibGLR Live Parsing Tests ===\n\n");

    test_live_create_and_observe (&stats);
    test_live_create_rejects_bad_input (&stats);
    test_live_edit_marks_branches (&stats);
    test_live_update_reuses_the_prefix (&stats);
    test_live_update_preserves_ambiguous_readings (&stats);
    test_live_edit_validation (&stats);
    test_live_broken_edit_is_reported (&stats);
    test_live_set_text (&stats);
    test_live_position_conversion (&stats);
    test_xml_event_stream (&stats);

    return glr_test_finish ("LibGLR Live Parsing", stats);
}
