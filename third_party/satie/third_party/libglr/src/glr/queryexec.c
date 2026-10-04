/**
 * @file queryexec.c
 * @brief Execution of S-expression queries, the builtin action table, and ASTs.
 *
 * A query walks the parse forest once and offers every visited node to every
 * rule, so the cost is one traversal rather than one per rule. When a rule's
 * pattern matches, its actions run in order with the bindings the pattern
 * captured, and any of them may report text, build AST nodes, rewrite the
 * grammar, reject a disambiguation candidate, emit XML, request a signal, or
 * ask the program to exit.
 *
 * The builtin actions are collected in @ref glr_global_query_actions. An
 * application that wants different behaviour builds its own table with
 * @ref glr_query_actions_register and passes it to @ref glr_query_compile,
 * where it shadows a builtin of the same name.
 */

#include "query-internal.h"

#include <glr/serialization.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Upper bound on the bindings one pattern may capture, and on the rejections one
   run may record inline. Both are generous for hand-written queries; a query
   that needs more gets a clear failure rather than silent truncation. */
#define GLR_QUERY_MAX_BINDINGS 32
#define GLR_QUERY_MAX_REJECTED 64

/* ------------------------------------------------------------------ */
/* Action tables                                                       */
/* ------------------------------------------------------------------ */

int
glr_query_actions_register (glr_query_action_t *table, size_t capacity,
                            const char *name, glr_query_action_fn action,
                            void *user_data, void (*destroy) (void *user_data),
                            const char *summary)
{
    if (table == NULL || name == NULL || name[0] == '\0')
    {
        return -1;
    }
    if (action == NULL && user_data == NULL)
    {
        return -1; /* an entry with neither cannot do anything */
    }
    {
        size_t i = 0;
        while (table[i].name != NULL)
        {
            if (strcmp (table[i].name, name) == 0)
            {
                if (table[i].destroy != NULL && table[i].user_data != NULL)
                {
                    table[i].destroy (table[i].user_data);
                }
                table[i].action = action;
                table[i].user_data = user_data;
                table[i].destroy = destroy;
                table[i].summary = summary;
                return 0;
            }
            if (i + 1 == capacity)
            {
                return -1; /* table is full */
            }
            i++;
        }
        if (capacity == 0)
        {
            return -1;
        }
        table[i].name = name;
        table[i].action = action;
        table[i].user_data = user_data;
        table[i].destroy = destroy;
        table[i].summary = summary;
        return 0;
    }
}

const glr_query_action_t *
glr_query_actions_find (const glr_query_action_t *table, const char *name)
{
    if (table == NULL || name == NULL)
    {
        return NULL;
    }
    for (size_t i = 0; table[i].name != NULL; i++)
    {
        if (strcmp (table[i].name, name) == 0)
        {
            return &table[i];
        }
    }
    return NULL;
}

const glr_query_action_t *
glr_query_resolve_action (const glr_query_action_t *local, const char *name)
{
    const glr_query_action_t *found = glr_query_actions_find (local, name);

    if (found != NULL)
    {
        return found;
    }
    return glr_query_actions_find (glr_global_query_actions, name);
}

size_t
glr_query_actions_count (const glr_query_action_t *table)
{
    size_t count = 0;

    if (table == NULL)
    {
        return 0;
    }
    while (table[count].name != NULL)
    {
        count++;
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* ASTs                                                                */
/* ------------------------------------------------------------------ */

struct glr_ast_t
{
    glr_ast_node_t *root; /* owned */
};

/* strdup() is not part of C11, so the module carries its own copy helper. */
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

glr_ast_node_t *
glr_ast_node_new (const char *name, const char *value, size_t start, size_t end)
{
    glr_ast_node_t *node = calloc (1, sizeof (*node));

    if (node == NULL)
    {
        return NULL;
    }
    if (name != NULL)
    {
        node->name = query_strdup (name);
    }
    if (value != NULL)
    {
        node->value = query_strdup (value);
    }
    node->start = start;
    node->end = end;
    if ((name != NULL && node->name == NULL)
        || (value != NULL && node->value == NULL))
    {
        free (node->name);
        free (node->value);
        free (node);
        return NULL;
    }
    return node;
}

void
glr_ast_node_free (glr_ast_node_t *node)
{
    if (node == NULL)
    {
        return;
    }
    for (size_t i = 0; i < node->child_count; i++)
    {
        glr_ast_node_free (node->children[i]);
    }
    free (node->children);
    free (node->name);
    free (node->value);
    free (node);
}

int
glr_ast_node_add_child (glr_ast_node_t *parent, glr_ast_node_t *child)
{
    if (parent == NULL || child == NULL)
    {
        return -1;
    }
    if (parent->child_count >= parent->capacity)
    {
        size_t capacity = parent->capacity == 0 ? 4 : parent->capacity * 2;
        glr_ast_node_t **grown = realloc (parent->children,
                                         capacity * sizeof (*grown));
        if (grown == NULL)
        {
            return -1;
        }
        parent->children = grown;
        parent->capacity = capacity;
    }
    parent->children[parent->child_count++] = child;
    return 0;
}

glr_ast_t *
glr_ast_create (void)
{
    return calloc (1, sizeof (glr_ast_t));
}

void
glr_ast_destroy (glr_ast_t *ast)
{
    if (ast == NULL)
    {
        return;
    }
    glr_ast_node_free (ast->root);
    free (ast);
}

const glr_ast_node_t *
glr_ast_root (const glr_ast_t *ast)
{
    return ast != NULL ? ast->root : NULL;
}

int
glr_ast_set_root (glr_ast_t *ast, glr_ast_node_t *node)
{
    if (ast == NULL || node == NULL)
    {
        return -1;
    }
    if (ast->root != NULL)
    {
        return -1; /* one root per AST; build a new AST instead */
    }
    ast->root = node;
    return 0;
}

size_t
glr_ast_count_nodes (const glr_ast_node_t *node)
{
    size_t count = 0;

    if (node == NULL)
    {
        return 0;
    }
    count = 1;
    for (size_t i = 0; i < node->child_count; i++)
    {
        count += glr_ast_count_nodes (node->children[i]);
    }
    return count;
}

const glr_ast_node_t *
glr_ast_find (const glr_ast_node_t *node, const char *name)
{
    if (node == NULL || name == NULL)
    {
        return NULL;
    }
    if (node->name != NULL && strcmp (node->name, name) == 0)
    {
        return node;
    }
    for (size_t i = 0; i < node->child_count; i++)
    {
        const glr_ast_node_t *found = glr_ast_find (node->children[i], name);
        if (found != NULL)
        {
            return found;
        }
    }
    return NULL;
}

/* --- S-expression output --- */

struct ast_writer
{
    glr_ast_write_fn write;
    void *user_data;
    bool failed;
};

static void
ast_write_text (struct ast_writer *writer, const char *text)
{
    size_t length = strlen (text);

    if (writer->failed)
    {
        return;
    }
    if (writer->write (text, length, writer->user_data) != 0)
    {
        writer->failed = true;
    }
}

static void
ast_write_quoted (struct ast_writer *writer, const char *text)
{
    bool needs_quotes = (text == NULL) || *text == '\0';

    if (!needs_quotes)
    {
        for (const char *p = text; *p != '\0'; p++)
        {
            if (*p == '(' || *p == ')' || *p == '"' || *p == ' '
                || *p == '\t' || *p == '\n')
            {
                needs_quotes = true;
                break;
            }
        }
    }
    if (!needs_quotes)
    {
        ast_write_text (writer, text);
        return;
    }

    ast_write_text (writer, "\"");
    for (const char *p = text; p != NULL && *p != '\0'; p++)
    {
        char escaped[2] = { *p, '\0' };
        if (*p == '"' || *p == '\\')
        {
            char with_slash[3] = { '\\', *p, '\0' };
            ast_write_text (writer, with_slash);
        }
        else
        {
            ast_write_text (writer, escaped);
        }
    }
    ast_write_text (writer, "\"");
}

static void
ast_write_node (struct ast_writer *writer, const glr_ast_node_t *node)
{
    if (node == NULL || writer->failed)
    {
        return;
    }
    /* A value node is written as a quoted atom so it reads back as one. */
    if (node->name == NULL)
    {
        ast_write_quoted (writer, node->value != NULL ? node->value : "");
        return;
    }

    ast_write_text (writer, "(");
    ast_write_text (writer, node->name);
    for (size_t i = 0; i < node->child_count; i++)
    {
        ast_write_text (writer, " ");
        ast_write_node (writer, node->children[i]);
    }
    ast_write_text (writer, ")");
}

int
glr_ast_write (const glr_ast_node_t *node, glr_ast_write_fn write,
               void *user_data)
{
    struct ast_writer writer;

    if (node == NULL || write == NULL)
    {
        return -1;
    }
    writer.write = write;
    writer.user_data = user_data;
    writer.failed = false;

    ast_write_node (&writer, node);
    ast_write_text (&writer, "\n");
    return writer.failed ? -1 : 0;
}

/* Collects the streaming writer's output so glr_ast_to_sexp() can hand back a
   single NUL-terminated buffer. */
struct ast_collect
{
    char *data;
    size_t length;
    size_t capacity;
    bool failed;
};

static int
ast_collect_write (const char *text, size_t length, void *user_data)
{
    struct ast_collect *collect = user_data;

    if (collect->failed)
    {
        return -1;
    }
    if (collect->length + length + 1 > collect->capacity)
    {
        size_t capacity = collect->capacity == 0 ? 256 : collect->capacity;
        char *grown;
        while (capacity < collect->length + length + 1)
        {
            capacity *= 2;
        }
        grown = realloc (collect->data, capacity);
        if (grown == NULL)
        {
            collect->failed = true;
            return -1;
        }
        collect->data = grown;
        collect->capacity = capacity;
    }
    memcpy (collect->data + collect->length, text, length);
    collect->length += length;
    collect->data[collect->length] = '\0';
    return 0;
}

int
glr_ast_to_sexp (const glr_ast_node_t *node, char **out, size_t *out_length)
{
    struct ast_collect collect;

    if (node == NULL || out == NULL)
    {
        return -1;
    }
    memset (&collect, 0, sizeof (collect));

    if (glr_ast_write (node, ast_collect_write, &collect) != 0)
    {
        free (collect.data);
        return -1;
    }
    if (collect.data == NULL)
    {
        collect.data = calloc (1, 1);
        if (collect.data == NULL)
        {
            return -1;
        }
    }

    *out = collect.data;
    if (out_length != NULL)
    {
        *out_length = collect.length;
    }
    return 0;
}

/* --- S-expression input --- */

static glr_ast_node_t *
ast_from_sexp (const sexp_t *element, char *error, size_t error_size)
{
    glr_ast_node_t *node;

    if (element == NULL)
    {
        return NULL;
    }
    if (element->ty == SEXP_VALUE)
    {
        return glr_ast_node_new (NULL, element->val, 0, 0);
    }
    if (element->list == NULL)
    {
        glr_query_set_error (error, error_size, "empty AST list");
        return NULL;
    }

    node = glr_ast_node_new (element->list->val, NULL, 0, 0);
    if (node == NULL)
    {
        return NULL;
    }
    for (const sexp_t *rest = element->list->next; rest != NULL;
         rest = rest->next)
    {
        glr_ast_node_t *child = ast_from_sexp (rest, error, error_size);
        if (child == NULL || glr_ast_node_add_child (node, child) != 0)
        {
            glr_ast_node_free (node);
            return NULL;
        }
    }
    return node;
}

glr_ast_t *
glr_ast_from_sexp (const char *source, size_t length, char *error,
                   size_t error_size)
{
    sexp_t *root;
    glr_ast_node_t *node;
    glr_ast_t *ast;
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

    node = ast_from_sexp (root, error, error_size);
    destroy_sexp (root);
    if (node == NULL)
    {
        glr_query_set_error (error, error_size, "AST is not valid");
        return NULL;
    }

    ast = glr_ast_create ();
    if (ast == NULL || glr_ast_set_root (ast, node) != 0)
    {
        glr_ast_node_free (node);
        glr_ast_destroy (ast);
        glr_query_set_error (error, error_size, "out of memory creating AST");
        return NULL;
    }
    return ast;
}

/* ------------------------------------------------------------------ */
/* Builtin actions                                                     */
/* ------------------------------------------------------------------ */

/* Report a string, length-delimited, to the run's reporting sink. */
static void
action_report (glr_query_context_t *context, const char *text, size_t length)
{
    if (context == NULL)
    {
        return;
    }
    context->report_count++;
    if (context->report != NULL && text != NULL)
    {
        context->report (text, length, context->report_data);
    }
}

/* The first argument of the action, or "" when it has none. */
static const char *
action_arg (const char *const *args, size_t count, size_t index)
{
    if (args == NULL || index >= count || args[index] == NULL)
    {
        return "";
    }
    return args[index];
}

static const glr_forest_node_t *
action_binding (const glr_query_match_t *match, const char *name)
{
    if (match == NULL || name == NULL || match->binding_names == NULL)
    {
        return NULL;
    }
    for (size_t i = 0; i < match->binding_count; i++)
    {
        if (match->binding_names[i] != NULL
            && strcmp (match->binding_names[i], name) == 0)
        {
            return match->bindings[i];
        }
    }
    return NULL;
}

/* `(@report "prefix" NODE)`: report the text of a bound node, or of the whole
   match when no node is named. */
static glr_query_action_result_t
action_report_match (glr_query_context_t *context,
                     const glr_query_match_t *match, const char *prefix,
                     const char *binding_name)
{
    const glr_forest_node_t *node = match != NULL ? match->matched : NULL;
    size_t start;
    size_t end;

    if (binding_name[0] != '\0')
    {
        node = action_binding (match, binding_name);
    }
    if (node == NULL)
    {
        action_report (context, prefix, strlen (prefix));
        return GLR_QUERY_ACTION_CONTINUE;
    }

    start = node->position;
    end = node->end_position;
    action_report (context, prefix, strlen (prefix));
    if (context->input != NULL && start <= end
        && end <= context->input_length)
    {
        action_report (context, context->input + start, end - start);
    }
    action_report (context, "\n", 1);
    return GLR_QUERY_ACTION_CONTINUE;
}

/* Build the AST node name for a matched node: the action's argument when
   given, otherwise the node's own name.

   A constructor stores a *production* id rather than a symbol id, so naming it
   needs the production's head. Looking the id up as a symbol would silently
   produce the name of an unrelated symbol, which is worse than no name. */
static const char *
action_node_name (glr_query_context_t *context, const glr_forest_node_t *node,
                  const char *given)
{
    const glr_symbol_t *symbol;

    if (given[0] != '\0')
    {
        return given;
    }
    if (node == NULL)
    {
        return "Node";
    }

    if (node->type == GLR_NODE_CONSTRUCTOR)
    {
        const glr_production_t *production
            = glr_grammar_get_production (context->grammar, node->symbol_id);
        if (production != NULL && production->head != NULL
            && production->head->name != NULL)
        {
            return production->head->name;
        }
        return "constructor";
    }

    symbol = glr_grammar_get_symbol (context->grammar, node->symbol_id);
    if (symbol != NULL && symbol->name != NULL)
    {
        return symbol->name;
    }
    return node->type == GLR_NODE_TERMINAL ? "terminal" : "nonterminal";
}

/* Mirror a packed subtree into AST nodes. Terminals become leaves carrying
   their source text, which is what gives an AST its values. */
static glr_ast_node_t *
action_build_ast (glr_query_context_t *context, const glr_forest_node_t *node)
{
    glr_ast_node_t *ast_node;
    char text[256];

    if (node == NULL)
    {
        return NULL;
    }

    if (node->type == GLR_NODE_TERMINAL)
    {
        size_t length = 0;
        if (context->input != NULL && node->position <= node->end_position
            && node->end_position <= context->input_length)
        {
            length = node->end_position - node->position;
            if (length >= sizeof (text))
            {
                length = sizeof (text) - 1;
            }
            memcpy (text, context->input + node->position, length);
        }
        text[length] = '\0';
        return glr_ast_node_new (NULL, text, node->position, node->end_position);
    }

    ast_node = glr_ast_node_new (action_node_name (context, node, ""), NULL,
                             node->position, node->end_position);
    if (ast_node == NULL)
    {
        return NULL;
    }
    for (size_t c = 0; c < node->child_count; c++)
    {
        glr_ast_node_t *child = action_build_ast (context, node->children[c]);
        if (child == NULL || glr_ast_node_add_child (ast_node, child) != 0)
        {
            glr_ast_node_free (ast_node);
            return NULL;
        }
    }
    return ast_node;
}

/* Attach `child` to the AST: to the current root when the AST is empty, and
   otherwise as a sibling under a synthetic list so a flat result stays flat. */
static int
action_attach (glr_query_context_t *context, glr_ast_node_t *child)
{
    if (context->ast == NULL || child == NULL)
    {
        return -1;
    }
    if (context->ast->root == NULL)
    {
        return glr_ast_set_root (context->ast, child);
    }
    if (glr_ast_set_root (context->ast, child) == 0)
    {
        return 0;
    }
    /* The AST already has a root: the new child becomes a sibling of it. */
    {
        glr_ast_node_t *parent = glr_ast_node_new ("list", NULL,
                                               child->start, child->end);
        glr_ast_node_t *previous = context->ast->root;
        int rc;

        if (parent == NULL)
        {
            return -1;
        }
        rc = glr_ast_node_add_child (parent, previous);
        if (rc == 0)
        {
            rc = glr_ast_node_add_child (parent, child);
        }
        if (rc != 0)
        {
            glr_ast_node_free (parent);
            return -1;
        }
        context->ast->root = parent;
        return 0;
    }
}

static glr_query_action_result_t
builtin_node (glr_query_context_t *context, const glr_query_match_t *match,
              const char *const *args, size_t arg_count, void *user_data)
{
    glr_ast_node_t *node;

    (void) match;
    (void) user_data;
    if (context->ast == NULL)
    {
        return GLR_QUERY_ACTION_CONTINUE;
    }

    node = action_build_ast (context, match != NULL ? match->matched : NULL);
    if (node == NULL)
    {
        context->halted = true;
        return GLR_QUERY_ACTION_STOP;
    }

    /* A named `@node()` renames the root of the mirrored subtree. */
    {
        const char *name = action_arg (args, arg_count, 0);
        if (name[0] != '\0' && node->name != NULL)
        {
            char *renamed = query_strdup (name);
            if (renamed != NULL)
            {
                free (node->name);
                node->name = renamed;
            }
        }
    }
    action_attach (context, node);
    return GLR_QUERY_ACTION_CONTINUE;
}

static glr_query_action_result_t
builtin_leaf (glr_query_context_t *context, const glr_query_match_t *match,
              const char *const *args, size_t arg_count, void *user_data)
{
    glr_ast_node_t *leaf;
    const char *value = action_arg (args, arg_count, 0);

    (void) user_data;
    if (context->ast == NULL || match == NULL)
    {
        return GLR_QUERY_ACTION_CONTINUE;
    }

    /* With no argument the leaf carries the matched text; with one it carries
       that literal, which is how a query injects a fixed token. */
    leaf = glr_ast_node_new (NULL, value[0] != '\0' ? value : match->text,
                         match->start, match->end);
    if (leaf == NULL)
    {
        return GLR_QUERY_ACTION_CONTINUE;
    }
    action_attach (context, leaf);
    return GLR_QUERY_ACTION_CONTINUE;
}

static glr_query_action_result_t
builtin_halt (glr_query_context_t *context, const glr_query_match_t *match,
              const char *const *args, size_t arg_count, void *user_data)
{
    (void) match;
    (void) args;
    (void) arg_count;
    (void) user_data;
    context->halted = true;
    return GLR_QUERY_ACTION_STOP;
}

static glr_query_action_result_t
builtin_exit (glr_query_context_t *context, const glr_query_match_t *match,
              const char *const *args, size_t arg_count, void *user_data)
{
    const char *code = action_arg (args, arg_count, 0);

    (void) match;
    (void) user_data;
    context->exit_requested = true;
    context->exit_code = code[0] != '\0' ? atoi (code) : 0;
    context->halted = true;
    return GLR_QUERY_ACTION_STOP;
}

/* `(@signal NAME)`: hand the matched text to the application as a signal.
   Signals are delivered through the same reporting sink, prefixed with the
   signal name, so an application can route them without a second channel. */
static glr_query_action_result_t
builtin_signal (glr_query_context_t *context, const glr_query_match_t *match,
                const char *const *args, size_t arg_count, void *user_data)
{
    const char *name = action_arg (args, arg_count, 0);

    (void) user_data;
    if (match == NULL)
    {
        return GLR_QUERY_ACTION_CONTINUE;
    }
    action_report (context, "@signal ", 8);
    action_report (context, name, strlen (name));
    action_report (context, " ", 1);
    action_report (context, match->text, match->text_length);
    action_report (context, "\n", 1);
    return GLR_QUERY_ACTION_CONTINUE;
}

/* `(@ipc CHANNEL)`: pass the matched text to another process. The text goes
   out through the reporting sink prefixed with the channel, which is what an
   IPC transport keys on. */
static glr_query_action_result_t
builtin_ipc (glr_query_context_t *context, const glr_query_match_t *match,
             const char *const *args, size_t arg_count, void *user_data)
{
    const char *channel = action_arg (args, arg_count, 0);

    (void) user_data;
    if (match == NULL)
    {
        return GLR_QUERY_ACTION_CONTINUE;
    }
    action_report (context, "@ipc ", 5);
    action_report (context, channel, strlen (channel));
    action_report (context, " ", 1);
    action_report (context, match->text, match->text_length);
    action_report (context, "\n", 1);
    return GLR_QUERY_ACTION_CONTINUE;
}

/* `(@reject)`: drop the matched reading. The rejection is recorded in the
   context rather than inside the forest, so the parser's own data structure is
   left alone and the application decides what to do with the list. */
static glr_query_action_result_t
builtin_reject (glr_query_context_t *context, const glr_query_match_t *match,
                const char *const *args, size_t arg_count, void *user_data)
{
    (void) args;
    (void) arg_count;
    (void) user_data;
    if (match == NULL || match->matched == NULL)
    {
        return GLR_QUERY_ACTION_CONTINUE;
    }
    for (size_t i = 0; i < context->rejected_count; i++)
    {
        if (context->rejected[i] == match->matched)
        {
            return GLR_QUERY_ACTION_CONTINUE; /* already rejected */
        }
    }
    /* The array is owned by the walk state, which grows it; a bounded inline
       list keeps the common case allocation-free. */
    if (context->rejected_count < GLR_QUERY_MAX_REJECTED)
    {
        context->rejected[context->rejected_count++] = match->matched;
    }
    action_report (context, "@reject ", 8);
    action_report (context, match->text, match->text_length);
    action_report (context, "\n", 1);
    return GLR_QUERY_ACTION_CONTINUE;
}

/* `(@rename "new-name")`: rewriting. The matched node's symbol is renamed in
   the grammar, which is the smallest useful rewrite a query can perform. */
static glr_query_action_result_t
builtin_rename (glr_query_context_t *context, const glr_query_match_t *match,
                const char *const *args, size_t arg_count, void *user_data)
{
    const char *name = action_arg (args, arg_count, 0);
    glr_symbol_t *symbol;

    (void) user_data;
    if (context->mutable_grammar == NULL || match == NULL
        || match->matched == NULL || name[0] == '\0')
    {
        return GLR_QUERY_ACTION_CONTINUE;
    }

    symbol = glr_grammar_get_symbol (context->mutable_grammar,
                                    match->matched->symbol_id);
    if (symbol == NULL || symbol->name == NULL)
    {
        return GLR_QUERY_ACTION_CONTINUE;
    }

    {
        char *renamed = (char *) glr_stringpool_intern (
            context->mutable_grammar->strings, name);
        if (renamed == NULL)
        {
            return GLR_QUERY_ACTION_CONTINUE;
        }
        symbol->name = renamed;
    }
    return GLR_QUERY_ACTION_CONTINUE;
}

/* `(@xml)`: stream the matched subtree as XML events. */
struct xml_capture
{
    glr_query_context_t *context;
    size_t written;
};

static void
xml_capture_event (glr_xml_event_t event, const glr_xml_event_info_t *info,
                   void *user_data)
{
    struct xml_capture *capture = user_data;

    if (capture == NULL)
    {
        return;
    }
    if (event == GLR_XML_EVENT_LEAF && info->text != NULL)
    {
        action_report (capture->context, info->text, info->text_length);
        capture->written++;
    }
}

static glr_query_action_result_t
builtin_xml (glr_query_context_t *context, const glr_query_match_t *match,
             const char *const *args, size_t arg_count, void *user_data)
{
    struct xml_capture capture;
    char *xml = NULL;
    size_t length = 0;

    (void) args;
    (void) arg_count;
    (void) user_data;
    if (match == NULL || match->matched == NULL)
    {
        return GLR_QUERY_ACTION_CONTINUE;
    }

    (void) xml;
    (void) length;

    /* Report the leaves of the matched subtree in document order, which is
       what a consumer of the stream expects to see. */
    capture.context = context;
    capture.written = 0;
    (void) glr_forest_node_write_xml_events (match->matched, context->grammar,
                                             context->input,
                                             context->input_length,
                                             xml_capture_event, &capture);
    action_report (context, "@xml ", 5);
    action_report (context, match->text, match->text_length);
    action_report (context, "\n", 1);
    return GLR_QUERY_ACTION_CONTINUE;
}

/* Extraction actions: report a bound node's text, or its symbol name. */
static glr_query_action_result_t
builtin_extract (glr_query_context_t *context, const glr_query_match_t *match,
                 const char *const *args, size_t arg_count, void *user_data)
{
    (void) user_data;
    /* `(@extract PREFIX NAME)`: report the text of the node the pattern bound
       to NAME, preceded by PREFIX. With no NAME the whole match is reported. */
    return action_report_match (context, match, action_arg (args, arg_count, 0),
                                action_arg (args, arg_count, 1));
}

static glr_query_action_result_t
builtin_print (glr_query_context_t *context, const glr_query_match_t *match,
               const char *const *args, size_t arg_count, void *user_data)
{
    const char *prefix = action_arg (args, arg_count, 0);

    (void) user_data;
    return action_report_match (context, match, prefix, "");
}

static glr_query_action_result_t
builtin_print_symbol (glr_query_context_t *context,
                      const glr_query_match_t *match, const char *const *args,
                      size_t arg_count, void *user_data)
{
    const glr_forest_node_t *node = match != NULL ? match->matched : NULL;
    const glr_symbol_t *symbol;

    (void) args;
    (void) arg_count;
    (void) user_data;
    if (node == NULL)
    {
        return GLR_QUERY_ACTION_CONTINUE;
    }
    symbol = glr_grammar_get_symbol (context->grammar, node->symbol_id);
    if (symbol != NULL && symbol->name != NULL)
    {
        action_report (context, symbol->name, strlen (symbol->name));
    }
    action_report (context, "\n", 1);
    return GLR_QUERY_ACTION_CONTINUE;
}

/* `(@count)`: count matches, reported as text so the caller can aggregate. */
static glr_query_action_result_t
builtin_count (glr_query_context_t *context, const glr_query_match_t *match,
               const char *const *args, size_t arg_count, void *user_data)
{
    char digits[32];
    int written;

    (void) match;
    (void) args;
    (void) arg_count;
    (void) user_data;
    written = snprintf (digits, sizeof (digits), "@count %zu\n",
                        context->report_count + 1);
    if (written > 0)
    {
        action_report (context, digits, (size_t) written);
    }
    return GLR_QUERY_ACTION_CONTINUE;
}

const glr_query_action_t glr_global_query_actions[] = {
    /* Extraction. */
    { "extract", builtin_extract, NULL, NULL,
      "report the text of a bound node, or of the whole match" },
    { "print", builtin_print, NULL, NULL, "report the matched text" },
    { "print-symbol", builtin_print_symbol, NULL, NULL,
      "report the matched node's symbol name" },
    { "count", builtin_count, NULL, NULL, "count matches as they fire" },

    /* AST building. */
    { "@node", builtin_node, NULL, NULL,
      "build an AST node mirroring the matched subtree" },
    { "@leaf", builtin_leaf, NULL, NULL, "build an AST leaf" },
    { "@xml", builtin_xml, NULL, NULL, "stream the matched subtree as XML" },

    /* Rewriting and disambiguation. */
    { "@rename", builtin_rename, NULL, NULL, "rename the matched symbol" },
    { "@reject", builtin_reject, NULL, NULL, "reject the matched reading" },

    /* Out-of-band communication. */
    { "@signal", builtin_signal, NULL, NULL, "deliver the matched text as a signal" },
    { "@ipc", builtin_ipc, NULL, NULL, "pass the matched text to another process" },

    /* Control flow. */
    { "@halt", builtin_halt, NULL, NULL, "stop walking the current root" },
    { "@exit", builtin_exit, NULL, NULL, "ask the program to exit" },

    { NULL, NULL, NULL, NULL, NULL }
};

/* ------------------------------------------------------------------ */
/* Running                                                             */
/* ------------------------------------------------------------------ */

struct query_walk_state
{
    glr_query_t *query;
    /* When `single_rule` is true only that rule is applied, which is how
       glr_query_count_matches() counts one rule without a second walk. */
    size_t single_rule;
    bool has_single_rule;
    const glr_grammar_t *grammar;
    glr_forest_t *forest;
    const char *input;
    size_t input_length;
    glr_query_context_t context;
    const glr_forest_node_t *rejected[GLR_QUERY_MAX_REJECTED];
    size_t rejected_count;
    glr_query_stats_t stats;
    glr_forest_node_t **seen;
    size_t seen_count;
    size_t seen_capacity;
};

static bool
query_walk_seen (struct query_walk_state *state, glr_forest_node_t *node)
{
    for (size_t i = 0; i < state->seen_count; i++)
    {
        if (state->seen[i] == node)
        {
            return true;
        }
    }
    if (state->seen_count >= state->seen_capacity)
    {
        size_t capacity = state->seen_capacity == 0 ? 64 : state->seen_capacity * 2;
        glr_forest_node_t **grown = realloc (state->seen,
                                             capacity * sizeof (*grown));
        if (grown == NULL)
        {
            return true; /* stop rather than loop forever */
        }
        state->seen = grown;
        state->seen_capacity = capacity;
    }
    state->seen[state->seen_count++] = node;
    return false;
}

/* Build the text a match exposes to its actions. */
static void
query_fill_match (struct query_walk_state *state,
                  const glr_forest_node_t *node, glr_query_match_t *match)
{
    memset (match, 0, sizeof (*match));
    match->matched = node;
    match->start = node->position;
    match->end = node->end_position;
    if (state->input != NULL && node->position <= node->end_position
        && node->end_position <= state->input_length)
    {
        match->text = state->input + node->position;
        match->text_length = node->end_position - node->position;
    }
    else
    {
        match->text = "";
        match->text_length = 0;
    }
}

/* Run every rule against one node, in declaration order. */
static void
query_apply_rules (struct query_walk_state *state,
                   const glr_forest_node_t *node)
{
    size_t rule_count = glr_query_rule_count (state->query);

    for (size_t r = 0; r < rule_count; r++)
    {
        if (state->has_single_rule && r != state->single_rule)
        {
            continue;
        }

        glr_query_match_t match;
        const glr_forest_node_t *bindings[GLR_QUERY_MAX_BINDINGS];
        const char *names[GLR_QUERY_MAX_BINDINGS];
        size_t count = 0;

        if (glr_query_rule_node (state->query, r) == NULL)
        {
            continue;
        }
        if (!glr_query_rule_matches (state->query, r, state->grammar, node,
                                     bindings, names, GLR_QUERY_MAX_BINDINGS,
                                     &count))
        {
            continue;
        }

        state->stats.matches++;
        query_fill_match (state, node, &match);
        match.bindings = bindings;
        match.binding_names = names;
        match.binding_count = count;

        for (size_t a = 0; a < glr_query_rule_action_count (state->query, r);
             a++)
        {
            const glr_query_action_t *action
                = glr_query_rule_action (state->query, r, a);
            const char *const *args = glr_query_rule_action_args (state->query,
                                                                  r, a);
            size_t arg_count = glr_query_rule_action_arg_count (state->query,
                                                                r, a);
            glr_query_action_fn fn;

            if (action == NULL || action->action == NULL)
            {
                continue;
            }
            fn = action->action;

            state->context.match_index = state->stats.actions_run;
            if (fn (&state->context, &match, args, arg_count,
                    action->user_data)
                == GLR_QUERY_ACTION_STOP)
            {
                state->stats.actions_run++;
                state->context.halted = true;
                return;
            }
            state->stats.actions_run++;
            if (state->context.halted || state->context.exit_requested)
            {
                return;
            }
        }

        if (state->context.halted || state->context.exit_requested)
        {
            return;
        }
    }
}

static void
query_walk (struct query_walk_state *state, const glr_forest_node_t *node)
{
    if (state->context.halted || state->context.exit_requested || node == NULL)
    {
        return;
    }
    if (query_walk_seen (state, (glr_forest_node_t *) node))
    {
        return; /* the forest is a DAG: visit each node once */
    }

    state->stats.nodes_visited++;
    query_apply_rules (state, node);
    if (state->context.halted || state->context.exit_requested)
    {
        return;
    }

    for (size_t c = 0; c < node->child_count; c++)
    {
        query_walk (state, node->children[c]);
        if (state->context.halted || state->context.exit_requested)
        {
            return;
        }
    }
}

size_t
glr_query_walk (glr_query_t *query, const glr_grammar_t *grammar,
                glr_forest_t *forest, const glr_forest_node_t *root,
                const char *input, size_t input_length,
                const glr_query_options_t *options, glr_query_stats_t *stats)
{
    struct query_walk_state state;
    glr_query_stats_t local;

    if (query == NULL || grammar == NULL || forest == NULL)
    {
        return 0;
    }
    if (input == NULL && input_length > 0)
    {
        return 0;
    }

    memset (&state, 0, sizeof (state));
    state.query = query;
    state.grammar = grammar;
    state.forest = forest;
    state.input = input;
    state.input_length = input_length;
    state.stats.rules = glr_query_rule_count (query);

    state.context.grammar = grammar;
    /* Rewriting needs a grammar the caller offered as mutable; the const one is
       never cast away here. */
    state.context.mutable_grammar = options != NULL ? options->mutable_grammar
                                                    : NULL;
    state.context.forest = forest;
    state.context.root = root;
    state.context.input = input;
    state.context.input_length = input_length;
    state.context.query = query;
    state.context.rejected = state.rejected;
    if (options != NULL)
    {
        state.context.user_data = options->user_data;
        state.context.report = options->report;
        state.context.report_data = options->user_data;
        state.context.ast = options->ast;
    }

    if (root != NULL)
    {
        query_walk (&state, root);
    }
    else
    {
        for (size_t pos = 0; pos < forest->node_count; pos++)
        {
            for (glr_forest_node_t *node = forest->nodes[pos]; node != NULL;
                 node = node->next)
            {
                query_walk (&state, node);
                if (state.context.halted || state.context.exit_requested)
                {
                    break;
                }
            }
            if (state.context.halted || state.context.exit_requested)
            {
                break;
            }
        }
    }

    local = state.stats;
    if (stats != NULL)
    {
        *stats = local;
    }
    free (state.seen);
    return local.matches;
}

bool
glr_query_was_rejected (const glr_query_context_t *context,
                        const glr_forest_node_t *node)
{
    if (context == NULL || node == NULL || context->rejected == NULL)
    {
        return false;
    }
    for (size_t i = 0; i < context->rejected_count; i++)
    {
        if (context->rejected[i] == node)
        {
            return true;
        }
    }
    return false;
}

size_t
glr_query_run_ex (glr_query_t *query, const glr_grammar_t *grammar,
                  glr_forest_t *forest, const glr_forest_node_t *root,
                  const char *input, size_t input_length,
                  const glr_query_options_t *options,
                  glr_query_stats_t *stats)
{
    return glr_query_walk (query, grammar, forest, root, input, input_length,
                           options, stats);
}

size_t
glr_query_run (glr_query_t *query, const glr_grammar_t *grammar,
               glr_forest_t *forest, const glr_forest_node_t *root,
               const char *input, size_t input_length, void *user_data,
               glr_query_stats_t *stats)
{
    glr_query_options_t options;

    memset (&options, 0, sizeof (options));
    options.user_data = user_data;
    return glr_query_run_ex (query, grammar, forest, root, input, input_length,
                              &options, stats);
}

size_t
glr_query_count_matches (const glr_query_t *query, size_t rule_index,
                         const glr_grammar_t *grammar,
                         const glr_forest_t *forest,
                         const glr_forest_node_t *root)
{
    if (query == NULL || grammar == NULL || forest == NULL)
    {
        return 0;
    }
    if (glr_query_rule_node (query, rule_index) == NULL)
    {
        return 0;
    }

    if (root != NULL)
    {
        /* Count inside one subtree, using the same shared-node rule as the
           walk so a node several paths reach is counted once. */
        struct query_walk_state state;

        memset (&state, 0, sizeof (state));
        state.query = (glr_query_t *) query;
        state.grammar = grammar;
        state.forest = (glr_forest_t *) forest;
        state.single_rule = rule_index;
        state.has_single_rule = true;
        query_walk (&state, root);
        free (state.seen);
        return state.stats.matches;
    }

    {
        size_t count = 0;

        for (size_t pos = 0; pos < forest->node_count; pos++)
        {
            for (glr_forest_node_t *node = forest->nodes[pos]; node != NULL;
                 node = node->next)
            {
                size_t bindings = 0;

                if (glr_query_rule_matches (query, rule_index, grammar, node,
                                            NULL, NULL, 0, &bindings))
                {
                    count++;
                }
            }
        }
        return count;
    }
}
