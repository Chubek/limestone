#ifndef GLR_READER_H
#define GLR_READER_H

#include <glr/lexer-hooks.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @file reader.h
   * @brief UTF-16 token reader used by the parser front end.
   *
   * The reader owns the cursor over an immutable input buffer, decodes UTF-16
   * code units, and turns them into terminal tokens. Applications may attach
   * @ref glr_lexer_hooks_t to recognize multi-codepoint tokens, normalize
   * Unicode categories into grammar terminals, or override the default terminal
   * name that would otherwise come from @ref glr_lexer_unicode_name.
   *
   * @see lexer-hooks.h
   * @see parser.h
   */

  /**
   * @typedef glr_reader_t
   * @brief Opaque reader instance that stores input, encoding, hooks, and cursor state.
   */
  typedef struct glr_reader glr_reader_t;

  /**
   * @typedef glr_reader_encoding_t
   * @brief UTF-16 byte-order mode used when decoding reader input.
   */
  typedef enum
  {
    GLR_READER_ENCODING_UTF16_LE,   /**< Interpret input as little-endian UTF-16. */
    GLR_READER_ENCODING_UTF16_BE,   /**< Interpret input as big-endian UTF-16. */
    GLR_READER_ENCODING_UTF16_AUTO  /**< Detect byte order from a BOM, then fall back to host defaults. */
  } glr_reader_encoding_t;

  /**
   * @typedef glr_reader_status_t
   * @brief Result code returned by tokenization operations.
   */
  typedef enum
  {
    GLR_READER_STATUS_OK,               /**< A token was produced successfully. */
    GLR_READER_STATUS_EOF,              /**< The cursor reached the end of input. */
    GLR_READER_STATUS_INVALID_ARGUMENT, /**< A required pointer or length was invalid. */
    GLR_READER_STATUS_INVALID_ENCODING, /**< The selected encoding cannot decode the buffer. */
    GLR_READER_STATUS_INVALID_SEQUENCE, /**< The input contains an invalid UTF-16 sequence. */
    GLR_READER_STATUS_NO_MEMORY         /**< Allocation failed while producing a token. */
  } glr_reader_status_t;

  /**
   * @struct glr_reader_token_t
   * @brief Token emitted by @ref glr_reader_next.
   *
   * The token records both the grammar terminal name and enough source-location
   * information for callers to map parser actions back to bytes in the input.
   * If @ref from_hook is true, @ref terminal_name came from a lexer hook and may
   * represent more than one UTF-16 code unit.
   */
  typedef struct
  {
    char *terminal_name;     /**< Heap-owned terminal name; clear with @ref glr_reader_token_clear. */
    uint32_t codepoint;      /**< First Unicode scalar value represented by the token. */
    size_t byte_offset;      /**< Byte offset of the token in the original input buffer. */
    size_t bytes_consumed;   /**< Number of bytes consumed to create the token. */
    bool from_hook;          /**< True when a lexer hook accepted the token event. */
  } glr_reader_token_t;

  /**
   * @brief Allocate a reader with default auto-detected UTF-16 encoding.
   * @return New reader, or NULL if allocation fails.
   */
  glr_reader_t *glr_reader_create (void);

  /**
   * @brief Destroy a reader and release any internal cursor resources.
   * @param reader Reader returned by @ref glr_reader_create, or NULL.
   */
  void glr_reader_destroy (glr_reader_t *reader);

  /**
   * @brief Attach an immutable input buffer and reset the cursor to its start.
   * @param reader Reader to configure.
   * @param input UTF-16 byte buffer; ownership remains with the caller.
   * @param input_length Size of @p input in bytes.
   * @return 0 on success, -1 on invalid arguments or allocation failure.
   */
  int glr_reader_set_input (glr_reader_t *reader, const void *input,
                            size_t input_length);

  /**
   * @brief Rewind the reader cursor without changing input, encoding, or hooks.
   * @param reader Reader to rewind; NULL is ignored.
   */
  void glr_reader_reset (glr_reader_t *reader);

  /**
   * @brief Set the byte-order policy used for subsequent decoding.
   * @param reader Reader to configure.
   * @param encoding Encoding policy to use.
   */
  void glr_reader_set_encoding (glr_reader_t *reader,
                                glr_reader_encoding_t encoding);

  /**
   * @brief Return the current byte-order policy.
   * @param reader Reader to query.
   * @return Configured encoding, or @ref GLR_READER_ENCODING_UTF16_AUTO for NULL.
   */
  glr_reader_encoding_t glr_reader_get_encoding (const glr_reader_t *reader);

  /**
   * @brief Attach a lexer hook registry to the reader.
   * @param reader Reader to configure.
   * @param hooks Hook registry; ownership remains with the caller.
   * @return 0 on success, -1 on invalid arguments.
   * @see glr_lexer_hooks_add
   */
  int glr_reader_set_lexer_hooks (glr_reader_t *reader, glr_lexer_hooks_t *hooks);

  /**
   * @brief Return the hook registry currently attached to the reader.
   * @param reader Reader to query.
   * @return Hook registry, or NULL if none is configured.
   */
  glr_lexer_hooks_t *glr_reader_get_lexer_hooks (const glr_reader_t *reader);

  /**
   * @brief Decode and return the next terminal token.
   * @param reader Reader whose cursor will advance on success.
   * @param token Output token initialized by the function.
   * @return Status describing success, EOF, invalid input, or allocation failure.
   */
  glr_reader_status_t glr_reader_next (glr_reader_t *reader,
                                       glr_reader_token_t *token);

  /**
   * @brief Release heap fields owned by a token and reset it to an empty state.
   * @param token Token produced by @ref glr_reader_next; NULL is ignored.
   */
  void glr_reader_token_clear (glr_reader_token_t *token);

  /**
   * @brief Convert a reader status to a stable diagnostic string.
   * @param status Status code to describe.
   * @return Static, null-terminated description string.
   */
  const char *glr_reader_status_string (glr_reader_status_t status);

  /**
   * @brief Get the current byte offset of the reader cursor.
   * @param reader Reader to query (may be NULL).
   * @return Cursor offset in bytes, or 0 for NULL.
   */
  size_t glr_reader_get_offset (const glr_reader_t *reader);

  /**
   * @brief Get the number of unread bytes remaining.
   * @param reader Reader to query (may be NULL).
   * @return Remaining bytes, or 0 for NULL / exhausted input.
   */
  size_t glr_reader_remaining (const glr_reader_t *reader);

  /**
   * @brief Check whether the cursor reached the end of input.
   * @param reader Reader to query (may be NULL).
   * @return true at EOF (NULL counts as EOF).
   */
  bool glr_reader_at_eof (const glr_reader_t *reader);

#ifdef __cplusplus
}
#endif

#endif /* GLR_READER_H */
