/** GLR execution with immutable configurations, a CTL agenda and klib hash
    deduplication. Each LR action forks the original configuration; each lexical
    alternative retains its own lookahead and byte cursor through reductions. */
#include <glr/parser.h>
#include <glr/scannerless.h>
#include "containers.h"

#include <klib/khash.h>
#include <klib/kstring.h>
#include <limits.h>
#include <string.h>

#define GLR_SCAN_LOOKAHEAD (-2)
#define GLR_MAX_REDUCE_CHAIN 4096

typedef struct
{
  glr_stack_t *stack;
  size_t position;
  int terminal;
  size_t length;
  size_t reductions;
  khint_t hash;
} parse_task_t;
typedef parse_task_t *parse_task_ptr_t;

#define T parse_task_ptr_t
#define P
#include <ctl/vec.h>

static khint_t
task_hash (parse_task_ptr_t task)
{
  return task->hash;
}

static int
task_equal (parse_task_ptr_t a, parse_task_ptr_t b)
{
  if (a->position != b->position || a->terminal != b->terminal
      || a->length != b->length || a->stack->height != b->stack->height)
    return 0;
  for (size_t i = 0; i < a->stack->height; i++)
    {
      glr_stack_node_t *x = a->stack->states[i];
      glr_stack_node_t *y = b->stack->states[i];
      if (glr_stack_node_get_state (x) != glr_stack_node_get_state (y)
          || glr_stack_node_get_forest_node (x) != glr_stack_node_get_forest_node (y))
        return 0;
    }
  return 1;
}

KHASH_INIT (glr_configurations, parse_task_ptr_t, char, 0, task_hash, task_equal)

typedef struct
{
  glr_parser_t *parser;
  glr_parse_table_t *table;
  khash_t (glr_configurations) *seen;
  vec_parse_task_ptr_t pending;
  vec_parse_task_ptr_t completed;
  size_t completed_position;
  glr_terminal_match_t *matches;
  size_t match_count;
  size_t cached_position;
  size_t furthest;
  bool reader;
  bool scannerless;
} parse_run_t;

static char *
copy_text (const char *text)
{
  kstring_t copy = { 0, 0, NULL };
  if (text == NULL || strlen (text) > INT_MAX || kputs (text, &copy) < 0)
    {
      free (copy.s);
      return NULL;
    }
  return ks_release (&copy);
}

static bool
is_utf16 (const char *input, size_t length)
{
  const unsigned char *bytes = (const unsigned char *) input;
  if (input == NULL || length < 2 || length % 2 != 0)
    return false;
  if ((bytes[0] == 0xff && bytes[1] == 0xfe)
      || (bytes[0] == 0xfe && bytes[1] == 0xff))
    return true;
  for (size_t i = 1; i < length; i += 2)
    if (bytes[i] != 0)
      return false;
  return true;
}

static glr_parse_table_t *
active_table (glr_parser_t *parser)
{
  return parser->parse_table != NULL ? parser->parse_table : parser->grammar->parse_table;
}

static int
initialize_table (glr_parser_t *parser)
{
  glr_parse_table_t *table = active_table (parser);
  uint64_t fingerprint = UINT64_C (14695981039346656037);
  if (!glr_grammar_validate (parser->grammar, NULL, 0))
    {
      parser->error = GLR_PARSE_ERROR_GRAMMAR;
      return -1;
    }
  fingerprint = (fingerprint ^ parser->grammar->symbol_count) * UINT64_C (1099511628211);
  fingerprint = (fingerprint ^ (uint32_t) parser->grammar->start_symbol->id)
                * UINT64_C (1099511628211);
  for (size_t i = 0; i < parser->grammar->production_count; i++)
    {
      const glr_production_t *production = parser->grammar->productions[i];
      fingerprint = (fingerprint ^ (uint32_t) production->head->id)
                    * UINT64_C (1099511628211);
      fingerprint = (fingerprint ^ production->body_length) * UINT64_C (1099511628211);
      for (size_t c = 0; c < production->body_length; c++)
        fingerprint = (fingerprint ^ (uint32_t) production->body[c]->id)
                      * UINT64_C (1099511628211);
    }
  if (parser->generated_parse_table && parser->grammar_fingerprint != fingerprint)
    {
      glr_parse_table_destroy (parser->parse_table);
      parser->parse_table = NULL;
      parser->owns_parse_table = parser->generated_parse_table = false;
      table = active_table (parser);
    }
  if (table == NULL)
    {
      table = glr_grammar_build_parse_table (parser->grammar, NULL, 0);
      if (table == NULL)
        {
          parser->error = GLR_PARSE_ERROR_GRAMMAR;
          return -1;
        }
      parser->parse_table = table;
      parser->owns_parse_table = true;
      parser->generated_parse_table = true;
      parser->grammar_fingerprint = fingerprint;
    }
  parser->state_table = (void **) table->states;
  parser->state_table_size = table->state_count;
  return 0;
}

glr_parser_t *
glr_parser_create (glr_grammar_t *grammar)
{
  glr_parser_t *parser;
  if (grammar == NULL)
    return NULL;
  parser = calloc (1, sizeof (*parser));
  if (parser == NULL)
    return NULL;
  parser->grammar = grammar;
  parser->forest = glr_forest_create ();
  parser->reader = glr_reader_create ();
  if (parser->forest == NULL || parser->reader == NULL)
    {
      glr_parser_destroy (parser);
      return NULL;
    }
  return parser;
}

static void
clear_stacks (glr_parser_t *parser)
{
  for (size_t i = 0; i < parser->stack_count; i++)
    glr_stack_destroy (parser->stacks[i]);
  free (parser->stacks);
  free (parser->stack_skip_reduce);
  parser->stacks = NULL;
  parser->stack_skip_reduce = NULL;
  parser->stack_count = parser->stack_capacity = 0;
}

void
glr_parser_destroy (glr_parser_t *parser)
{
  if (parser == NULL)
    return;
  clear_stacks (parser);
  glr_forest_destroy (parser->forest);
  if (parser->owns_parse_table)
    glr_parse_table_destroy (parser->parse_table);
  glr_reader_token_clear (&parser->lookahead);
  glr_reader_token_clear (&parser->last_token);
  glr_reader_destroy (parser->reader);
  glr_parser_clear_disambiguators (parser);
  free (parser->trivia);
  free (parser);
}

int
glr_parser_reset (glr_parser_t *parser)
{
  glr_forest_t *fresh;
  if (parser == NULL)
    return -1;
  fresh = glr_forest_create ();
  if (fresh == NULL)
    {
      parser->error = GLR_PARSE_ERROR_MEMORY;
      return -1;
    }
  clear_stacks (parser);
  glr_forest_destroy (parser->forest);
  parser->forest = fresh;
  glr_reader_token_clear (&parser->lookahead);
  glr_reader_token_clear (&parser->last_token);
  parser->input = NULL;
  parser->input_length = parser->input_pos = 0;
  parser->error = GLR_PARSE_SUCCESS;
  return 0;
}

static uint32_t
top_state (const glr_stack_t *stack)
{
  return glr_stack_node_get_state (stack->states[stack->height - 1]);
}

static int
push_entry (glr_stack_t *stack, uint32_t state, glr_forest_node_t *node)
{
  glr_stack_node_t *entry = glr_stack_node_create (state, 0);
  if (entry == NULL)
    return -1;
  glr_stack_node_set_forest_node (entry, node);
  if (glr_stack_push (stack, entry) != 0)
    {
      glr_stack_node_release (entry);
      return -1;
    }
  return 0;
}

/* The queue is a min-heap over CTL's vector; its ordering is by byte cursor,
   so variable-width lexical branches and reader tokens progress coherently. */
static void
agenda_push (vec_parse_task_ptr_t *agenda, parse_task_t *task)
{
  size_t child = agenda->size;
  vec_parse_task_ptr_t_push_back (agenda, task);
  while (child != 0)
    {
      size_t parent = (child - 1) / 2;
      if (agenda->value[parent]->position <= task->position)
        break;
      agenda->value[child] = agenda->value[parent];
      child = parent;
    }
  agenda->value[child] = task;
}

static parse_task_t *
agenda_pop (vec_parse_task_ptr_t *agenda)
{
  parse_task_t *task = agenda->value[0];
  parse_task_t *last = *vec_parse_task_ptr_t_back (agenda);
  size_t parent = 0;
  vec_parse_task_ptr_t_pop_back (agenda);
  while (parent < agenda->size / 2)
    {
      size_t child = parent * 2 + 1;
      if (child + 1 < agenda->size
          && agenda->value[child + 1]->position < agenda->value[child]->position)
        child++;
      if (last->position <= agenda->value[child]->position)
        break;
      agenda->value[parent] = agenda->value[child];
      parent = child;
    }
  if (agenda->size != 0)
    agenda->value[parent] = last;
  return task;
}

/* Takes ownership of the stack on every path, including duplicate rejection. */
static int
schedule (parse_run_t *run, glr_stack_t *stack, size_t position, int terminal,
            size_t length, size_t reductions)
{
  parse_task_t *task;
  parse_task_ptr_t *grown;
  uint64_t hash = position * UINT64_C (1099511628211) + (uint32_t) terminal;
  int inserted;
  if (stack == NULL)
    goto memory;
  task = malloc (sizeof (*task));
  if (task == NULL)
    {
      glr_stack_destroy (stack);
      goto memory;
    }
  task->stack = stack;
  task->position = position;
  task->terminal = terminal;
  task->length = length;
  task->reductions = reductions;
  hash = (hash ^ length) * UINT64_C (1099511628211);
  for (size_t i = 0; i < stack->height; i++)
    {
      glr_stack_node_t *entry = stack->states[i];
      hash = (hash ^ glr_stack_node_get_state (entry)) * UINT64_C (1099511628211);
      hash = (hash ^ (uintptr_t) glr_stack_node_get_forest_node (entry))
             * UINT64_C (1099511628211);
    }
  task->hash = (khint_t) (hash ^ (hash >> 32));
  if (kh_get (glr_configurations, run->seen, task) != kh_end (run->seen))
    {
      glr_stack_destroy (stack);
      free (task);
      return 0;
    }
  grown = GLR_VECTOR_RESERVE (&run->pending, run->pending.size + 1);
  if (grown == NULL)
    {
      glr_stack_destroy (stack);
      free (task);
      goto memory;
    }
  run->pending.value = grown;
  kh_put (glr_configurations, run->seen, task, &inserted);
  if (inserted < 0)
    {
      glr_stack_destroy (stack);
      free (task);
      goto memory;
    }
  agenda_push (&run->pending, task);
  return 0;
memory:
  run->parser->error = GLR_PARSE_ERROR_MEMORY;
  return -1;
}

static int
remember_accept (glr_parser_t *parser, const glr_stack_t *stack)
{
  glr_stack_t *copy;
  glr_forest_node_t *root = glr_stack_node_get_forest_node (stack->states[stack->height - 1]);
  if (root == NULL)
    return 0;
  for (size_t i = 0; i < parser->stack_count; i++)
    if (glr_stack_node_get_forest_node (
            parser->stacks[i]->states[parser->stacks[i]->height - 1]) == root)
      return 0;
  copy = glr_stack_fork ((glr_stack_t *) stack, stack->height);
  if (copy == NULL)
    return -1;
  if (parser->stack_count == parser->stack_capacity)
    {
      size_t capacity = parser->stack_capacity == 0 ? 4 : parser->stack_capacity * 2;
      glr_stack_t **stacks = malloc (capacity * sizeof (*stacks));
      bool *flags = calloc (capacity, sizeof (*flags));
      if (stacks == NULL || flags == NULL)
        {
          free (stacks);
          free (flags);
          glr_stack_destroy (copy);
          return -1;
        }
      if (parser->stack_count != 0)
        memcpy (stacks, parser->stacks, parser->stack_count * sizeof (*stacks));
      free (parser->stacks);
      free (parser->stack_skip_reduce);
      parser->stacks = stacks;
      parser->stack_skip_reduce = flags;
      parser->stack_capacity = capacity;
    }
  parser->stacks[parser->stack_count++] = copy;
  parser->forest->root = root;
  return 0;
}

static int
choose_action (glr_parser_t *parser, const parse_task_t *task,
                 const glr_action_set_t *actions)
{
  glr_disambig_context_t context = { 0 };
  glr_disambig_candidate_t *candidates;
  size_t winner = SIZE_MAX;
  int chosen = -1;
  if (parser->disambig_hooks == NULL || actions->action_count < 2)
    return -1;
  candidates = calloc (actions->action_count, sizeof (*candidates));
  if (candidates == NULL)
    {
      parser->error = GLR_PARSE_ERROR_MEMORY;
      return -2;
    }
  for (size_t i = 0; i < actions->action_count; i++)
    {
      const glr_action_t *action = &actions->actions[i];
      candidates[i].stack = task->stack;
      candidates[i].node = glr_stack_node_get_forest_node (
          task->stack->states[task->stack->height - 1]);
      candidates[i].production = action->type == GLR_ACTION_REDUCE
          ? glr_grammar_get_production (parser->grammar, (int) action->reduce.production_id) : NULL;
      candidates[i].start_position = task->position;
      candidates[i].end_position = task->position + task->length;
      candidates[i].probability = 1.0;
    }
  context.parser = parser;
  context.grammar = parser->grammar;
  context.forest = parser->forest;
  context.candidates = candidates;
  context.candidate_count = actions->action_count;
  context.lookahead_symbol_id = task->terminal;
  context.start_position = task->position;
  context.end_position = task->position + task->length;
  context.user_data = parser->user_data;
  if (glr_parser_run_disambiguators (parser, &context, &winner) == GLR_DISAMBIG_RESOLVED
      && winner < actions->action_count && glr_disambig_context_active_count (&context) == 1)
    chosen = (int) winner;
  free (candidates);
  return chosen;
}

static int
reduce_action (parse_run_t *run, const parse_task_t *task, const glr_action_t *action)
{
  glr_parser_t *parser = run->parser;
  glr_production_t *production = glr_grammar_get_production (
      parser->grammar, (int) action->reduce.production_id);
  glr_forest_node_t **children = NULL;
  glr_forest_node_t *constructor;
  glr_forest_node_t *symbol;
  glr_stack_t *stack;
  uint32_t next_state;
  size_t start = task->position;
  size_t end = task->position;
  size_t count;
  size_t below;
  if (production == NULL)
    {
      parser->error = GLR_PARSE_ERROR_GRAMMAR;
      return -1;
    }
  count = production->body_length;
  if (count >= task->stack->height)
    return 0;
  below = task->stack->height - count - 1;
  if (glr_parse_table_get_goto (run->table,
        glr_stack_node_get_state (task->stack->states[below]),
        (uint32_t) production->head->id, &next_state) != 0)
    return 0;
  if (task->reductions >= GLR_MAX_REDUCE_CHAIN)
    {
      parser->error = GLR_PARSE_ERROR_GRAMMAR;
      return -1;
    }
  if (count != 0)
    {
      children = malloc (count * sizeof (*children));
      if (children == NULL)
        goto memory;
      for (size_t i = 0; i < count; i++)
        {
          children[i] = glr_stack_node_get_forest_node (task->stack->states[below + i + 1]);
          if (children[i] == NULL || children[i]->symbol_id != production->body[i]->id)
            {
              free (children);
              return 0;
            }
        }
      start = children[0]->position;
      end = children[count - 1]->end_position;
    }
  constructor = glr_forest_pack_production (parser->forest, production->id,
                                           start, end, children, count);
  free (children);
  if (constructor == NULL)
    goto memory;
  symbol = glr_forest_get_symbol (parser->forest, production->head->id, start, end);
  if (symbol == NULL)
    goto memory;
  {
    size_t i;
    for (i = 0; i < symbol->child_count; i++)
      if (symbol->children[i] == constructor)
        break;
    if (i == symbol->child_count && glr_forest_add_child (symbol, constructor) != 0)
      goto memory;
  }
  stack = glr_stack_fork (task->stack, task->stack->height - count);
  if (stack == NULL)
    goto memory;
  if (push_entry (stack, next_state, symbol) != 0)
    {
      glr_stack_destroy (stack);
      goto memory;
    }
  return schedule (run, stack, task->position, task->terminal, task->length,
                   task->reductions + 1);
memory:
  parser->error = GLR_PARSE_ERROR_MEMORY;
  return -1;
}

static int
shift_action (parse_run_t *run, const parse_task_t *task, const glr_action_t *action)
{
  glr_stack_t *stack;
  glr_forest_node_t *terminal;
  if (task->terminal < 0 || task->length == 0)
    return 0;
  terminal = glr_forest_get_terminal (run->parser->forest, task->terminal,
                                     task->position, task->position + task->length);
  if (terminal == NULL)
    goto memory;
  stack = glr_stack_fork (task->stack, task->stack->height);
  if (stack == NULL)
    goto memory;
  if (push_entry (stack, action->shift.next_state, terminal) != 0)
    {
      glr_stack_destroy (stack);
      goto memory;
    }
  if (run->reader)
    {
      glr_parser_t *parser = run->parser;
      char *name = copy_text (parser->lookahead.terminal_name);
      if (name == NULL)
        {
          glr_stack_destroy (stack);
          goto memory;
        }
      glr_reader_token_clear (&parser->last_token);
      parser->last_token = parser->lookahead;
      parser->last_token.terminal_name = name;
    }
  return schedule (run, stack, task->position + task->length,
                   GLR_SCAN_LOOKAHEAD, 0, 0);
memory:
  run->parser->error = GLR_PARSE_ERROR_MEMORY;
  return -1;
}

static size_t
skip_trivia (const glr_parser_t *parser, size_t position)
{
  if (parser->trivia != NULL)
    {
      size_t length = strlen (parser->trivia);
      while (length <= parser->input_length - position
             && memcmp (parser->input + position, parser->trivia, length) == 0)
        position += length;
    }
  return position;
}

static int
obtain_matches (parse_run_t *run, size_t position)
{
  glr_parser_t *parser = run->parser;
  if (run->cached_position == position)
    return 0;
  free (run->matches);
  run->matches = NULL;
  run->match_count = 0;
  run->cached_position = position;
  if (run->reader)
    {
      glr_reader_status_t status = glr_reader_next (parser->reader, &parser->lookahead);
      int id;
      if (status == GLR_READER_STATUS_EOF)
        return 0;
      if (status != GLR_READER_STATUS_OK)
        {
          parser->error = status == GLR_READER_STATUS_NO_MEMORY
                              ? GLR_PARSE_ERROR_MEMORY : GLR_PARSE_ERROR_SYNTAX;
          return -1;
        }
      id = glr_grammar_find_symbol (parser->grammar, parser->lookahead.terminal_name,
                                    GLR_SYMBOL_TERMINAL);
      if (id < 0)
        {
          run->furthest = parser->lookahead.byte_offset + parser->lookahead.bytes_consumed;
          parser->error = GLR_PARSE_ERROR_SYNTAX;
          return -1;
        }
      run->matches = malloc (sizeof (*run->matches));
      if (run->matches == NULL)
        goto memory;
      run->matches[0].symbol_id = id;
      run->matches[0].position = parser->lookahead.byte_offset;
      run->matches[0].length = parser->lookahead.bytes_consumed;
      run->match_count = 1;
      return 0;
    }
  if (glr_scannerless_scan (parser->grammar, parser->input, parser->input_length,
                            position, &run->matches, &run->match_count) != 0)
    goto memory;
  if (!run->scannerless && run->match_count > 1)
    {
      size_t longest = 0;
      for (size_t i = 1; i < run->match_count; i++)
        if (run->matches[i].length > run->matches[longest].length)
          longest = i;
      run->matches[0] = run->matches[longest];
      run->match_count = 1;
    }
  return 0;
memory:
  parser->error = GLR_PARSE_ERROR_MEMORY;
  return -1;
}

static int
scan_task (parse_run_t *run, const parse_task_t *task)
{
  glr_parser_t *parser = run->parser;
  size_t position = run->reader ? task->position : skip_trivia (parser, task->position);
  if (parser->snapshot_hook != NULL && task->position != 0)
    parser->snapshot_hook (task->stack, task->position, parser->snapshot_data);
  if (position > run->furthest)
    run->furthest = position;
  if (obtain_matches (run, position) != 0)
    return -1;
  if ((run->reader && run->match_count == 0) || position == parser->input_length)
    return schedule (run, glr_stack_fork (task->stack, task->stack->height),
                     parser->input_length, -1, 0, 0);
  for (size_t i = 0; i < run->match_count; i++)
    if (schedule (run, glr_stack_fork (task->stack, task->stack->height),
                   run->matches[i].position, run->matches[i].symbol_id,
                   run->matches[i].length, 0) != 0)
      return -1;
  return 0;
}

static int
run_parser (glr_parser_t *parser, const glr_stack_t *seed, size_t position)
{
  parse_run_t run = { 0 };
  glr_stack_t *initial = NULL;
  int rc = -1;
  if (initialize_table (parser) != 0)
    return -1;
  run.parser = parser;
  run.table = active_table (parser);
  run.scannerless = parser->scannerless || glr_scannerless_has_patterns (parser->grammar);
  run.reader = !run.scannerless && is_utf16 (parser->input, parser->input_length);
  run.cached_position = SIZE_MAX;
  run.furthest = position;
  run.pending = vec_parse_task_ptr_t_init ();
  run.completed = vec_parse_task_ptr_t_init ();
  run.seen = kh_init (glr_configurations);
  if (run.seen == NULL)
    goto memory;
  if (run.reader)
    {
      glr_reader_set_encoding (parser->reader, GLR_READER_ENCODING_UTF16_AUTO);
      if (glr_reader_set_lexer_hooks (parser->reader, parser->lexer_hooks) != 0
          || glr_reader_set_input (parser->reader, parser->input, parser->input_length) != 0)
        goto memory;
    }
  initial = seed != NULL ? glr_stack_copy (seed) : glr_stack_create ();
  if (initial == NULL)
    goto memory;
  glr_stack_set_gss_entries (initial, true);
  if (seed == NULL && push_entry (initial, 0, NULL) != 0)
    {
      glr_stack_destroy (initial);
      goto memory;
    }
  if (schedule (&run, initial, position, GLR_SCAN_LOOKAHEAD, 0, 0) != 0)
    goto cleanup;
  while (run.pending.size != 0)
    {
      parse_task_t *task = agenda_pop (&run.pending);
      parse_task_ptr_t *grown;
      const glr_action_set_t *actions;
      uint32_t column;
      int chosen;
      /* No action can move the cursor backwards. Once an offset is closed,
         release its immutable configurations and hash keys; future stacks
         retain shared GSS entries through their atomic reference counts. */
      if (run.completed.size != 0 && task->position != run.completed_position)
        {
          for (size_t i = 0; i < run.completed.size; i++)
            {
              parse_task_t *old = run.completed.value[i];
              khint_t slot = kh_get (glr_configurations, run.seen, old);
              kh_del (glr_configurations, run.seen, slot);
              glr_stack_destroy (old->stack);
              free (old);
            }
          vec_parse_task_ptr_t_clear (&run.completed);
        }
      run.completed_position = task->position;
      grown = GLR_VECTOR_RESERVE (&run.completed, run.completed.size + 1);
      if (grown == NULL)
        goto memory;
      run.completed.value = grown;
      vec_parse_task_ptr_t_push_back (&run.completed, task);
      parser->input_pos = task->position;
      if (task->position > run.furthest)
        run.furthest = task->position;
      if (task->terminal == GLR_SCAN_LOOKAHEAD)
        {
          if (scan_task (&run, task) != 0)
            goto cleanup;
          continue;
        }
      column = task->terminal >= 0 ? (uint32_t) task->terminal
                                   : glr_parse_table_eof_column (run.table);
      actions = glr_parse_table_get_actions (run.table, top_state (task->stack), column);
      if (actions == NULL)
        continue;
      chosen = choose_action (parser, task, actions);
      if (chosen == -2)
        goto cleanup;
      for (size_t i = 0; i < actions->action_count; i++)
        {
          const glr_action_t *action = &actions->actions[i];
          if (chosen >= 0 && i != (size_t) chosen)
            continue;
          if (action->type == GLR_ACTION_REDUCE)
            {
              if (reduce_action (&run, task, action) != 0)
                goto cleanup;
            }
          else if (action->type == GLR_ACTION_SHIFT)
            {
              if (shift_action (&run, task, action) != 0)
                goto cleanup;
            }
          else if (action->type == GLR_ACTION_ACCEPT && task->terminal == -1)
            {
              if (remember_accept (parser, task->stack) != 0)
                goto memory;
            }
        }
    }
  if (parser->stack_count == 0)
    {
      parser->error = GLR_PARSE_ERROR_SYNTAX;
      goto cleanup;
    }
  rc = 0;
  goto cleanup;
memory:
  parser->error = GLR_PARSE_ERROR_MEMORY;
cleanup:
  parser->input_pos = rc == 0 ? parser->input_length : run.furthest;
  if (run.seen != NULL)
    for (khint_t i = kh_begin (run.seen); i != kh_end (run.seen); i++)
      if (kh_exist (run.seen, i))
        {
          parse_task_t *task = kh_key (run.seen, i);
          glr_stack_destroy (task->stack);
          free (task);
        }
  kh_destroy (glr_configurations, run.seen);
  vec_parse_task_ptr_t_free (&run.pending);
  vec_parse_task_ptr_t_free (&run.completed);
  free (run.matches);
  return rc;
}

static glr_parse_result_t
finish_result (glr_parser_t *parser, int status)
{
  glr_parse_result_t result = { 0 };
  result.position = parser->input_pos;
  result.user_data = parser->user_data;
  result.error = status == 0 ? GLR_PARSE_SUCCESS : parser->error;
  if (status == 0 && glr_grammar_has_semantic_actions (parser->grammar))
    {
      result.semantic_error = glr_semantic_evaluate (parser->grammar, parser->forest,
          parser->input, parser->input_length, parser->semantic_resolver,
          parser->user_data, &result.semantic_value);
      if (result.semantic_error != GLR_SEMANTIC_SUCCESS)
        result.error = parser->error = GLR_PARSE_ERROR_SEMANTIC;
    }
  if (result.error == GLR_PARSE_SUCCESS)
    result.forest = parser->forest;
  return result;
}

glr_parse_result_t
glr_parse (glr_parser_t *parser, const char *input, size_t length)
{
  glr_parse_result_t result = { 0 };
  if (parser == NULL || input == NULL || glr_parser_reset (parser) != 0)
    {
      result.error = GLR_PARSE_ERROR_MEMORY;
      return result;
    }
  parser->input = input;
  parser->input_length = length;
  return finish_result (parser, run_parser (parser, NULL, 0));
}

int
glr_parser_parse_from (glr_parser_t *parser, const glr_stack_t *stack,
                        glr_forest_t *forest, size_t position, const char *input,
                        size_t length, glr_parse_result_t *out_result)
{
  glr_parse_result_t result = { 0 };
  glr_forest_t *owned;
  glr_stack_t *seed;
  if (parser == NULL || stack == NULL || forest == NULL || input == NULL
      || !stack->owns_gss_entries || stack->height == 0 || forest == parser->forest)
    {
      result.error = GLR_PARSE_ERROR_MEMORY;
      goto done;
    }
  result.user_data = parser->user_data;
  if (position > length)
    {
      result.error = GLR_PARSE_ERROR_SYNTAX;
      goto done;
    }
  if (!parser->scannerless && !glr_scannerless_has_patterns (parser->grammar)
      && is_utf16 (input, length))
    {
      result.error = GLR_PARSE_ERROR_GRAMMAR;
      goto done;
    }
  seed = glr_stack_copy (stack);
  if (seed == NULL)
    {
      result.error = GLR_PARSE_ERROR_MEMORY;
      goto done;
    }
  if (glr_parser_reset (parser) != 0)
    {
      glr_stack_destroy (seed);
      result.error = GLR_PARSE_ERROR_MEMORY;
      goto done;
    }
  owned = parser->forest;
  parser->forest = forest;
  forest->root = NULL;
  parser->input = input;
  parser->input_length = length;
  result = finish_result (parser, run_parser (parser, seed, position));
  parser->forest = owned;
  glr_stack_destroy (seed);
done:
  if (out_result != NULL)
    *out_result = result;
  return result.error == GLR_PARSE_SUCCESS ? 0 : -1;
}

glr_forest_t *
glr_parser_take_forest (glr_parser_t *parser)
{
  glr_forest_t *fresh;
  glr_forest_t *forest;
  if (parser == NULL)
    return NULL;
  fresh = glr_forest_create ();
  if (fresh == NULL)
    return NULL;
  forest = parser->forest;
  parser->forest = fresh;
  return forest;
}

int
glr_parser_set_snapshot_hook (glr_parser_t *parser, glr_parser_snapshot_fn hook,
                               void *user_data)
{
  if (parser == NULL)
    return -1;
  parser->snapshot_hook = hook;
  parser->snapshot_data = user_data;
  return 0;
}

int
glr_parser_set_lexer_hooks (glr_parser_t *parser, glr_lexer_hooks_t *hooks)
{
  if (parser == NULL)
    return -1;
  parser->lexer_hooks = hooks;
  return glr_reader_set_lexer_hooks (parser->reader, hooks);
}

glr_lexer_hooks_t *
glr_parser_get_lexer_hooks (const glr_parser_t *parser)
{
  return parser != NULL ? parser->lexer_hooks : NULL;
}

int
glr_parser_set_parse_table (glr_parser_t *parser, glr_parse_table_t *table, bool own)
{
  if (parser == NULL)
    return -1;
  if (parser->owns_parse_table && parser->parse_table != table)
    glr_parse_table_destroy (parser->parse_table);
  parser->parse_table = table;
  parser->owns_parse_table = table != NULL && own;
  parser->generated_parse_table = false;
  return 0;
}

glr_parse_table_t *
glr_parser_get_parse_table (const glr_parser_t *parser)
{
  return parser != NULL ? parser->parse_table : NULL;
}

int
glr_parser_set_trivia (glr_parser_t *parser, const char *trivia)
{
  char *copy;
  if (parser == NULL || (trivia != NULL && *trivia == '\0'))
    return -1;
  copy = trivia != NULL ? copy_text (trivia) : NULL;
  if (trivia != NULL && copy == NULL)
    return -1;
  free (parser->trivia);
  parser->trivia = copy;
  return 0;
}

const char *
glr_parser_get_trivia (const glr_parser_t *parser)
{
  return parser != NULL ? parser->trivia : NULL;
}

const glr_reader_token_t *
glr_parser_get_last_token (const glr_parser_t *parser)
{
  return parser != NULL && parser->last_token.terminal_name != NULL
             ? &parser->last_token : NULL;
}

int
glr_parser_set_scannerless (glr_parser_t *parser, bool enabled)
{
  if (parser == NULL)
    return -1;
  parser->scannerless = enabled;
  return 0;
}

int
glr_parser_set_semantic_resolver (glr_parser_t *parser, glr_semantic_resolver_fn resolver)
{
  if (parser == NULL)
    return -1;
  parser->semantic_resolver = resolver;
  return 0;
}

const char *glr_version (void) { return "1.0.0"; }
const char *glr_name (void) { return "LibGLR"; }
