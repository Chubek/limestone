#ifndef GLR_LEXER_HOOKS_H
#define GLR_LEXER_HOOKS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @file lexer-hooks.h
   * @brief Pluggable lexer hook infrastructure for UTF-16 readers.
   */

  typedef struct glr_lexer_hooks glr_lexer_hooks_t;

  /**
   * @brief Information about the terminal-sized unit currently being read.
   */
  typedef struct
  {
    uint32_t codepoint;            /**< Decoded Unicode scalar value. */
    const char *unicode_name;      /**< Unicode name from the bundled table. */
    const unsigned char *input;    /**< Full input buffer. */
    size_t input_length;           /**< Input buffer size in bytes. */
    size_t byte_offset;            /**< Byte offset of the current codepoint. */
    size_t default_bytes_consumed; /**< Bytes for the decoded UTF-16 unit. */
    void *user_data;               /**< Per-registry user data. */
  } glr_lexer_event_t;

  /**
   * @brief A lexer hook response.
   *
   * Hooks can decline to respond by leaving @ref accepted false. When accepted,
   * the reader copies @ref terminal_name and advances by @ref bytes_consumed.
   */
  typedef struct
  {
    const char *terminal_name; /**< Replacement terminal name. */
    size_t bytes_consumed;     /**< Input bytes consumed by the custom token. */
    bool accepted;             /**< Whether the hook handled the event. */
  } glr_lexer_response_t;

  /**
   * @brief Hook callback for terminal discovery.
   *
   * Return true when the hook produced a meaningful response, false to fall
   * back to the next hook or the Unicode-name terminal.
   */
  typedef bool (*glr_lexer_hook_fn) (const glr_lexer_event_t *event,
                                     glr_lexer_response_t *response,
                                     void *hook_user_data);

  /**
   * @brief Destroy callback for hook user data.
   */
  typedef void (*glr_lexer_hook_destroy_fn) (void *hook_user_data);

  glr_lexer_hooks_t *glr_lexer_hooks_create (void);
  void glr_lexer_hooks_destroy (glr_lexer_hooks_t *hooks);
  void glr_lexer_hooks_clear (glr_lexer_hooks_t *hooks);

  int glr_lexer_hooks_add (glr_lexer_hooks_t *hooks, const char *name,
                           int priority, glr_lexer_hook_fn fn,
                           void *hook_user_data,
                           glr_lexer_hook_destroy_fn destroy_fn);

  void glr_lexer_hooks_set_user_data (glr_lexer_hooks_t *hooks, void *user_data);
  void *glr_lexer_hooks_get_user_data (glr_lexer_hooks_t *hooks);

  /**
   * @brief Count hooks in a registry (null-safe).
   *
   * @param hooks Registry (may be NULL)
   * @return Number of registered hooks
   */
  size_t glr_lexer_hooks_count (const glr_lexer_hooks_t *hooks);

  /**
   * @brief Remove one hook by name and destroy it.
   *
   * @param hooks Registry (may be NULL)
   * @param name Hook name (NULL matches anonymous hooks only)
   * @return 0 when removed, -1 when not found
   */
  int glr_lexer_hooks_remove (glr_lexer_hooks_t *hooks, const char *name);

  /**
   * @brief Dispatch one lexer event through the hook chain.
   *
   * Hooks run in priority order; the first hook that accepts the event
   * (returns true with an accepted response consuming at least the
   * default byte count) wins. The response is reset before dispatch
   * and on total decline.
   *
   * @param hooks Registry (may be NULL, declines everything)
   * @param event Event to dispatch (may be NULL, declines)
   * @param response Output response (must be non-NULL)
   * @return true when a hook accepted the event
   */
  bool glr_lexer_hooks_dispatch (glr_lexer_hooks_t *hooks,
                                 const glr_lexer_event_t *event,
                                 glr_lexer_response_t *response);

  const char *glr_lexer_unicode_name (uint32_t codepoint);

  static inline void
  glr_lexer_response_reset (glr_lexer_response_t *response)
  {
    if (response != NULL)
      {
        response->terminal_name = NULL;
        response->bytes_consumed = 0;
        response->accepted = false;
      }
  }

  static inline void
  glr_lexer_response_accept (glr_lexer_response_t *response,
                             const char *terminal_name,
                             size_t bytes_consumed)
  {
    if (response != NULL)
      {
        response->terminal_name = terminal_name;
        response->bytes_consumed = bytes_consumed;
        response->accepted = true;
      }
  }

#ifdef __cplusplus
}
#endif

#endif /* GLR_LEXER_HOOKS_H */
