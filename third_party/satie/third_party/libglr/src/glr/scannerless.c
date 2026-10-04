#include <glr/scannerless.h>
#include "containers.h"
#include "grammar-internal.h"

#include <klib/kstring.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>

struct glr_terminal_pattern
{
  bool literal;
  char *text;
  size_t length;
  regex_t prefix;
  regex_t complete;
};

typedef glr_terminal_match_t scan_match_t;
#define T scan_match_t
#define P
#include <ctl/vec.h>

void
glr_terminal_pattern_destroy (glr_terminal_pattern_t *pattern)
{
  if (pattern == NULL)
    return;
  if (!pattern->literal)
    {
      regfree (&pattern->prefix);
      regfree (&pattern->complete);
    }
  free (pattern->text);
  free (pattern);
}

static void
pattern_error (char *error, size_t size, const char *message)
{
  if (error != NULL && size != 0)
    snprintf (error, size, "%s", message);
}

int
glr_scannerless_set_pattern (glr_grammar_t *grammar, int terminal_id,
                             const char *expression, char *error,
                             size_t error_size)
{
  glr_symbol_t *symbol = glr_grammar_get_symbol (grammar, terminal_id);
  glr_terminal_pattern_t *pattern;
  kstring_t source = { 0, 0, NULL };
  int rc;
  pattern_error (error, error_size, "");
  if (!glr_symbol_is_terminal (symbol) || expression == NULL
      || strlen (expression) > INT_MAX - 8)
    {
      pattern_error (error, error_size, "invalid terminal or expression");
      return -1;
    }
  pattern = calloc (1, sizeof (*pattern));
  if (pattern == NULL)
    goto memory;
  if (kputs ("^(", &source) < 0 || kputs (expression, &source) < 0
      || kputc (')', &source) < 0)
    goto free_pattern;
  rc = regcomp (&pattern->prefix, source.s, REG_EXTENDED);
  if (rc != 0)
    {
      if (error != NULL && error_size != 0)
        regerror (rc, &pattern->prefix, error, error_size);
      free (source.s);
      free (pattern);
      return -1;
    }
  if (kputc ('$', &source) < 0)
    {
      regfree (&pattern->prefix);
      goto free_pattern;
    }
  rc = regcomp (&pattern->complete, source.s, REG_EXTENDED);
  free (source.s);
  source.s = NULL;
  if (rc != 0)
    {
      if (error != NULL && error_size != 0)
        regerror (rc, &pattern->complete, error, error_size);
      regfree (&pattern->prefix);
      free (pattern);
      return -1;
    }
  if (regexec (&pattern->complete, "", 0, NULL, 0) == 0)
    {
      pattern_error (error, error_size, "terminal pattern accepts an empty string");
      glr_terminal_pattern_destroy (pattern);
      return -1;
    }
  pattern->text = malloc (strlen (expression) + 1);
  if (pattern->text == NULL)
    {
      glr_terminal_pattern_destroy (pattern);
      goto memory;
    }
  strcpy (pattern->text, expression);
  glr_terminal_pattern_destroy (symbol->pattern);
  symbol->pattern = pattern;
  return 0;
free_pattern:
  free (source.s);
  free (pattern);
memory:
  pattern_error (error, error_size, "out of memory compiling terminal pattern");
  return -1;
}

int
glr_scannerless_set_literal (glr_grammar_t *grammar, int terminal_id,
                             const void *literal, size_t length)
{
  glr_symbol_t *symbol = glr_grammar_get_symbol (grammar, terminal_id);
  glr_terminal_pattern_t *pattern;
  if (!glr_symbol_is_terminal (symbol) || literal == NULL || length == 0
      || length == SIZE_MAX)
    return -1;
  pattern = calloc (1, sizeof (*pattern));
  if (pattern == NULL)
    return -1;
  pattern->literal = true;
  pattern->length = length;
  pattern->text = malloc (length + 1);
  if (pattern->text == NULL)
    {
      free (pattern);
      return -1;
    }
  memcpy (pattern->text, literal, length);
  pattern->text[length] = '\0';
  glr_terminal_pattern_destroy (symbol->pattern);
  symbol->pattern = pattern;
  return 0;
}

int
glr_scannerless_clear_pattern (glr_grammar_t *grammar, int terminal_id)
{
  glr_symbol_t *symbol = glr_grammar_get_symbol (grammar, terminal_id);
  if (!glr_symbol_is_terminal (symbol))
    return -1;
  glr_terminal_pattern_destroy (symbol->pattern);
  symbol->pattern = NULL;
  return 0;
}

bool
glr_scannerless_has_patterns (const glr_grammar_t *grammar)
{
  if (grammar == NULL)
    return false;
  for (size_t i = 0; i < grammar->symbol_count; i++)
    if (grammar->symbols[i] != NULL && grammar->symbols[i]->pattern != NULL)
      return true;
  return false;
}

static int
terminal_match (const glr_symbol_t *terminal, const char *input, size_t length,
                 kstring_t *text, size_t *matched)
{
  const glr_terminal_pattern_t *pattern = terminal->pattern;
  regmatch_t match;
  int rc;
  *matched = 0;
  if (pattern == NULL || pattern->literal)
    {
      const char *literal = pattern != NULL ? pattern->text : terminal->name;
      size_t n = pattern != NULL ? pattern->length
                                 : (literal != NULL ? strlen (literal) : 0);
      if (n > 0 && n <= length && memcmp (input, literal, n) == 0)
        *matched = n;
      return *matched != 0;
    }
  /* POSIX regex operates on text. Bound the temporary string by the provided
     byte count, and never read the caller's buffer as a C string. */
  {
    const char *nul = memchr (input, '\0', length);
    if (nul != NULL)
      length = (size_t) (nul - input);
  }
  if (length > INT_MAX)
    return -1;
  text->l = 0;
  if (kputsn (input, (int) length, text) < 0)
    return -1;
  rc = regexec (&pattern->prefix, text->s, 1, &match, 0);
  if (rc == REG_NOMATCH)
    return 0;
  if (rc != 0)
    return -1;
  if (match.rm_so == 0 && match.rm_eo > 0)
    *matched = (size_t) match.rm_eo;
  return *matched != 0;
}

int
glr_scannerless_match (const glr_symbol_t *terminal, const char *input,
                       size_t length, size_t *matched_length)
{
  kstring_t text = { 0, 0, NULL };
  int rc;
  if (matched_length != NULL)
    *matched_length = 0;
  if (!glr_symbol_is_terminal ((glr_symbol_t *) terminal) || input == NULL
      || matched_length == NULL)
    return -1;
  rc = terminal_match (terminal, input, length, &text, matched_length);
  free (text.s);
  return rc;
}

int
glr_scannerless_scan (const glr_grammar_t *grammar, const char *input,
                      size_t length, size_t position,
                      glr_terminal_match_t **matches, size_t *match_count)
{
  vec_scan_match_t found = vec_scan_match_t_init ();
  kstring_t text = { 0, 0, NULL };
  if (matches != NULL)
    *matches = NULL;
  if (match_count != NULL)
    *match_count = 0;
  if (grammar == NULL || input == NULL || matches == NULL || match_count == NULL
      || position > length)
    return -1;
  for (size_t i = 0; i < grammar->symbol_count; i++)
    {
      const glr_symbol_t *symbol = grammar->symbols[i];
      size_t longest;
      bool regex;
      if (!glr_symbol_is_terminal ((glr_symbol_t *) symbol))
        continue;
      if (terminal_match (symbol, input + position, length - position,
                           &text, &longest) < 0)
        goto fail;
      if (longest == 0)
        continue;
      regex = symbol->pattern != NULL && !symbol->pattern->literal;
      for (size_t n = regex ? 1 : longest; n <= longest; n++)
        {
          scan_match_t match = { symbol->id, position, n };
          scan_match_t *grown;
          if (regex)
            {
              char saved = text.s[n];
              int rc;
              text.s[n] = '\0';
              rc = regexec (&symbol->pattern->complete, text.s, 0, NULL, 0);
              text.s[n] = saved;
              if (rc == REG_NOMATCH)
                continue;
              if (rc != 0)
                goto fail;
            }
          grown = GLR_VECTOR_RESERVE (&found, found.size + 1);
          if (grown == NULL)
            goto fail;
          found.value = grown;
          vec_scan_match_t_push_back (&found, match);
        }
    }
  free (text.s);
  *matches = found.value;
  *match_count = found.size;
  return 0;
fail:
  free (text.s);
  vec_scan_match_t_free (&found);
  return -1;
}
