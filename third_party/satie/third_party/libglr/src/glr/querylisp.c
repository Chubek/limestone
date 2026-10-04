/**
 * @file querylisp.c
 * @brief Parsing and compilation of S-expression queries and AST definitions.
 *
 * This file owns the S-expression side of the query layer: turning query text
 * into compiled patterns and rules with the third_party/sfsexp reader, and
 * turning AST definition text into a lookup of node kinds. Execution lives in
 * queryexec.c, so the syntax and the behaviour can be changed independently.
 *
 * Pattern grammar, as accepted by glr_query_pattern_compile():
 *
 * @code
 *   pattern  := node-pattern
 *   node-pattern := "(" kind [symbol] [binding] [node-pattern ...] ")"
 *   kind     := terminal | nonterminal | constructor | any
 *   symbol   := NAME | "_"
 *   binding  := ("$" | "@") NAME
 * @endcode
 *
 * `any` matches a node of any kind, so `(any Expr)` and `(any _)` are the two
 * useful spellings. A pattern matches a node, not a subtree: the walk in
 * queryexec.c visits every node and asks each pattern whether it applies, which
 * is what lets one traversal serve every rule.
 *
 * Nested patterns match the node's children in order. Each nested pattern takes
 * the next child that fits and every nested pattern has to be taken, so the
 * order a query writes them in is the order it means.
 *
 * Query grammar:
 *
 * @code
 *   query    := "(" "query" [ "(" "name" STRING ")" ] "(" "rules" rule* ")" ")"
 *             | rule*
 *   rule     := "(" "match" pattern action+ ")"
 *   action   := "(" NAME [arg ...] ")"
 * @endcode
 *
 * AST definition grammar:
 *
 * @code
 *   ast      := "(" "ast" node-def* ")"
 *   node-def := "(" "node" NAME pattern ")"
 * @endcode
 */

#include "query-internal.h"

#include "../../third_party/sfsexp/src/sexp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A compiled pattern: a node shape plus the bindings it captures. */
struct glr_query_pattern_t
{
    glr_forest_node_type_t kind;
    bool any_kind;    /* `(any ...)` matches whatever the node is */
    char *symbol;     /* required symbol name, or NULL for a wildcard */
    char *binding;    /* name to bind the matched node to, or NULL */
    char **child_symbols; /* symbols required of the children, may be NULL */
    size_t child_symbol_count;
    glr_query_pattern_t **children; /* child patterns, or NULL */
    size_t child_count;
};

struct glr_query_action_arg
{
    char *text; /* argument as written, owned */
};

struct glr_query_rule
{
    glr_query_pattern_t *pattern;
    /* Resolved actions with their arguments. */
    struct glr_query_rule_action
    {
        const glr_query_action_t *action;
        char **args;
        size_t arg_count;
    } *actions;
    size_t action_count;
};

struct glr_query_t
{
    char *name;
    struct glr_query_rule *rules;
    size_t rule_count;
    const glr_query_action_t *actions; /* borrowed, may be NULL */
    glr_ast_def_t *ast;                /* inline `(ast ...)` definition, may be NULL */
    glr_query_t *owner;                /* query an action's user_data belongs to */
};

struct glr_ast_def_t
{
    char **names;
    char **patterns;
    size_t count;
};

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

/* Parse an `(ast ...)` form that has already been read. Shared with
   glr_query_compile(), which picks up an inline AST block. */
static glr_ast_def_t *glr_query_ast_from_sexp_node (const sexp_t *form,
                                                    char *error,
                                                    size_t error_size);

void
glr_query_set_error (char *buffer, size_t size, const char *message)
{
    if (buffer != NULL && size > 0)
    {
        snprintf (buffer, size, "%s", message);
    }
}

static char *
query_strdup (const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL)
    {
        return NULL;
    }
    length = strlen (text);
    copy = malloc (length + 1);
    if (copy == NULL)
    {
        return NULL;
    }
    memcpy (copy, text, length + 1);
    return copy;
}

/* Text of an atom, or NULL when the element is a list. */
const char *
glr_query_atom_text_of (const sexp_t *element);

bool
glr_query_is_atom_of (const sexp_t *element, const char *name)
{
    const char *text = glr_query_atom_text_of (element);
    return text != NULL && strcmp (text, name) == 0;
}

bool
glr_query_is_list_of (const sexp_t *element, const char *name)
{
    if (element == NULL || element->ty != SEXP_LIST || element->list == NULL)
    {
        return false;
    }
    return glr_query_is_atom_of (element->list, name);
}

const char *
glr_query_atom_text (const void *element)
{
    return glr_query_atom_text_of ((const sexp_t *) element);
}

const char *
glr_query_atom_text_of (const sexp_t *element)
{
    if (element == NULL || element->ty != SEXP_VALUE)
    {
        return NULL;
    }
    return element->val;
}

bool
glr_query_is_atom (const void *element, const char *name)
{
    const char *text = glr_query_atom_text (element);
    return text != NULL && strcmp (text, name) == 0;
}

/* Is this list headed by `name`? */
bool
glr_query_is_list (const void *element, const char *name)
{
    const sexp_t *node = element;

    if (node == NULL || node->ty != SEXP_LIST || node->list == NULL)
    {
        return false;
    }
    return glr_query_is_atom (node->list, name);
}

/* ------------------------------------------------------------------ */
/* Pattern compilation                                                 */
/* ------------------------------------------------------------------ */

static void
query_pattern_free (glr_query_pattern_t *pattern)
{
    if (pattern == NULL)
    {
        return;
    }
    for (size_t i = 0; i < pattern->child_count; i++)
    {
        query_pattern_free (pattern->children[i]);
    }
    free (pattern->children);
    for (size_t i = 0; i < pattern->child_symbol_count; i++)
    {
        free (pattern->child_symbols[i]);
    }
    free (pattern->child_symbols);
    free (pattern->symbol);
    free (pattern->binding);
    free (pattern);
}

void
glr_query_pattern_destroy (glr_query_pattern_t *pattern)
{
    query_pattern_free (pattern);
}

/* Kind names accepted in a pattern. */
static bool
query_parse_kind (const char *text, glr_forest_node_type_t *kind,
                  bool *any_kind)
{
    *any_kind = false;
    if (strcmp (text, "terminal") == 0)
    {
        *kind = GLR_NODE_TERMINAL;
        return true;
    }
    if (strcmp (text, "nonterminal") == 0)
    {
        *kind = GLR_NODE_NONTERMINAL;
        return true;
    }
    if (strcmp (text, "constructor") == 0)
    {
        *kind = GLR_NODE_CONSTRUCTOR;
        return true;
    }
    if (strcmp (text, "any") == 0)
    {
        *any_kind = true;
        *kind = GLR_NODE_TERMINAL; /* unused when any_kind is set */
        return true;
    }
    return false;
}

static glr_query_pattern_t *
query_compile_pattern (const sexp_t *form, char *error, size_t error_size)
{
    glr_query_pattern_t *pattern;
    const sexp_t *kind_form;
    const char *kind_text;
    const char *symbol = NULL;
    const char *binding = NULL;
    size_t child_count = 0;

    if (form == NULL || form->ty != SEXP_LIST || form->list == NULL)
    {
        glr_query_set_error (error, error_size, "pattern must be a list");
        return NULL;
    }

    kind_form = form->list;
    kind_text = glr_query_atom_text_of (kind_form);
    if (kind_text == NULL)
    {
        glr_query_set_error (error, error_size, "pattern must start with a kind name");
        return NULL;
    }

    pattern = calloc (1, sizeof (*pattern));
    if (pattern == NULL)
    {
        glr_query_set_error (error, error_size, "out of memory compiling pattern");
        return NULL;
    }
    if (!query_parse_kind (kind_text, &pattern->kind, &pattern->any_kind))
    {
        glr_query_set_error (error, error_size, "unknown pattern kind");
        free (pattern);
        return NULL;
    }

    for (const sexp_t *rest = kind_form->next; rest != NULL; rest = rest->next)
    {
        const char *text;

        /* Nested lists are child patterns and are compiled below. */
        if (rest->ty == SEXP_LIST)
        {
            continue;
        }
        text = glr_query_atom_text_of (rest);
        if (text == NULL)
        {
            glr_query_set_error (error, error_size, "malformed pattern");
            query_pattern_free (pattern);
            return NULL;
        }
        /* Both `$name` and `@name` bind: queries read more naturally with one
           sigil, and actions are spelled with `@`, so accepting both avoids a
           rule that binds with `$` but names an action with `@`. */
        if ((text[0] == '$' || text[0] == '@') && text[1] != '\0')
        {
            if (binding != NULL)
            {
                glr_query_set_error (error, error_size, "pattern binds twice");
                query_pattern_free (pattern);
                return NULL;
            }
            binding = text + 1;
        }
        else if (symbol == NULL)
        {
            symbol = text;
        }
        else
        {
            glr_query_set_error (error, error_size, "pattern has too many atoms");
            query_pattern_free (pattern);
            return NULL;
        }
    }

    if (symbol != NULL && strcmp (symbol, "_") != 0)
    {
        pattern->symbol = query_strdup (symbol);
        if (pattern->symbol == NULL)
        {
            query_pattern_free (pattern);
            return NULL;
        }
    }
    if (binding != NULL)
    {
        pattern->binding = query_strdup (binding);
        if (pattern->binding == NULL)
        {
            query_pattern_free (pattern);
            return NULL;
        }
    }

    /* Nested lists are child patterns. Counting first keeps the allocation
       exact, which matters when a query holds hundreds of rules. */
    for (const sexp_t *rest = kind_form->next; rest != NULL; rest = rest->next)
    {
        if (rest->ty == SEXP_LIST)
        {
            child_count++;
        }
    }
    if (child_count == 0)
    {
        return pattern;
    }

    pattern->children = calloc (child_count, sizeof (*pattern->children));
    if (pattern->children == NULL)
    {
        query_pattern_free (pattern);
        glr_query_set_error (error, error_size, "out of memory compiling pattern");
        return NULL;
    }
    for (const sexp_t *rest = kind_form->next; rest != NULL; rest = rest->next)
    {
        if (rest->ty != SEXP_LIST)
        {
            continue;
        }
        pattern->children[pattern->child_count] = query_compile_pattern (
            rest, error, error_size);
        if (pattern->children[pattern->child_count] == NULL)
        {
            query_pattern_free (pattern);
            return NULL;
        }
        pattern->child_count++;
    }
    return pattern;
}

glr_query_pattern_t *
glr_query_pattern_compile (const char *source, size_t length, char *error,
                            size_t error_size)
{
    sexp_t *root;
    glr_query_pattern_t *pattern;

    glr_query_set_error (error, error_size, "");
    if (source == NULL)
    {
        glr_query_set_error (error, error_size, "pattern source is null");
        return NULL;
    }

    /* sfsexp takes a mutable buffer, so the text is copied. */
    {
        char *buffer = malloc (length + 1);
        if (buffer == NULL)
        {
            glr_query_set_error (error, error_size, "out of memory copying pattern");
            return NULL;
        }
        memcpy (buffer, source, length);
        buffer[length] = '\0';
        root = parse_sexp (buffer, length);
        free (buffer);
    }
    if (root == NULL)
    {
        glr_query_set_error (error, error_size, "pattern is not valid S-expression");
        return NULL;
    }

    pattern = query_compile_pattern (root, error, error_size);
    destroy_sexp (root);
    if (pattern == NULL)
    {
        glr_query_set_error (error, error_size, "pattern is not valid");
    }
    return pattern;
}

/* ------------------------------------------------------------------ */
/* Pattern matching                                                    */
/* ------------------------------------------------------------------ */

bool
glr_query_pattern_match (const glr_query_pattern_t *pattern,
                         const glr_grammar_t *grammar,
                         const glr_forest_node_t *node,
                         const glr_forest_node_t **out_bindings,
                         size_t binding_capacity, size_t *out_count)
{
    size_t count = 0;

    if (pattern == NULL || node == NULL)
    {
        if (out_count != NULL)
        {
            *out_count = 0;
        }
        return false;
    }

    if (!pattern->any_kind && pattern->kind != node->type)
    {
        if (out_count != NULL)
        {
            *out_count = 0;
        }
        return false;
    }
    if (pattern->symbol != NULL)
    {
        const glr_symbol_t *symbol = glr_grammar_get_symbol (grammar,
                                                            node->symbol_id);
        if (symbol == NULL || symbol->name == NULL
            || strcmp (symbol->name, pattern->symbol) != 0)
        {
            if (out_count != NULL)
            {
                *out_count = 0;
            }
            return false;
        }
    }

    /* Nested patterns match the node's children in order: each child pattern
       takes the next child that fits, and every child pattern has to be taken.
       Matching in order is what makes `(constructor _ (terminal @op)
       (nonterminal _))` mean "an operator with an operand after it" instead of
       "a node that happens to have both". */
    if (pattern->child_count > 0)
    {
        size_t taken = 0;

        for (size_t c = 0; c < node->child_count && taken < pattern->child_count;
             c++)
        {
            if (glr_query_pattern_match (pattern->children[taken], grammar,
                                         node->children[c], NULL, 0, NULL))
            {
                taken++;
            }
        }
        if (taken < pattern->child_count)
        {
            if (out_count != NULL)
            {
                *out_count = 0;
            }
            return false;
        }
    }

    if (pattern->binding != NULL)
    {
        if (out_bindings != NULL && count < binding_capacity)
        {
            out_bindings[count] = node;
        }
        count++;
    }
    if (out_count != NULL)
    {
        *out_count = count;
    }
    return true;
}

/* Collect every binding of a pattern. The children are scanned with the same
   ordered rule glr_query_pattern_match() uses, so the binding reported for a
   nested `$name` is the node the match itself accepted. */
static size_t
query_collect_bindings (const glr_query_pattern_t *pattern,
                        const glr_grammar_t *grammar,
                        const glr_forest_node_t *node,
                        const glr_forest_node_t **out_nodes,
                        const char **out_names, size_t capacity, size_t count)
{
    size_t taken = 0;

    if (pattern == NULL || node == NULL)
    {
        return count;
    }

    if (pattern->binding != NULL)
    {
        if (out_nodes != NULL && count < capacity)
        {
            out_nodes[count] = node;
            out_names[count] = pattern->binding;
        }
        count++;
    }

    for (size_t c = 0; c < node->child_count && taken < pattern->child_count; c++)
    {
        if (!glr_query_pattern_match (pattern->children[taken], grammar,
                                      node->children[c], NULL, 0, NULL))
        {
            continue;
        }
        count = query_collect_bindings (pattern->children[taken], grammar,
                                        node->children[c], out_nodes, out_names,
                                        capacity, count);
        taken++;
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* Rules and queries                                                   */
/* ------------------------------------------------------------------ */

static void
query_rule_free (struct glr_query_rule *rule)
{
    if (rule == NULL)
    {
        return;
    }
    for (size_t a = 0; a < rule->action_count; a++)
    {
        for (size_t i = 0; i < rule->actions[a].arg_count; i++)
        {
            free (rule->actions[a].args[i]);
        }
        free (rule->actions[a].args);
    }
    free (rule->actions);
    query_pattern_free (rule->pattern);
    rule->pattern = NULL;
}

void
glr_query_destroy (glr_query_t *query)
{
    if (query == NULL)
    {
        return;
    }
    for (size_t i = 0; i < query->rule_count; i++)
    {
        query_rule_free (&query->rules[i]);
    }
    free (query->rules);
    free (query->name);
    glr_query_ast_destroy (query->ast);
    free (query);
}

const char *
glr_query_name (const glr_query_t *query)
{
    return query != NULL ? query->name : NULL;
}

size_t
glr_query_rule_count (const glr_query_t *query)
{
    return query != NULL ? query->rule_count : 0;
}

const glr_ast_def_t *
glr_query_ast (const glr_query_t *query)
{
    return query != NULL ? query->ast : NULL;
}

void
glr_query_set_ast (glr_query_t *query, glr_ast_def_t *ast)
{
    if (query == NULL)
    {
        glr_query_ast_destroy (ast);
        return;
    }
    glr_query_ast_destroy (query->ast);
    query->ast = ast;
}

static int
query_compile_rule (glr_query_t *query, const sexp_t *form, char *error,
                    size_t error_size)
{
    struct glr_query_rule *rule;
    size_t action_count = 0;
    bool seen_any_list = false;

    if (form == NULL || form->ty != SEXP_LIST || form->list == NULL)
    {
        glr_query_set_error (error, error_size, "rule must be a list");
        return -1;
    }
    if (!glr_query_is_atom_of (form->list, "match"))
    {
        glr_query_set_error (error, error_size, "rule must start with (match ...)");
        return -1;
    }

    /* The first list after (match ...) is the pattern; the rest are actions, so
       a rule needs at least one of them to be worth running. */
    for (const sexp_t *rest = form->list->next; rest != NULL; rest = rest->next)
    {
        if (rest->ty == SEXP_LIST)
        {
            if (seen_any_list)
            {
                action_count++;
            }
            seen_any_list = true;
        }
    }
    if (!seen_any_list)
    {
        glr_query_set_error (error, error_size, "rule has no pattern");
        return -1;
    }
    if (action_count == 0)
    {
        glr_query_set_error (error, error_size, "rule has no action");
        return -1;
    }

    rule = &query->rules[query->rule_count];
    memset (rule, 0, sizeof (*rule));

    {
        bool seen_pattern = false;
        for (const sexp_t *rest = form->list->next; rest != NULL;
             rest = rest->next)
        {
            if (rest->ty != SEXP_LIST)
            {
                glr_query_set_error (error, error_size, "malformed rule");
                query_rule_free (rule);
                return -1;
            }
            if (!seen_pattern)
            {
                rule->pattern
                    = query_compile_pattern (rest, error, error_size);
                if (rule->pattern == NULL)
                {
                    query_rule_free (rule);
                    return -1;
                }
                seen_pattern = true;
                continue;
            }

            {
                const glr_query_action_t *action;
                size_t arg_count = 0;
                size_t index = rule->action_count;

                if (rest->list == NULL
                    || glr_query_atom_text_of (rest->list) == NULL)
                {
                    glr_query_set_error (error, error_size, "action must be named");
                    query_rule_free (rule);
                    return -1;
                }
                action = glr_query_resolve_action (
                    query->actions, glr_query_atom_text_of (rest->list));
                if (action == NULL)
                {
                    char message[160];
                    snprintf (message, sizeof (message),
                              "unknown action '%s'",
                              glr_query_atom_text_of (rest->list));
                    glr_query_set_error (error, error_size, message);
                    query_rule_free (rule);
                    return -1;
                }

                rule->actions = realloc (
                    rule->actions, (index + 1) * sizeof (*rule->actions));
                if (rule->actions == NULL)
                {
                    glr_query_set_error (error, error_size, "out of memory");
                    query_rule_free (rule);
                    return -1;
                }
                memset (&rule->actions[index], 0,
                        sizeof (rule->actions[index]));
                rule->actions[index].action = action;

                for (const sexp_t *arg = rest->list->next; arg != NULL;
                     arg = arg->next)
                {
                    arg_count++;
                }
                if (arg_count > 0)
                {
                    size_t i = 0;
                    rule->actions[index].args = calloc (
                        arg_count, sizeof (*rule->actions[index].args));
                    if (rule->actions[index].args == NULL)
                    {
                        glr_query_set_error (error, error_size, "out of memory");
                        query_rule_free (rule);
                        return -1;
                    }
                    for (const sexp_t *arg = rest->list->next; arg != NULL;
                         arg = arg->next, i++)
                    {
                        const char *text = glr_query_atom_text_of (arg);
                        if (text == NULL)
                        {
                            text = "";
                        }
                        rule->actions[index].args[i] = query_strdup (text);
                        if (rule->actions[index].args[i] == NULL)
                        {
                            glr_query_set_error (error, error_size, "out of memory");
                            query_rule_free (rule);
                            return -1;
                        }
                    }
                    rule->actions[index].arg_count = arg_count;
                }
                rule->action_count = index + 1;
            }
        }
    }

    query->rule_count++;
    return 0;
}

glr_query_t *
glr_query_compile (const char *source, size_t length,
                   const glr_query_action_t *actions, char *error,
                   size_t error_size)
{
    sexp_t *root;
    glr_query_t *query;
    size_t rule_count = 0;

    glr_query_set_error (error, error_size, "");
    if (source == NULL)
    {
        glr_query_set_error (error, error_size, "query source is null");
        return NULL;
    }

    {
        char *buffer = malloc (length + 1);
        if (buffer == NULL)
        {
            glr_query_set_error (error, error_size, "out of memory copying query");
            return NULL;
        }
        memcpy (buffer, source, length);
        buffer[length] = '\0';
        root = parse_sexp (buffer, length);
        free (buffer);
    }
    if (root == NULL)
    {
        glr_query_set_error (error, error_size, "query is not valid S-expression");
        return NULL;
    }

    query = calloc (1, sizeof (*query));
    if (query == NULL)
    {
        destroy_sexp (root);
        glr_query_set_error (error, error_size, "out of memory creating query");
        return NULL;
    }
    query->actions = actions;

    /* A `(query ...)` wrapper is optional, so accept both the wrapped form and
       a bare list of rules. */
    {
        const sexp_t *forms = NULL;

        if (glr_query_is_list_of (root, "query"))
        {
            for (const sexp_t *rest = root->list->next; rest != NULL;
                 rest = rest->next)
            {
                if (glr_query_is_list_of (rest, "name"))
                {
                    const char *text
                        = glr_query_atom_text_of (rest->list->next);
                    if (text != NULL)
                    {
                        query->name = query_strdup (text);
                    }
                    continue;
                }
                if (glr_query_is_list_of (rest, "ast"))
                {
                    glr_ast_def_t *definition = glr_query_ast_from_sexp_node (
                        rest, error, error_size);
                    if (definition == NULL)
                    {
                        glr_query_destroy (query);
                        destroy_sexp (root);
                        return NULL;
                    }
                    glr_query_set_ast (query, definition);
                    continue;
                }
                if (glr_query_is_list_of (rest, "rules"))
                {
                    forms = rest->list;
                    continue;
                }
            }
            if (forms == NULL)
            {
                glr_query_destroy (query);
                destroy_sexp (root);
                glr_query_set_error (error, error_size, "query has no (rules ...) block");
                return NULL;
            }
        }
        else
        {
            forms = root;
        }

        for (const sexp_t *rest = forms->next; rest != NULL; rest = rest->next)
        {
            rule_count++;
        }
        if (rule_count == 0)
        {
            glr_query_destroy (query);
            destroy_sexp (root);
            glr_query_set_error (error, error_size, "query has no rules");
            return NULL;
        }

        query->rules = calloc (rule_count, sizeof (*query->rules));
        if (query->rules == NULL)
        {
            glr_query_destroy (query);
            destroy_sexp (root);
            glr_query_set_error (error, error_size, "out of memory creating query");
            return NULL;
        }

        for (const sexp_t *rest = forms->next; rest != NULL; rest = rest->next)
        {
            if (query_compile_rule (query, rest, error, error_size) != 0)
            {
                glr_query_destroy (query);
                destroy_sexp (root);
                return NULL;
            }
        }
    }

    destroy_sexp (root);
    return query;
}

/* ------------------------------------------------------------------ */
/* AST definitions                                                     */
/* ------------------------------------------------------------------ */



void
glr_query_ast_destroy (glr_ast_def_t *definition)
{
    if (definition == NULL)
    {
        return;
    }
    for (size_t i = 0; i < definition->count; i++)
    {
        free (definition->names[i]);
        free (definition->patterns[i]);
    }
    free (definition->names);
    free (definition->patterns);
    free (definition);
}

static int
glr_query_ast_add (glr_ast_def_t *definition, const char *name,
                   const char *pattern_text)
{
    char **names = realloc (definition->names,
                            (definition->count + 1) * sizeof (*names));
    char **patterns;

    if (names == NULL)
    {
        return -1;
    }
    definition->names = names;
    patterns = realloc (definition->patterns,
                        (definition->count + 1) * sizeof (*patterns));
    if (patterns == NULL)
    {
        return -1;
    }
    definition->patterns = patterns;

    definition->names[definition->count] = query_strdup (name);
    definition->patterns[definition->count] = query_strdup (pattern_text);
    if (definition->names[definition->count] == NULL
        || definition->patterns[definition->count] == NULL)
    {
        return -1;
    }
    definition->count++;
    return 0;
}

/* Render a parsed S-expression back to text. AST definitions keep their
   patterns as source so a query can compile them lazily against whichever
   grammar it runs on, which means they have to be stored as text. */
struct sexp_render
{
    char *data;
    size_t length;
    size_t capacity;
    bool failed;
};

static void
render_append (struct sexp_render *render, const char *bytes, size_t length)
{
    if (render->failed)
    {
        return;
    }
    if (render->length + length + 1 > render->capacity)
    {
        size_t capacity = render->capacity == 0 ? 256 : render->capacity;
        char *grown;
        while (capacity < render->length + length + 1)
        {
            capacity *= 2;
        }
        grown = realloc (render->data, capacity);
        if (grown == NULL)
        {
            render->failed = true;
            return;
        }
        render->data = grown;
        render->capacity = capacity;
    }
    memcpy (render->data + render->length, bytes, length);
    render->length += length;
    render->data[render->length] = '\0';
}

static void
render_puts (struct sexp_render *render, const char *bytes)
{
    render_append (render, bytes, strlen (bytes));
}

/* Quote an atom when it would not read back as one. */
static void
render_atom (struct sexp_render *render, const char *text)
{
    bool needs_quotes = (text == NULL) || *text == '\0';

    if (!needs_quotes)
    {
        for (const char *p = text; *p != '\0'; p++)
        {
            if (*p == '(' || *p == ')' || *p == '"' || *p == '\''
                || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            {
                needs_quotes = true;
                break;
            }
        }
    }
    if (!needs_quotes)
    {
        render_puts (render, text);
        return;
    }

    render_puts (render, "\"");
    for (const char *p = text; p != NULL && *p != '\0'; p++)
    {
        if (*p == '"' || *p == '\\')
        {
            char escaped[3] = { '\\', *p, '\0' };
            render_puts (render, escaped);
        }
        else
        {
            char single[2] = { *p, '\0' };
            render_puts (render, single);
        }
    }
    render_puts (render, "\"");
}

static void
render_sexp (struct sexp_render *render, const sexp_t *element)
{
    if (element == NULL)
    {
        return;
    }
    if (element->ty == SEXP_LIST)
    {
        render_puts (render, "(");
        for (const sexp_t *rest = element->list; rest != NULL;
             rest = rest->next)
        {
            render_sexp (render, rest);
            render_puts (render, " ");
        }
        render_puts (render, ")");
        return;
    }
    render_atom (render, element->val);
}

static int
render_sexp_to_text (const sexp_t *element, char **out, size_t *out_length)
{
    struct sexp_render render;

    memset (&render, 0, sizeof (render));
    render_sexp (&render, element);
    if (render.failed)
    {
        free (render.data);
        return -1;
    }
    if (render.data == NULL)
    {
        render.data = calloc (1, 1);
        if (render.data == NULL)
        {
            return -1;
        }
    }
    *out = render.data;
    if (out_length != NULL)
    {
        *out_length = render.length;
    }
    return 0;
}

static glr_ast_def_t *
glr_query_ast_from_sexp_node (const sexp_t *form, char *error,
                              size_t error_size)
{
    glr_ast_def_t *definition = calloc (1, sizeof (*definition));
    char *buffer;
    size_t length = 0;

    glr_query_set_error (error, error_size, "");
    if (definition == NULL)
    {
        glr_query_set_error (error, error_size, "out of memory");
        return NULL;
    }
    if (form == NULL || form->ty != SEXP_LIST || form->list == NULL
        || !glr_query_is_atom (form->list, "ast"))
    {
        glr_query_set_error (error, error_size, "AST definition must be (ast ...)");
        glr_query_ast_destroy (definition);
        return NULL;
    }

    for (const sexp_t *rest = form->list->next; rest != NULL; rest = rest->next)
    {
        const char *name;

        if (!glr_query_is_list_of (rest, "node"))
        {
            glr_query_set_error (error, error_size, "AST entries must be (node ...)");
            glr_query_ast_destroy (definition);
            return NULL;
        }
        if (rest->list->next == NULL || rest->list->next->next == NULL)
        {
            glr_query_set_error (error, error_size,
                         "(node NAME PATTERN) needs a name and a pattern");
            glr_query_ast_destroy (definition);
            return NULL;
        }
        name = glr_query_atom_text_of (rest->list->next);
        if (name == NULL)
        {
            glr_query_set_error (error, error_size, "node name must be an atom");
            glr_query_ast_destroy (definition);
            return NULL;
        }

        /* The pattern is stored as text so a query can compile it lazily
           against whichever grammar it runs on. */
        if (render_sexp_to_text (rest->list->next->next, &buffer, &length) != 0)
        {
            glr_query_set_error (error, error_size, "out of memory rendering pattern");
            glr_query_ast_destroy (definition);
            return NULL;
        }
        if (glr_query_ast_add (definition, name, buffer) != 0)
        {
            free (buffer);
            glr_query_set_error (error, error_size, "out of memory");
            glr_query_ast_destroy (definition);
            return NULL;
        }
        free (buffer);
    }

    if (definition->count == 0)
    {
        glr_query_set_error (error, error_size, "AST definition declares no nodes");
        glr_query_ast_destroy (definition);
        return NULL;
    }
    return definition;
}

glr_ast_def_t *
glr_query_ast_from_sexp (const char *source, size_t length, char *error,
                         size_t error_size)
{
    sexp_t *root;
    glr_ast_def_t *definition;
    char *buffer;

    glr_query_set_error (error, error_size, "");
    if (source == NULL)
    {
        glr_query_set_error (error, error_size, "AST source is null");
        return NULL;
    }

    buffer = malloc (length + 1);
    if (buffer == NULL)
    {
        glr_query_set_error (error, error_size, "out of memory copying AST");
        return NULL;
    }
    memcpy (buffer, source, length);
    buffer[length] = '\0';
    root = parse_sexp (buffer, length);
    free (buffer);
    if (root == NULL)
    {
        glr_query_set_error (error, error_size, "AST is not valid S-expression");
        return NULL;
    }

    definition = glr_query_ast_from_sexp_node (root, error, error_size);
    destroy_sexp (root);
    return definition;
}

glr_ast_def_t *
glr_query_ast_load_file (const char *path, char *error, size_t error_size)
{
    FILE *stream;
    char *buffer;
    size_t capacity = 4096;
    size_t length = 0;
    glr_ast_def_t *definition;

    glr_query_set_error (error, error_size, "");
    if (path == NULL)
    {
        glr_query_set_error (error, error_size, "path is null");
        return NULL;
    }
    stream = fopen (path, "rb");
    if (stream == NULL)
    {
        glr_query_set_error (error, error_size, "cannot open AST definition file");
        return NULL;
    }

    buffer = malloc (capacity);
    if (buffer == NULL)
    {
        fclose (stream);
        glr_query_set_error (error, error_size, "out of memory reading AST");
        return NULL;
    }
    for (;;)
    {
        size_t got = fread (buffer + length, 1, capacity - length - 1, stream);
        length += got;
        if (length + 1 < capacity)
        {
            break; /* short read means end of file */
        }
        {
            char *grown = realloc (buffer, capacity * 2);
            if (grown == NULL)
            {
                free (buffer);
                fclose (stream);
                glr_query_set_error (error, error_size, "out of memory reading AST");
                return NULL;
            }
            buffer = grown;
            capacity *= 2;
        }
    }
    fclose (stream);
    buffer[length] = '\0';

    definition = glr_query_ast_from_sexp (buffer, length, error, error_size);
    free (buffer);
    return definition;
}

const char *
glr_query_ast_rule (const glr_ast_def_t *definition, const char *name)
{
    if (definition == NULL || name == NULL)
    {
        return NULL;
    }
    for (size_t i = 0; i < definition->count; i++)
    {
        if (strcmp (definition->names[i], name) == 0)
        {
            return definition->patterns[i];
        }
    }
    return NULL;
}

size_t
glr_query_ast_node_kinds (const glr_ast_def_t *definition, const char **out_names,
                          size_t capacity)
{
    if (definition == NULL)
    {
        return 0;
    }
    for (size_t i = 0; i < definition->count && i < capacity; i++)
    {
        if (out_names != NULL)
        {
            out_names[i] = definition->names[i];
        }
    }
    return definition->count;
}

/* ------------------------------------------------------------------ */
/* Rule accessors for queryexec.c                                      */
/* ------------------------------------------------------------------ */

const struct glr_query_pattern_t *
glr_query_rule_node (const glr_query_t *query, size_t rule_index)
{
    if (query == NULL || rule_index >= query->rule_count)
    {
        return NULL;
    }
    return query->rules[rule_index].pattern;
}

bool
glr_query_rule_matches (const glr_query_t *query, size_t rule_index,
                        const glr_grammar_t *grammar,
                        const glr_forest_node_t *node,
                        const glr_forest_node_t **out_bindings,
                        const char **out_names, size_t binding_capacity,
                        size_t *out_count)
{
    if (query == NULL || rule_index >= query->rule_count)
    {
        if (out_count != NULL)
        {
            *out_count = 0;
        }
        return false;
    }
    if (!glr_query_pattern_match (query->rules[rule_index].pattern, grammar,
                                 node, NULL, 0, NULL))
    {
        if (out_count != NULL)
        {
            *out_count = 0;
        }
        return false;
    }

    /* The pattern fits, so gather the bindings, recursing into the parts of it
       that also matched. */
    if (out_count != NULL)
    {
        *out_count = query_collect_bindings (query->rules[rule_index].pattern,
                                             grammar, node, out_bindings,
                                             out_names, binding_capacity, 0);
    }
    return true;
}

size_t
glr_query_rule_action_count (const glr_query_t *query, size_t rule_index)
{
    if (query == NULL || rule_index >= query->rule_count)
    {
        return 0;
    }
    return query->rules[rule_index].action_count;
}

const glr_query_action_t *
glr_query_rule_action (const glr_query_t *query, size_t rule_index,
                       size_t action_index)
{
    if (query == NULL || rule_index >= query->rule_count
        || action_index >= query->rules[rule_index].action_count)
    {
        return NULL;
    }
    return query->rules[rule_index].actions[action_index].action;
}

const char *const *
glr_query_rule_action_args (const glr_query_t *query, size_t rule_index,
                            size_t action_index)
{
    if (query == NULL || rule_index >= query->rule_count
        || action_index >= query->rules[rule_index].action_count)
    {
        return NULL;
    }
    return (const char *const *) query->rules[rule_index]
        .actions[action_index].args;
}

size_t
glr_query_rule_action_arg_count (const glr_query_t *query, size_t rule_index,
                                 size_t action_index)
{
    if (query == NULL || rule_index >= query->rule_count
        || action_index >= query->rules[rule_index].action_count)
    {
        return 0;
    }
    return query->rules[rule_index].actions[action_index].arg_count;
}
