/**
 * @file query.c
 * @brief S-expression queries over a parse forest, with AST building.
 *
 * The example parses an expression, compiles two queries against it, and shows
 * the two things queries are usually used for:
 *
 * 1. extraction, where a pattern captures a node and an action reports its
 *    text through the run's reporting sink;
 * 2. AST building, where `@node()` mirrors the accepted derivation into a tree
 *    that is then written back out as S-expressions.
 *
 * It also registers an application action, to show that a query can call back
 * into the program that wrote it.
 */

#include <glr/glr.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Expr -> Expr '+' Term | Term, Term -> Term '*' Factor | Factor,
   Factor -> n. Left recursion on purpose: the parser has to reduce before it
   shifts, which is what puts constructor nodes in the forest. */
static glr_grammar_t *
expression_grammar (void)
{
    glr_grammar_t *grammar = glr_grammar_create ();
    int expr;
    int term;
    int factor;
    int n;
    int plus;
    int star;
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
    star = glr_grammar_add_symbol (grammar, GLR_SYMBOL_TERMINAL, "*");

    body[0] = glr_grammar_get_symbol (grammar, term);
    glr_grammar_add_production (grammar, expr, body, 1);
    body[0] = glr_grammar_get_symbol (grammar, expr);
    body[1] = glr_grammar_get_symbol (grammar, plus);
    body[2] = glr_grammar_get_symbol (grammar, term);
    glr_grammar_add_production (grammar, expr, body, 3);
    body[0] = glr_grammar_get_symbol (grammar, term);
    body[1] = glr_grammar_get_symbol (grammar, star);
    body[2] = glr_grammar_get_symbol (grammar, factor);
    glr_grammar_add_production (grammar, term, body, 3);
    body[0] = glr_grammar_get_symbol (grammar, factor);
    glr_grammar_add_production (grammar, term, body, 1);
    body[0] = glr_grammar_get_symbol (grammar, n);
    glr_grammar_add_production (grammar, factor, body, 1);

    glr_grammar_set_start_symbol (grammar, expr);
    return grammar;
}

/* The reporting sink every builtin extraction action writes through. */
static void
print_report (const char *text, size_t length, void *user_data)
{
    size_t *reported = user_data;

    (void) reported;
    fwrite (text, 1, length, stdout);
}

/* An application action: it receives its own arguments from the query, so the
   same registered name can do different things in different rules. */
static glr_query_action_result_t
count_action (glr_query_context_t *context, const glr_query_match_t *match,
              const char *const *args, size_t arg_count, void *user_data)
{
    size_t *counter = user_data;
    const char *label = arg_count > 0 && args[0] != NULL ? args[0] : "match";

    (void) context;
    (void) match;
    (*counter)++;
    printf ("  counted a %s at bytes [%zu, %zu)\n", label, match->start,
            match->end);
    return GLR_QUERY_ACTION_CONTINUE;
}

int
main (int argc, char **argv)
{
    const char *document = argc > 1 ? argv[1] : "n+n*n";
    glr_grammar_t *grammar;
    glr_parser_t *parser;
    glr_parse_result_t result;
    char error[160];

    memset (error, 0, sizeof (error));
    grammar = expression_grammar ();
    if (grammar == NULL)
    {
        fprintf (stderr, "could not build the grammar\n");
        return EXIT_FAILURE;
    }

    parser = glr_parser_create (grammar);
    if (parser == NULL)
    {
        fprintf (stderr, "could not create the parser\n");
        glr_grammar_destroy (grammar);
        return EXIT_FAILURE;
    }

    result = glr_parse (parser, document, strlen (document));
    if (result.error != GLR_PARSE_SUCCESS)
    {
        fprintf (stderr, "\"%s\" does not parse (error %d)\n", document,
                 result.error);
        glr_parser_destroy (parser);
        glr_grammar_destroy (grammar);
        return EXIT_FAILURE;
    }
    printf ("parsed \"%s\" into %zu packed nodes\n\n", document,
            glr_forest_total_nodes (result.forest));

    /* --- 1. extraction, plus an application action --- */
    {
        static const char source[]
            = "(query"
              "  (name \"operands\")"
              "  (rules"
              "    (match (constructor _ (terminal @op) (nonterminal _))"
              "           (extract \"operator \" \"op\")"
              "           (count \"operator\"))"
              "    (match (terminal _) (print \"leaf: \"))))";
        glr_query_t *query;
        glr_query_options_t options;
        glr_query_stats_t stats;
        size_t counted = 0;
        glr_query_action_t actions[2];

        /* Registering an application action: the table is passed to
           glr_query_compile(), and a local name shadows a builtin of the same
           name if there is one. */
        memset (actions, 0, sizeof (actions));
        if (glr_query_actions_register (actions, 2, "count", count_action,
                                        &counted, NULL,
                                        "count the matches of a rule")
            != 0)
        {
            fprintf (stderr, "could not register the action\n");
            glr_parser_destroy (parser);
            glr_grammar_destroy (grammar);
            return EXIT_FAILURE;
        }

        query = glr_query_compile (source, strlen (source), actions, error,
                                   sizeof (error));
        if (query == NULL)
        {
            fprintf (stderr, "could not compile the query: %s\n", error);
            glr_parser_destroy (parser);
            glr_grammar_destroy (grammar);
            return EXIT_FAILURE;
        }

        printf ("query \"%s\": %zu rule(s)\n", glr_query_name (query),
                glr_query_rule_count (query));
        memset (&options, 0, sizeof (options));
        options.report = print_report;
        (void) glr_query_run_ex (query, grammar, result.forest,
                                 result.forest->root, document, strlen (document),
                                 &options, &stats);
        printf ("  %zu match(es) over %zu node(s); the application action ran "
                "%zu time(s)\n\n",
                stats.matches, stats.nodes_visited, counted);

        glr_query_destroy (query);
    }

    /* --- 2. AST building --- */
    {
        /* The AST is declared before it is built. The pattern attached to a
           node kind says which packed node that kind comes from. */
        static const char definition[]
            = "(ast"
              "  (node Expr (nonterminal _))"
              "  (node Term (nonterminal _)))";
        static const char source[]
            = "(query"
              "  (rules"
              "    (match (nonterminal Expr) (@node \"Expr\"))))";
        glr_ast_def_t *ast_definition;
        const char *kinds[4];
        glr_ast_t *ast;
        glr_query_t *query;
        glr_query_options_t options;
        char *sexp = NULL;
        size_t sexp_length = 0;

        ast_definition
            = glr_query_ast_from_sexp (definition, strlen (definition), error,
                                       sizeof (error));
        if (ast_definition == NULL)
        {
            fprintf (stderr, "could not read the AST definition: %s\n", error);
            glr_parser_destroy (parser);
            glr_grammar_destroy (grammar);
            return EXIT_FAILURE;
        }
        printf ("AST definition declares %zu kind(s):", 
               glr_query_ast_node_kinds (ast_definition, kinds, 4));
        for (size_t i = 0; i < glr_query_ast_node_kinds (ast_definition, kinds,
                                                         4);
             i++)
        {
            printf (" %s", kinds[i]);
        }
        printf ("\n");

        ast = glr_ast_create ();
        memset (&options, 0, sizeof (options));
        options.ast = ast;

        query = glr_query_compile (source, strlen (source), NULL, error,
                                   sizeof (error));
        if (query == NULL)
        {
            fprintf (stderr, "could not compile the AST query: %s\n", error);
            glr_ast_destroy (ast);
            glr_query_ast_destroy (ast_definition);
            glr_parser_destroy (parser);
            glr_grammar_destroy (grammar);
            return EXIT_FAILURE;
        }

        (void) glr_query_run_ex (query, grammar, result.forest,
                                 result.forest->root, document,
                                 strlen (document), &options, NULL);

        if (glr_ast_root (ast) != NULL
            && glr_ast_to_sexp (glr_ast_root (ast), &sexp, &sexp_length) == 0)
        {
            printf ("built an AST with %zu node(s):\n%s",
                    glr_ast_count_nodes (glr_ast_root (ast)), sexp);
            free (sexp);
        }

        glr_ast_destroy (ast);
        glr_query_destroy (query);
        glr_query_ast_destroy (ast_definition);
    }

    /* --- 3. the same tree as an XML event stream --- */
    {
        char *xml = NULL;
        size_t xml_length = 0;

        if (glr_forest_to_xml (result.forest, grammar, document,
                               strlen (document), &xml, &xml_length)
            == 0)
        {
            printf ("\n%s", xml);
            free (xml);
        }
    }

    glr_parser_destroy (parser);
    glr_grammar_destroy (grammar);
    return EXIT_SUCCESS;
}