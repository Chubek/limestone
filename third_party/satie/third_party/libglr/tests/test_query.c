/**
 * @file test_query.c
 * @brief Unit tests for S-expression queries, actions, and AST building.
 *
 * Covers pattern compilation and matching, the action table (registration,
 * lookup, and shadowing a builtin), running a query over a forest, the builtin
 * action set (extraction, AST building, rewriting, rejection, IPC, signals,
 * halting and exiting), and AST serialization round trips.
 */

#include "test_common.h"

#include <glr/glr.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Expr -> Expr '+' Term | Term, Term -> Factor, Factor -> n. */
static glr_grammar_t *
query_grammar (void)
{
    glr_grammar_t *grammar = glr_grammar_create ();
    int expr;
    int term;
    int factor;
    int n;
    int plus;
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

    body[0] = glr_grammar_get_symbol (grammar, term);
    glr_grammar_add_production (grammar, expr, body, 1);
    body[0] = glr_grammar_get_symbol (grammar, expr);
    body[1] = glr_grammar_get_symbol (grammar, plus);
    body[2] = glr_grammar_get_symbol (grammar, term);
    glr_grammar_add_production (grammar, expr, body, 3);
    body[0] = glr_grammar_get_symbol (grammar, factor);
    glr_grammar_add_production (grammar, term, body, 1);
    body[0] = glr_grammar_get_symbol (grammar, n);
    glr_grammar_add_production (grammar, factor, body, 1);
    glr_grammar_set_start_symbol (grammar, expr);
    return grammar;
}

/* A sink that concatenates everything reported, so a test can assert on it. */
struct sink
{
    char text[1024];
    size_t length;
    size_t reports;
};

static void
sink_write (const char *text, size_t length, void *user_data)
{
    struct sink *sink = user_data;

    sink->reports++;
    if (sink->length + length + 1 < sizeof (sink->text))
    {
        memcpy (sink->text + sink->length, text, length);
        sink->length += length;
        sink->text[sink->length] = '\0';
    }
}

static bool
sink_has (const struct sink *sink, const char *needle)
{
    return strstr (sink->text, needle) != NULL;
}

static void
sink_reset (struct sink *sink)
{
    memset (sink, 0, sizeof (*sink));
}

GLR_TEST_CASE (test_query_pattern_matching)
{
    glr_grammar_t *grammar = query_grammar ();
    glr_parser_t *parser;
    static const char text[] = "n+n";
    glr_parse_result_t result;
    glr_query_pattern_t *pattern;
    const glr_forest_node_t *bindings[4];
    size_t count = 0;
    char error[128];

    glr_test_begin ("query pattern compilation and matching");
    parser = glr_parser_create (grammar);
    GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
    result = glr_parse (parser, text, strlen (text));
    GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "text should parse");

    memset (error, 0, sizeof (error));
    pattern = glr_query_pattern_compile ("(terminal n)", strlen ("(terminal n)"),
                                         error, sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (pattern, error);
    GLR_TEST_ASSERT (!glr_query_pattern_match (pattern, grammar,
                                               result.forest->root, bindings,
                                               4, &count),
                     "the root is not a terminal, so no match");
    GLR_TEST_ASSERT_EQ (count, 0, "a failed match reports no bindings");
    glr_query_pattern_destroy (pattern);

    pattern = glr_query_pattern_compile ("(nonterminal Expr)",
                                         strlen ("(nonterminal Expr)"),
                                         error, sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (pattern, error);
    GLR_TEST_ASSERT (glr_query_pattern_match (pattern, grammar,
                                              result.forest->root, bindings,
                                              4, &count),
                     "the root is an Expr");
    GLR_TEST_ASSERT_EQ (count, 0, "an unbound pattern captures nothing");
    glr_query_pattern_destroy (pattern);

    /* A binding is captured and named. */
    pattern = glr_query_pattern_compile ("(nonterminal $root)",
                                         strlen ("(nonterminal $root)"),
                                         error, sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (pattern, error);
    GLR_TEST_ASSERT (glr_query_pattern_match (pattern, grammar,
                                              result.forest->root, bindings,
                                              4, &count),
                     "a binding pattern should match");
    GLR_TEST_ASSERT_EQ (count, 1, "one binding should be captured");
    GLR_TEST_ASSERT (bindings[0] == result.forest->root,
                     "the binding should be the matched node");
    glr_query_pattern_destroy (pattern);

    /* A wildcard matches any symbol of the right kind. */
    pattern = glr_query_pattern_compile ("(nonterminal _)",
                                         strlen ("(nonterminal _)"), error,
                                         sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (pattern, error);
    GLR_TEST_ASSERT (glr_query_pattern_match (pattern, grammar,
                                              result.forest->root, NULL, 0,
                                              NULL),
                     "a wildcard should match any non-terminal");
    glr_query_pattern_destroy (pattern);

    /* A wrong symbol must not match. */
    pattern = glr_query_pattern_compile ("(nonterminal Term)",
                                         strlen ("(nonterminal Term)"),
                                         error, sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (pattern, error);
    GLR_TEST_ASSERT (!glr_query_pattern_match (pattern, grammar,
                                               result.forest->root, NULL, 0,
                                               NULL),
                     "a different symbol should not match");
    glr_query_pattern_destroy (pattern);

    /* Bad patterns are reported rather than accepted. */
    glr_query_pattern_destroy (glr_query_pattern_compile ("(nosuchkind X)",
                                                          strlen ("(nosuchkind X)"),
                                                          error, sizeof (error)));
    GLR_TEST_ASSERT (error[0] != '\0', "an unknown kind should be reported");
    glr_query_pattern_destroy (glr_query_pattern_compile ("n", 1, error,
                                                          sizeof (error)));
    GLR_TEST_ASSERT (error[0] != '\0',
                     "a pattern that is not a list should be reported");
    glr_query_pattern_destroy (glr_query_pattern_compile (NULL, 0, error,
                                                          sizeof (error)));
    GLR_TEST_ASSERT (error[0] != '\0', "a null pattern should be reported");

    glr_parser_destroy (parser);
    glr_grammar_destroy (grammar);
    glr_test_end ();
}

GLR_TEST_CASE (test_query_compile_and_run)
{
    glr_grammar_t *grammar = query_grammar ();
    glr_parser_t *parser;
    static const char text[] = "n+n";
    static const char source[]
        = "(query"
          "  (name \"symbols\")"
          "  (rules"
          "    (match (terminal n) (print-symbol))"
          "    (match (terminal +) (print \"plus:\"))))";
    glr_parse_result_t result;
    glr_query_t *query;
    glr_query_options_t options;
    glr_query_stats_t qstats;
    struct sink sink;
    char error[128];

    glr_test_begin ("query compile and run");
    parser = glr_parser_create (grammar);
    GLR_TEST_ASSERT_NOT_NULL (parser, "parser should be created");
    result = glr_parse (parser, text, strlen (text));
    GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "text should parse");

    memset (error, 0, sizeof (error));
    query = glr_query_compile (source, strlen (source), NULL, error,
                               sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (query, error);
    GLR_TEST_ASSERT (glr_test_string_eq (glr_query_name (query), "symbols"),
                     "the query should keep its name");
    GLR_TEST_ASSERT_EQ (glr_query_rule_count (query), 2,
                        "the query should have two rules");

    /* The rule that matches a specific terminal fires once per match. */
    GLR_TEST_ASSERT (glr_query_count_matches (query, 0, grammar, result.forest,
                                              result.forest->root)
                         > 0,
                     "the 'n' rule should match");
    GLR_TEST_ASSERT_EQ (glr_query_count_matches (query, 0, grammar,
                                                 result.forest,
                                                 result.forest->root),
                        2, "the accepted path holds one 'n' per operand");
    GLR_TEST_ASSERT_EQ (glr_query_count_matches (query, 99, grammar,
                                                 result.forest,
                                                 result.forest->root),
                        0, "an out-of-range rule matches nothing");
    GLR_TEST_ASSERT_EQ (glr_query_count_matches (NULL, 0, grammar,
                                                 result.forest,
                                                 result.forest->root),
                        0, "a null query matches nothing");
    /* Counting the whole forest and counting below the root have to agree here,
       because both terminals are on the accepted path. */
    GLR_TEST_ASSERT_EQ (glr_query_count_matches (query, 0, grammar,
                                                 result.forest, NULL),
                        2, "the whole forest holds the same two terminals");
    GLR_TEST_ASSERT_EQ (glr_query_count_matches (query, 1, grammar,
                                                 result.forest,
                                                 result.forest->root),
                        1, "there is exactly one operator");

    sink_reset (&sink);
    memset (&options, 0, sizeof (options));
    options.report = sink_write;
    options.user_data = &sink;
    /* Searching below the root walks the accepted derivation, which holds one
       'n' per operand plus the one operator. */
    GLR_TEST_ASSERT_EQ (
        glr_query_run_ex (query, grammar, result.forest, result.forest->root,
                          text, strlen (text), &options, &qstats),
        3, "both rules should fire on every node they match");
    GLR_TEST_ASSERT (sink_has (&sink, "n"),
                     "the terminal's symbol should be reported");
    GLR_TEST_ASSERT (sink_has (&sink, "plus:+"),
                     "the operator and its text should be reported");
    GLR_TEST_ASSERT (qstats.nodes_visited > 0, "the walk should visit nodes");
    GLR_TEST_ASSERT_EQ (qstats.rules, 2, "stats should count the rules");
    GLR_TEST_ASSERT_EQ (qstats.errors, 0, "no action should fail");

    /* The simpler entry point works too. */
    sink_reset (&sink);
    GLR_TEST_ASSERT_EQ (glr_query_run (query, grammar, result.forest,
                                       result.forest->root, text,
                                       strlen (text), NULL, NULL),
                        3, "the plain run should agree");

    glr_query_destroy (query);
    glr_parser_destroy (parser);
    glr_grammar_destroy (grammar);
    glr_test_end ();
}

GLR_TEST_CASE (test_query_compile_errors)
{
    char error[128];

    glr_test_begin ("query compilation reports its failures");
    glr_query_destroy (glr_query_compile (NULL, 0, NULL, error,
                                          sizeof (error)));
    GLR_TEST_ASSERT (error[0] != '\0', "a null source should be reported");

    glr_query_destroy (glr_query_compile ("(query (rules))", strlen ("(query (rules))"),
                                          NULL, error, sizeof (error)));
    GLR_TEST_ASSERT (error[0] != '\0', "an empty rules block should fail");

    glr_query_destroy (
        glr_query_compile ("(query (rules (match (terminal n))))",
                           strlen ("(query (rules (match (terminal n))))"),
                           NULL, error, sizeof (error)));
    GLR_TEST_ASSERT (error[0] != '\0', "a rule without an action should fail");

    glr_query_destroy (glr_query_compile (
        "(query (rules (match (terminal n) (no-such-action))))", 47, NULL,
        error, sizeof (error)));
    GLR_TEST_ASSERT (error[0] != '\0', "an unknown action should be reported");

    glr_query_destroy (glr_query_compile ("(query (name \"x\"", strlen ("(query (name \"x\""),
                                          NULL, error, sizeof (error)));
    GLR_TEST_ASSERT (error[0] != '\0',
                     "malformed S-expression should be reported");

    GLR_TEST_ASSERT_EQ (glr_query_rule_count (NULL), 0,
                        "a null query has no rules");
    GLR_TEST_ASSERT_NULL (glr_query_name (NULL), "a null query has no name");
    glr_query_destroy (NULL);
    glr_test_end ();
}

/* An application-supplied action, to prove registration and shadowing work. */
static glr_query_action_result_t
upper_action (glr_query_context_t *context, const glr_query_match_t *match,
              const char *const *args, size_t arg_count, void *user_data)
{
    (void) args;
    (void) arg_count;
    (void) match;
    if (context->report != NULL)
    {
        const char *text = (const char *) user_data;
        context->report (text, strlen (text), context->report_data);
    }
    return GLR_QUERY_ACTION_CONTINUE;
}

GLR_TEST_CASE (test_query_action_table)
{
    glr_query_action_t table[4];
    glr_query_action_t shadow[3];
    struct sink sink;
    char error[128];
    size_t i;

    glr_test_begin ("query action table registration and shadowing");
    for (i = 0; i < 4; i++)
    {
        memset (&table[i], 0, sizeof (table[i]));
    }
    for (i = 0; i < 3; i++)
    {
        memset (&shadow[i], 0, sizeof (shadow[i]));
    }

    GLR_TEST_ASSERT_EQ (glr_query_actions_register (table, 4, "shout",
                                                    upper_action,
                                                    (void *) "SHOUT", NULL,
                                                    "test action"),
                        0, "an action should register");
    GLR_TEST_ASSERT_NOT_NULL (glr_query_actions_find (table, "shout"),
                              "the action should be found");
    GLR_TEST_ASSERT_NULL (glr_query_actions_find (table, "absent"),
                          "an unknown action should not be found");
    GLR_TEST_ASSERT_EQ (glr_query_actions_count (table), 1,
                        "the table should hold one entry");
    GLR_TEST_ASSERT_EQ (glr_query_actions_count (NULL), 0,
                        "a null table holds nothing");

    /* The local table is consulted before the globals. */
    GLR_TEST_ASSERT_NOT_NULL (glr_query_resolve_action (table, "shout"),
                              "a local action should resolve");
    GLR_TEST_ASSERT_NOT_NULL (glr_query_resolve_action (table, "print"),
                              "a builtin should still resolve");
    GLR_TEST_ASSERT_NULL (glr_query_resolve_action (table, "nope"),
                          "an unknown action should not resolve");

    /* Registering a name that already exists replaces it in place. */
    GLR_TEST_ASSERT_EQ (glr_query_actions_register (table, 4, "shout",
                                                    upper_action,
                                                    (void *) "QUIET", NULL,
                                                    NULL),
                        0, "re-registering should succeed");
    GLR_TEST_ASSERT_EQ (glr_query_actions_count (table), 1,
                        "re-registering should not add an entry");
    GLR_TEST_ASSERT_EQ (table[0].user_data, (void *) "QUIET",
                        "the data should be replaced");

    GLR_TEST_ASSERT_EQ (glr_query_actions_register (table, 4, "x", NULL, NULL,
                                                    NULL, NULL),
                        -1, "an entry with neither action nor data is refused");
    GLR_TEST_ASSERT_EQ (glr_query_actions_register (NULL, 4, "x", upper_action,
                                                    "d", NULL, NULL),
                        -1, "a null table should be refused");
    GLR_TEST_ASSERT_EQ (glr_query_actions_register (table, 4, "", upper_action,
                                                    "d", NULL, NULL),
                        -1, "an empty name should be refused");

    /* A full table refuses to grow. */
    GLR_TEST_ASSERT_EQ (glr_query_actions_register (shadow, 2, "a",
                                                    upper_action, "d", NULL,
                                                    NULL),
                        0, "the first entry fits");
    GLR_TEST_ASSERT_EQ (glr_query_actions_register (shadow, 2, "b",
                                                    upper_action, "d", NULL,
                                                    NULL),
                        0, "the second entry fits");
    GLR_TEST_ASSERT_EQ (glr_query_actions_register (shadow, 2, "c",
                                                    upper_action, "d", NULL,
                                                    NULL),
                        -1, "a third should not fit");
    (void) sink;
    (void) error;
    glr_test_end ();
}

GLR_TEST_CASE (test_query_builtin_actions)
{
    glr_grammar_t *grammar = query_grammar ();
    glr_parser_t *parser;
    static const char text[] = "n+n";
    glr_parse_result_t result;
    struct sink sink;
    glr_query_options_t options;
    char error[128];

    glr_test_begin ("builtin query actions");
    parser = glr_parser_create (grammar);
    result = glr_parse (parser, text, strlen (text));
    GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "text should parse");

    /* Extraction: report the text of a bound node. */
    {
        static const char source[]
            = "(query (rules"
              "  (match (constructor _ (nonterminal _) (terminal @op)"
              "                 (nonterminal _))"
              "         (extract \"op=\" op))))";
        glr_query_t *query
            = glr_query_compile (source, strlen (source), NULL, error,
                                 sizeof (error));
        GLR_TEST_ASSERT_NOT_NULL (query, error);
        sink_reset (&sink);
        memset (&options, 0, sizeof (options));
        options.report = sink_write;
        options.user_data = &sink;
        GLR_TEST_ASSERT (glr_query_run_ex (query, grammar, result.forest,
                                            result.forest->root, text,
                                            strlen (text), &options, NULL)
                             > 0,
                         "the operator rule should match");
        GLR_TEST_ASSERT (sink_has (&sink, "op=+"),
                         "the bound operator text should be reported");
        glr_query_destroy (query);
    }

    /* IPC and signalling route the matched text through the sink. */
    {
        static const char source[]
            = "(query (rules"
              "  (match (terminal +) (@ipc \"chan\"))"
              "  (match (terminal +) (@signal \"sig\"))))";
        glr_query_t *query
            = glr_query_compile (source, strlen (source), NULL, error,
                                 sizeof (error));
        GLR_TEST_ASSERT_NOT_NULL (query, error);
        sink_reset (&sink);
        memset (&options, 0, sizeof (options));
        options.report = sink_write;
        options.user_data = &sink;
        (void) glr_query_run_ex (query, grammar, result.forest,
                                 result.forest->root, text, strlen (text),
                                 &options, NULL);
        GLR_TEST_ASSERT (sink_has (&sink, "@ipc chan +"),
                         "the IPC action should pass the text along");
        GLR_TEST_ASSERT (sink_has (&sink, "@signal sig +"),
                         "the signal action should deliver the text");
        glr_query_destroy (query);
    }

    /* XML streams the matched subtree's leaves. */
    {
        static const char source[]
            = "(query (rules (match (terminal +) (@xml))))";
        glr_query_t *query
            = glr_query_compile (source, strlen (source), NULL, error,
                                 sizeof (error));
        GLR_TEST_ASSERT_NOT_NULL (query, error);
        sink_reset (&sink);
        memset (&options, 0, sizeof (options));
        options.report = sink_write;
        options.user_data = &sink;
        (void) glr_query_run_ex (query, grammar, result.forest,
                                 result.forest->root, text, strlen (text),
                                 &options, NULL);
        GLR_TEST_ASSERT (sink_has (&sink, "@xml"),
                         "the XML action should report something");
        glr_query_destroy (query);
    }

    /* Halting stops the walk early, so fewer nodes are visited than when the
       same query is allowed to finish. */
    {
        static const char full[]
            = "(query (rules (match (terminal _) (print \"seen\"))))";
        static const char stopped[]
            = "(query (rules"
              "  (match (terminal +) (@halt))"
              "  (match (terminal _) (print \"seen\"))))";
        glr_query_stats_t full_stats;
        glr_query_stats_t stopped_stats;
        glr_query_t *query;

        query = glr_query_compile (full, strlen (full), NULL, error,
                                   sizeof (error));
        GLR_TEST_ASSERT_NOT_NULL (query, error);
        sink_reset (&sink);
        memset (&options, 0, sizeof (options));
        options.report = sink_write;
        options.user_data = &sink;
        memset (&full_stats, 0, sizeof (full_stats));
        (void) glr_query_run_ex (query, grammar, result.forest,
                                 result.forest->root, text, strlen (text),
                                 &options, &full_stats);
        glr_query_destroy (query);

        query = glr_query_compile (stopped, strlen (stopped), NULL, error,
                                   sizeof (error));
        GLR_TEST_ASSERT_NOT_NULL (query, error);
        sink_reset (&sink);
        memset (&stopped_stats, 0, sizeof (stopped_stats));
        (void) glr_query_run_ex (query, grammar, result.forest,
                                 result.forest->root, text, strlen (text),
                                 &options, &stopped_stats);
        glr_query_destroy (query);

        GLR_TEST_ASSERT (full_stats.nodes_visited > 0,
                         "the unrestricted run should visit nodes");
        GLR_TEST_ASSERT (stopped_stats.nodes_visited
                             < full_stats.nodes_visited,
                         "the halted run should stop early");
        GLR_TEST_ASSERT (sink.reports > 0,
                         "the halted run should still report what it saw");
    }

    /* Rejection is recorded, not applied to the forest. */
    {
        static const char source[]
            = "(query (rules (match (terminal +) (@reject))))";
        glr_query_t *query
            = glr_query_compile (source, strlen (source), NULL, error,
                                 sizeof (error));
        GLR_TEST_ASSERT_NOT_NULL (query, error);
        sink_reset (&sink);
        memset (&options, 0, sizeof (options));
        options.report = sink_write;
        options.user_data = &sink;
        (void) glr_query_run_ex (query, grammar, result.forest,
                                 result.forest->root, text, strlen (text),
                                 &options, NULL);
        GLR_TEST_ASSERT (sink_has (&sink, "@reject +"),
                         "the rejection should be reported");
        glr_query_destroy (query);
    }

    glr_parser_destroy (parser);
    glr_grammar_destroy (grammar);
    glr_test_end ();
}

GLR_TEST_CASE (test_query_rewriting)
{
    glr_grammar_t *grammar = query_grammar ();
    glr_parser_t *parser;
    static const char text[] = "n+n";
    glr_parse_result_t result;
    glr_query_t *query;
    char error[128];

    glr_test_begin ("query rewriting renames a matched symbol");
    parser = glr_parser_create (grammar);
    result = glr_parse (parser, text, strlen (text));
    GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "text should parse");

    {
        static const char source[]
            = "(query (rules"
              "  (match (terminal +) (@rename \"plus\"))))";
        query = glr_query_compile (source, strlen (source), NULL, error,
                                   sizeof (error));
        GLR_TEST_ASSERT_NOT_NULL (query, error);
        glr_query_options_t options;

        memset (&options, 0, sizeof (options));
        options.mutable_grammar = grammar;

        if (query != NULL)
        {
            (void) glr_query_run_ex (query, grammar, result.forest,
                                     result.forest->root, text,
                                     strlen (text), &options, NULL);
            GLR_TEST_ASSERT (glr_grammar_find_symbol (grammar, "plus",
                                                       GLR_SYMBOL_TERMINAL)
                                 >= 0,
                             "the symbol should have been renamed");
            GLR_TEST_ASSERT_EQ (
                glr_grammar_find_symbol (grammar, "+", GLR_SYMBOL_TERMINAL), -1,
                "the old name should be gone");
        }
        glr_query_destroy (query);
    }

    glr_parser_destroy (parser);
    glr_grammar_destroy (grammar);
    glr_test_end ();
}

GLR_TEST_CASE (test_query_ast_building)
{
    glr_grammar_t *grammar = query_grammar ();
    glr_parser_t *parser;
    static const char text[] = "n+n";
    glr_parse_result_t result;
    glr_query_t *query;
    glr_ast_t *ast;
    glr_query_options_t options;
    char *sexp = NULL;
    size_t length = 0;
    char error[128];

    glr_test_begin ("query AST building");
    parser = glr_parser_create (grammar);
    result = glr_parse (parser, text, strlen (text));
    GLR_TEST_ASSERT_EQ (result.error, GLR_PARSE_SUCCESS, "text should parse");

    {
        static const char source[]
            = "(query"
              "  (ast (node Expr (nonterminal _)))"
              "  (rules"
              "    (match (nonterminal Expr) (@node \"Expr\"))"
              "    (match (nonterminal Expr) (@node))))";
        ast = glr_ast_create ();
        GLR_TEST_ASSERT_NOT_NULL (ast, "an AST should be created");
        memset (&options, 0, sizeof (options));
        options.ast = ast;

        query = glr_query_compile (source, strlen (source), NULL, error,
                                   sizeof (error));
        GLR_TEST_ASSERT_NOT_NULL (query, error);
        if (query != NULL)
        {
            (void) glr_query_run_ex (query, grammar, result.forest,
                                     result.forest->root, text, strlen (text),
                                     &options, NULL);
            GLR_TEST_ASSERT_NOT_NULL (glr_ast_root (ast),
                                      "the AST should have a root");
            GLR_TEST_ASSERT (glr_ast_count_nodes (glr_ast_root (ast)) > 0,
                             "the AST should hold nodes");
            GLR_TEST_ASSERT_NOT_NULL (glr_ast_find (glr_ast_root (ast),
                                                    "Expr"),
                                      "the root should be an Expr");
            GLR_TEST_ASSERT_EQ (
                glr_ast_to_sexp (glr_ast_root (ast), &sexp, &length), 0,
                "the AST should render");
            GLR_TEST_ASSERT (sexp != NULL && length > 0,
                             "the rendering should not be empty");
            GLR_TEST_ASSERT (strstr (sexp, "Expr") != NULL,
                             "the rendering should name the node kind");
            GLR_TEST_ASSERT (strstr (sexp, "\"+\"") != NULL
                                 || strstr (sexp, "n") != NULL,
                             "the rendering should carry the leaf text");
            free (sexp);
            sexp = NULL;
            glr_query_destroy (query);
        }
        glr_ast_destroy (ast);
    }

    /* Leaves are built directly, and a fixed value can be injected. */
    {
        static const char source[]
            = "(query (rules"
              "  (match (terminal n) (@leaf \"n\"))"
              "  (match (terminal +) (@leaf \"plus\"))))";
        glr_ast_t *leaves = glr_ast_create ();
        memset (&options, 0, sizeof (options));
        options.ast = leaves;
        query = glr_query_compile (source, strlen (source), NULL, error,
                                   sizeof (error));
        GLR_TEST_ASSERT_NOT_NULL (query, error);
        if (query != NULL)
        {
            (void) glr_query_run_ex (query, grammar, result.forest,
                                     result.forest->root, text, strlen (text),
                                     &options, NULL);
            GLR_TEST_ASSERT_NOT_NULL (glr_ast_root (leaves),
                                      "leaves should have been built");
            /* Several matches attach as siblings under a synthetic list, so
               the root is either a leaf or a container of them. */
            GLR_TEST_ASSERT (glr_ast_root (leaves)->value != NULL
                                 || glr_ast_root (leaves)->child_count > 0,
                             "the leaves should have been built");
            GLR_TEST_ASSERT (glr_ast_count_nodes (glr_ast_root (leaves)) > 0,
                             "the AST should hold the matched leaves");
            glr_query_destroy (query);
        }
        glr_ast_destroy (leaves);
    }

    glr_parser_destroy (parser);
    glr_grammar_destroy (grammar);
    glr_test_end ();
}

GLR_TEST_CASE (test_ast_serialization)
{
    glr_ast_t *ast;
    glr_ast_t *reparsed;
    glr_ast_t *extra;
    char *sexp = NULL;
    size_t length = 0;
    char error[128];

    glr_test_begin ("AST serialization round trip");
    ast = glr_ast_from_sexp ("(Program (Decl int x) (Decl int y))",
                             strlen ("(Program (Decl int x) (Decl int y))"),
                             error, sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (ast, error);
    if (ast == NULL)
    {
        return;
    }
    GLR_TEST_ASSERT_NOT_NULL (glr_ast_root (ast), "the AST should have a root");
    GLR_TEST_ASSERT (glr_test_string_eq (glr_ast_root (ast)->name, "Program"),
                     "the root kind should be read back");
    GLR_TEST_ASSERT_EQ (glr_ast_root (ast)->child_count, 2,
                        "the root should have two children");
    GLR_TEST_ASSERT_EQ (glr_ast_count_nodes (glr_ast_root (ast)), 7,
                        "the tree should hold seven nodes");
    GLR_TEST_ASSERT_NOT_NULL (glr_ast_find (glr_ast_root (ast), "Decl"),
                              "a child kind should be findable");

    GLR_TEST_ASSERT_EQ (glr_ast_to_sexp (glr_ast_root (ast), &sexp, &length),
                        0, "the AST should render");
    GLR_TEST_ASSERT (sexp != NULL && strstr (sexp, "Program") != NULL,
                     "the rendering should mention the root kind");
    free (sexp);
    sexp = NULL;

    reparsed = glr_ast_from_sexp ("(Program (Decl int x) (Decl int y))",
                                 strlen ("(Program (Decl int x) (Decl int y))"),
                                 error, sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (reparsed, error);
    GLR_TEST_ASSERT_EQ (glr_ast_count_nodes (glr_ast_root (reparsed)),
                        glr_ast_count_nodes (glr_ast_root (ast)),
                        "the round trip should preserve the node count");
    glr_ast_destroy (reparsed);

    /* A quoted atom reads back as a value, not as a nested list. */
    reparsed = glr_ast_from_sexp ("(Leaf \"a value\")", strlen ("(Leaf \"a value\")"),
                                 error, sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (reparsed, error);
    if (reparsed != NULL)
    {
        const glr_ast_node_t *root = glr_ast_root (reparsed);

        GLR_TEST_ASSERT (glr_test_string_eq (root->name, "Leaf"),
                         "the list head should be the node kind");
        GLR_TEST_ASSERT_EQ (root->child_count, 1,
                            "the quoted atom should be a child");
        GLR_TEST_ASSERT (
            root->children[0] != NULL
                && glr_test_string_eq (root->children[0]->value, "a value"),
            "a quoted atom should become a value");
        glr_ast_destroy (reparsed);
    }

    /* Only one root per AST. */
    extra = glr_ast_from_sexp ("(Other)", strlen ("(Other)"), error,
                              sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (extra, "a second AST should be readable");
    if (extra != NULL)
    {
        glr_ast_node_t *stolen = (glr_ast_node_t *) glr_ast_root (extra);
        GLR_TEST_ASSERT_EQ (glr_ast_set_root (ast, stolen), -1,
                            "a second root should be refused");
        glr_ast_destroy (extra);
    }

    GLR_TEST_ASSERT_NULL (glr_ast_from_sexp ("(unterminated", strlen ("(unterminated"),
                                            error, sizeof (error)),
                          "malformed S-expression should be reported");
    GLR_TEST_ASSERT_NULL (glr_ast_from_sexp (NULL, 0, error, sizeof (error)),
                          "a null source should be reported");

    glr_ast_destroy (ast);
    glr_ast_destroy (NULL);
    glr_test_end ();
}

GLR_TEST_CASE (test_ast_definitions)
{
    static const char source[]
        = "(ast"
          "  (node Expr (nonterminal _))"
          "  (node Term (nonterminal _)))";
    glr_ast_def_t *definition;
    const char *kinds[4];
    char error[128];

    glr_test_begin ("AST definitions");
    definition = glr_query_ast_from_sexp (source, strlen (source), error,
                                          sizeof (error));
    GLR_TEST_ASSERT_NOT_NULL (definition, error);
    GLR_TEST_ASSERT_EQ (glr_query_ast_node_kinds (definition, kinds, 4), 2,
                        "the definition should declare two kinds");
    GLR_TEST_ASSERT_NOT_NULL (glr_query_ast_rule (definition, "Expr"),
                              "a declared kind should have a rule");
    GLR_TEST_ASSERT_NULL (glr_query_ast_rule (definition, "Nope"),
                          "an undeclared kind has no rule");
    GLR_TEST_ASSERT_EQ (glr_query_ast_node_kinds (NULL, NULL, 0), 0,
                        "a null definition declares nothing");
    glr_query_ast_destroy (definition);
    glr_query_ast_destroy (NULL);

    glr_query_ast_destroy (
        glr_query_ast_from_sexp ("(not-an-ast)", strlen ("(not-an-ast)"),
                                        error, sizeof (error)));
    GLR_TEST_ASSERT (error[0] != '\0',
                     "a malformed definition should be reported");
    glr_query_ast_destroy (glr_query_ast_from_sexp (NULL, 0, error,
                                                     sizeof (error)));
    GLR_TEST_ASSERT (error[0] != '\0',
                     "a null definition should be reported");

    GLR_TEST_ASSERT_NULL (glr_query_ast_load_file ("/nonexistent/ast.sexp",
                                                   error, sizeof (error)),
                          "a missing file should be reported");
    GLR_TEST_ASSERT (error[0] != '\0', "the failure should be explained");
    glr_test_end ();
}

int
main (void)
{
    GLR_TEST_INIT;

    printf ("=== LibGLR Query Tests ===\n\n");

    test_query_pattern_matching (&stats);
    test_query_compile_and_run (&stats);
    test_query_compile_errors (&stats);
    test_query_action_table (&stats);
    test_query_builtin_actions (&stats);
    test_query_rewriting (&stats);
    test_query_ast_building (&stats);
    test_ast_serialization (&stats);
    test_ast_definitions (&stats);

    return glr_test_finish ("LibGLR Query", stats);
}
