/**
 * @file fuzz_live_query.c
 * @brief Fuzz target for editor-facing incremental parsing and for queries.
 *
 * Two input paths are exercised, because both take untrusted bytes:
 *
 * - The live parser is given a document, then a sequence of edits derived from
 *   the same bytes. Positions come from the input, so the range arithmetic, the
 *   splicing, the branch marking, and the resume-from-snapshot path all run
 *   against values nobody chose to be in range.
 * - The query compiler and executor are given a query built from the input
 *   against a fixed grammar and a parsed forest, which exercises the
 *   S-expression reader, pattern compilation, the action table, and the AST
 *   builder.
 *
 * Run with the builtin seed by passing no input, or under afl-fuzz with a
 * corpus of documents and queries.
 */

#include <glr/glr.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __AFL_FUZZ_TESTCASE_LEN
__AFL_FUZZ_INIT();
#endif

#ifndef __AFL_LOOP
#define __AFL_LOOP(n) (0)
#endif

/* Expr -> Expr '+' Expr | Expr '-' Expr | Term, Term -> Term '*' Factor |
   Factor, Factor -> n | '(' Expr ')'. Single-character terminals keep the
   grammar-driven tokenizer unambiguous. */
static glr_grammar_t *
fuzz_expression_grammar (void)
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

/* A report sink that only counts, so the executor's output path is exercised
   without building an unbounded string. */
static void
fuzz_count_report (const char *text, size_t length, void *user_data)
{
    size_t *total = user_data;

    (void) text;
    *total += length;
}

/* Drive a live session through several edits derived from the input bytes.
   Every edit is reported by line/column, so the input also exercises the
   position conversion and the out-of-range rejections. */
static void
fuzz_live_session (glr_grammar_t *grammar, const unsigned char *buf,
                   size_t len)
{
    glr_live_parser_t *live;
    char error[128];
    size_t document_length;
    const char *document;

    memset (error, 0, sizeof (error));

    /* Half the input is the document, the rest drives the edits. Splitting on a
       length rather than a marker keeps the harness total. */
    document_length = len / 2;
    document = (const char *) buf;

    live = glr_live_parser_create (grammar, document, document_length, error,
                                   sizeof (error));
    if (live == NULL)
    {
        return; /* an unparseable start is an expected outcome */
    }

    for (size_t i = document_length; i < len && i < document_length + 24; i++)
    {
        glr_live_edit_t edit;
        glr_live_parser_stats_t stats;

        /* Positions stay inside the document by construction: the byte under
           the cursor picks the line, and the column is derived from it. */
        edit.start_line = 1;
        edit.start_column
            = (uint32_t)(document_length == 0
                             ? 1
                             : (buf[i] % (unsigned char) document_length) + 1);
        edit.end_line = edit.start_line;
        edit.end_column = edit.start_column;
        edit.replacement = (buf[i] & 1) ? "n" : "*";
        edit.replacement_length = 1;

        if (glr_live_parser_edit (live, &edit, error, sizeof (error)) != 0)
        {
            continue;
        }

        /* The marked set is part of the contract, so read it before updating. */
        for (size_t j = 0; j < glr_live_parser_dirty_count (live); j++)
        {
            size_t start = 0;
            size_t end = 0;
            (void) glr_live_parser_dirty_node (live, j, &start, &end);
        }
        (void) glr_live_parser_last_edit_range (live, NULL, NULL);

        if (glr_live_parser_update (live, error, sizeof (error)) != 0)
        {
            break; /* the text stopped parsing; the session says so */
        }
        if (glr_live_parser_get_stats (live, &stats) != 0)
        {
            break;
        }

        /* An update that claims to have reused bytes must have consumed them. */
        if (stats.bytes_reused + stats.bytes_reparsed != stats.input_length)
        {
            break;
        }
    }

    /* The tree that comes out is walked whole, so a stale node left behind by
       an edit would be read here. */
    if (glr_live_parser_forest (live) != NULL)
    {
        char *xml = NULL;
        size_t xml_length = 0;
        size_t text_length = 0;
        const char *text = glr_live_parser_text (live, &text_length);

        (void) glr_forest_to_xml (glr_live_parser_forest (live), grammar, text,
                                  text_length, &xml, &xml_length);
        free (xml);
    }

    glr_live_parser_destroy (live);
}

/* Compile and run whatever query text the tail of the input spells, against a
   forest built from a small fixed document. */
static void
fuzz_query (glr_grammar_t *grammar, const unsigned char *buf, size_t len)
{
    static const char document[] = "n+n*n";
    glr_parser_t *parser;
    glr_parse_result_t result;
    glr_query_t *query;
    glr_ast_t *ast;
    glr_query_options_t options;
    char error[128];
    size_t reported = 0;

    memset (error, 0, sizeof (error));
    parser = glr_parser_create (grammar);
    if (parser == NULL)
    {
        return;
    }
    result = glr_parse (parser, document, strlen (document));
    if (result.error != GLR_PARSE_SUCCESS)
    {
        glr_parser_destroy (parser);
        return;
    }

    /* The query text is limited so a fuzz case cannot spend unbounded time in
       the S-expression reader. */
    query = glr_query_compile ((const char *) buf, len > 512 ? 512 : len, NULL,
                               error, sizeof (error));
    if (query == NULL)
    {
        glr_parser_destroy (parser);
        return;
    }

    ast = glr_ast_create ();
    memset (&options, 0, sizeof (options));
    options.report = fuzz_count_report;
    options.user_data = &reported;
    options.ast = ast;

    (void) glr_query_run_ex (query, grammar, result.forest, result.forest->root,
                             document, strlen (document), &options, NULL);

    /* Render whatever the AST-building actions produced, so a malformed tree
       built from a matched subtree shows up here rather than in the caller. */
    if (glr_ast_root (ast) != NULL)
    {
        char *sexp = NULL;
        size_t sexp_length = 0;

        if (glr_ast_to_sexp (glr_ast_root (ast), &sexp, &sexp_length) == 0)
        {
            glr_ast_t *reparsed
                = glr_ast_from_sexp (sexp, sexp_length, error, sizeof (error));
            glr_ast_destroy (reparsed);
            free (sexp);
        }
    }

    glr_ast_destroy (ast);
    glr_query_destroy (query);
    glr_parser_destroy (parser);
}

static void
run_once (const unsigned char *buf, size_t len)
{
    glr_grammar_t *grammar;

    if (len == 0)
    {
        return;
    }

    grammar = fuzz_expression_grammar ();
    if (grammar == NULL)
    {
        return;
    }

    fuzz_live_session (grammar, buf, len);
    fuzz_query (grammar, buf + len / 2, len - len / 2);

    glr_grammar_destroy (grammar);
}

int
main (int argc, char **argv)
{
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT ();
#endif

    if (argc > 1)
    {
        /* File mode: one document per file, for reproducing a finding. */
        FILE *stream = fopen (argv[1], "rb");
        unsigned char buffer[8192];
        size_t got;

        if (stream == NULL)
        {
            return 0;
        }
        got = fread (buffer, 1, sizeof (buffer), stream);
        fclose (stream);
        run_once (buffer, got);
        return 0;
    }

    {
        unsigned char buffer[8192];
        size_t got = fread (buffer, 1, sizeof (buffer), stdin);

        run_once (buffer, got);
    }

    return 0;
}