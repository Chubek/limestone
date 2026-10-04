#ifndef GLR_SCANNERLESS_H
#define GLR_SCANNERLESS_H

#include <glr/grammar.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct glr_terminal_pattern glr_terminal_pattern_t;
typedef struct
{
  int symbol_id;
  size_t position;
  size_t length;
} glr_terminal_match_t;

/** Attach a POSIX extended regular expression to a terminal. Matching is
    anchored at the cursor, and every non-empty matching prefix is retained.
    Expressions accepting the empty string are rejected. Regex input is text
    (no embedded NUL); literals support arbitrary bytes. Replacement is atomic
    on failure. Patterns are owned by the grammar. */
int glr_scannerless_set_pattern (glr_grammar_t *grammar, int terminal_id,
                                 const char *expression, char *error,
                                 size_t error_size);
int glr_scannerless_set_literal (glr_grammar_t *grammar, int terminal_id,
                                 const void *literal, size_t length);
int glr_scannerless_clear_pattern (glr_grammar_t *grammar, int terminal_id);
bool glr_scannerless_has_patterns (const glr_grammar_t *grammar);

/** Return all matches at one byte offset, ordered by symbol id then length.
    Unconfigured terminals match their names literally. The caller frees the
    returned array with free(). On failure, outputs are NULL and zero. */
int glr_scannerless_scan (const glr_grammar_t *grammar, const char *input,
                          size_t length, size_t position,
                          glr_terminal_match_t **matches, size_t *match_count);

/** Longest matching prefix of one terminal: 1 for a match, 0 for no match,
    -1 for invalid input or allocation failure. */
int glr_scannerless_match (const glr_symbol_t *terminal, const char *input,
                           size_t length, size_t *matched_length);

#ifdef __cplusplus
}
#endif
#endif
