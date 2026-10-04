#include <glr/semantic-action.h>
#include "containers.h"
#include "grammar-internal.h"

#include <klib/khash.h>
#include <string.h>

KHASH_MAP_INIT_INT64 (glr_evaluated, size_t)

typedef struct
{
  const glr_forest_node_t *node;
  size_t choice;
  size_t next_child;
  unsigned mark;
  void *value;
} eval_entry_t;

#define T eval_entry_t
#define P
#include <ctl/vec.h>
#define T size_t
#define P
#include <ctl/vec.h>

int
glr_production_set_semantic_action (glr_grammar_t *grammar, int production_id,
                                    glr_semantic_action_fn action,
                                    void *action_data,
                                    glr_semantic_data_destroy_fn destroy)
{
  glr_production_t *production = glr_grammar_get_production (grammar, production_id);
  struct glr_semantic_action *replacement = NULL;
  if (production == NULL)
    return -1;
  if (action != NULL)
    {
      replacement = malloc (sizeof (*replacement));
      if (replacement == NULL)
        return -1;
      replacement->callback = action;
      replacement->data = action_data;
      replacement->destroy = destroy;
    }
  if (production->semantic_action != NULL)
    {
      struct glr_semantic_action *old = production->semantic_action;
      if (old->destroy != NULL && (replacement == NULL || old->data != action_data))
        old->destroy (old->data);
      free (old);
    }
  production->semantic_action = replacement;
  return 0;
}

bool
glr_grammar_has_semantic_actions (const glr_grammar_t *grammar)
{
  if (grammar == NULL)
    return false;
  for (size_t i = 0; i < grammar->production_count; i++)
    if (grammar->productions[i] != NULL
        && grammar->productions[i]->semantic_action != NULL)
      return true;
  return false;
}

static int
remember (khash_t (glr_evaluated) *index, vec_eval_entry_t *entries,
           const glr_forest_node_t *node, size_t *id)
{
  khint_t slot = kh_get (glr_evaluated, index, (uintptr_t) node);
  eval_entry_t entry = { node, 0, 0, 0, NULL };
  eval_entry_t *grown;
  int inserted;
  if (slot != kh_end (index))
    {
      *id = kh_val (index, slot);
      return 0;
    }
  grown = GLR_VECTOR_RESERVE (entries, entries->size + 1);
  if (grown == NULL)
    return -1;
  entries->value = grown;
  slot = kh_put (glr_evaluated, index, (uintptr_t) node, &inserted);
  if (inserted < 0)
    return -1;
  *id = entries->size;
  kh_val (index, slot) = *id;
  vec_eval_entry_t_push_back (entries, entry);
  return 0;
}

static int
push_index (vec_size_t *vector, size_t id)
{
  size_t *grown = GLR_VECTOR_RESERVE (vector, vector->size + 1);
  if (grown == NULL)
    return -1;
  vector->value = grown;
  vec_size_t_push_back (vector, id);
  return 0;
}

static glr_semantic_error_t
validate_entry (const glr_grammar_t *grammar, eval_entry_t *entry, size_t length,
                 glr_semantic_resolver_fn resolver, void *user_data)
{
  const glr_forest_node_t *node = entry->node;
  if (node == NULL || node->position > node->end_position
      || node->end_position > length
      || (node->child_count != 0 && node->children == NULL))
    return GLR_SEMANTIC_ERROR_ARGUMENT;
  if (node->type == GLR_NODE_TERMINAL)
    {
      glr_symbol_t *symbol = glr_grammar_get_symbol (grammar, node->symbol_id);
      return glr_symbol_is_terminal (symbol) && node->child_count == 0
                 ? GLR_SEMANTIC_SUCCESS : GLR_SEMANTIC_ERROR_ARGUMENT;
    }
  if (node->type == GLR_NODE_NONTERMINAL)
    {
      glr_symbol_t *symbol = glr_grammar_get_symbol (grammar, node->symbol_id);
      const glr_forest_node_t *child;
      glr_production_t *production;
      if (!glr_symbol_is_nonterminal (symbol) || node->child_count == 0)
        return GLR_SEMANTIC_ERROR_ARGUMENT;
      if (node->child_count > 1)
        {
          if (resolver == NULL)
            return GLR_SEMANTIC_ERROR_AMBIGUOUS;
          entry->choice = resolver (node, user_data);
          if (entry->choice >= node->child_count)
            return GLR_SEMANTIC_ERROR_AMBIGUOUS;
        }
      child = node->children[entry->choice];
      if (child == NULL || child->type != GLR_NODE_CONSTRUCTOR)
        return GLR_SEMANTIC_ERROR_ARGUMENT;
      production = glr_grammar_get_production (grammar, child->symbol_id);
      if (production == NULL || production->head != symbol
          || child->position != node->position
          || child->end_position != node->end_position)
        return GLR_SEMANTIC_ERROR_ARGUMENT;
      return GLR_SEMANTIC_SUCCESS;
    }
  if (node->type == GLR_NODE_CONSTRUCTOR)
    {
      glr_production_t *production = glr_grammar_get_production (grammar, node->symbol_id);
      if (production == NULL || production->body_length != node->child_count)
        return GLR_SEMANTIC_ERROR_ARGUMENT;
      for (size_t i = 0; i < node->child_count; i++)
        {
          const glr_forest_node_t *child = node->children[i];
          if (child == NULL || child->symbol_id != production->body[i]->id
              || child->position < node->position
              || child->end_position > node->end_position
              || (production->body[i]->type == GLR_SYMBOL_TERMINAL
                  ? child->type != GLR_NODE_TERMINAL
                  : child->type != GLR_NODE_NONTERMINAL))
            return GLR_SEMANTIC_ERROR_ARGUMENT;
        }
      return GLR_SEMANTIC_SUCCESS;
    }
  return GLR_SEMANTIC_ERROR_ARGUMENT;
}

static void *
lookup_value (khash_t (glr_evaluated) *index, const vec_eval_entry_t *entries,
               const glr_forest_node_t *node)
{
  khint_t slot = kh_get (glr_evaluated, index, (uintptr_t) node);
  return slot != kh_end (index) ? entries->value[kh_val (index, slot)].value : NULL;
}

glr_semantic_error_t
glr_semantic_evaluate (const glr_grammar_t *grammar, const glr_forest_t *forest,
                       const char *input, size_t length,
                       glr_semantic_resolver_fn resolver, void *user_data,
                       void **value)
{
  khash_t (glr_evaluated) *index = NULL;
  vec_eval_entry_t entries = vec_eval_entry_t_init ();
  vec_size_t stack = vec_size_t_init ();
  vec_size_t order = vec_size_t_init ();
  glr_semantic_error_t error = GLR_SEMANTIC_ERROR_MEMORY;
  void **values = NULL;
  size_t values_capacity = 0;
  size_t root;
  if (value != NULL)
    *value = NULL;
  if (grammar == NULL || forest == NULL || forest->root == NULL
      || input == NULL || value == NULL)
    return GLR_SEMANTIC_ERROR_ARGUMENT;
  index = kh_init (glr_evaluated);
  if (index == NULL || remember (index, &entries, forest->root, &root) != 0
      || push_index (&stack, root) != 0)
    goto cleanup;

  /* Explicit DFS stack: long operator chains need no C recursion. Build a
     postorder only after resolving every ambiguity and checking for cycles. */
  while (stack.size != 0)
    {
      size_t id = *vec_size_t_back (&stack);
      eval_entry_t *entry = &entries.value[id];
      const glr_forest_node_t *node = entry->node;
      size_t count;
      if (entry->mark == 0)
        {
          error = validate_entry (grammar, entry, length, resolver, user_data);
          if (error != GLR_SEMANTIC_SUCCESS)
            goto cleanup;
          entry->mark = 1;
        }
      count = node->type == GLR_NODE_NONTERMINAL ? 1 : node->child_count;
      if (entry->next_child < count)
        {
          const glr_forest_node_t *child = node->children[
              node->type == GLR_NODE_NONTERMINAL ? entry->choice : entry->next_child];
          size_t child_id;
          entry->next_child++;
          error = GLR_SEMANTIC_ERROR_MEMORY;
          if (remember (index, &entries, child, &child_id) != 0)
            goto cleanup;
          if (entries.value[child_id].mark == 1)
            {
              error = GLR_SEMANTIC_ERROR_CYCLE;
              goto cleanup;
            }
          if (entries.value[child_id].mark == 0 && push_index (&stack, child_id) != 0)
            goto cleanup;
          continue;
        }
      entry->mark = 2;
      vec_size_t_pop_back (&stack);
      error = GLR_SEMANTIC_ERROR_MEMORY;
      if (push_index (&order, id) != 0)
        goto cleanup;
    }

  for (size_t i = 0; i < order.size; i++)
    {
      eval_entry_t *entry = &entries.value[order.value[i]];
      const glr_forest_node_t *node = entry->node;
      if (node->type == GLR_NODE_NONTERMINAL)
        entry->value = lookup_value (index, &entries, node->children[entry->choice]);
      else if (node->type == GLR_NODE_TERMINAL)
        entry->value = (void *) node;
      else
        {
          glr_production_t *production = glr_grammar_get_production (grammar, node->symbol_id);
          struct glr_semantic_action *action = production->semantic_action;
          entry->value = node->child_count == 1
                             ? lookup_value (index, &entries, node->children[0])
                             : (void *) node;
          if (action != NULL)
            {
              glr_semantic_context_t context;
              if (node->child_count != 0)
                {
                  void **grown = glr_array_grow (values, &values_capacity,
                                                node->child_count, sizeof (*values));
                  if (grown == NULL)
                    {
                      error = GLR_SEMANTIC_ERROR_MEMORY;
                      goto cleanup;
                    }
                  values = grown;
                }
              for (size_t c = 0; c < node->child_count; c++)
                values[c] = lookup_value (index, &entries, node->children[c]);
              memset (&context, 0, sizeof (context));
              context.selection.production = production;
              context.selection.children = node->children;
              context.selection.child_count = node->child_count;
              context.selection.values = values;
              context.selection.input = input;
              context.selection.input_length = length;
              context.grammar = grammar;
              context.constructor = node;
              context.user_data = user_data;
              if (action->callback (&context, &entry->value, action->data) != 0)
                {
                  error = GLR_SEMANTIC_ERROR_ACTION;
                  goto cleanup;
                }
            }
        }
    }
  *value = entries.value[root].value;
  error = GLR_SEMANTIC_SUCCESS;
cleanup:
  free (values);
  kh_destroy (glr_evaluated, index);
  vec_eval_entry_t_free (&entries);
  vec_size_t_free (&stack);
  vec_size_t_free (&order);
  return error;
}
