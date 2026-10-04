#include <glr/reader.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct glr_reader
{
  const unsigned char *input;
  size_t input_length;
  size_t offset;
  size_t bom_bytes;
  glr_reader_encoding_t configured_encoding;
  glr_reader_encoding_t active_encoding;
  glr_lexer_hooks_t *hooks;
};

extern bool glr_lexer_hooks_dispatch (glr_lexer_hooks_t *hooks,
                                      const glr_lexer_event_t *event,
                                      glr_lexer_response_t *response);

/* strdup() is not part of C11, so the reader carries its own copy helper. */
static char *
glr_reader_strdup (const char *text)
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

static uint16_t
read_u16 (const unsigned char *data, glr_reader_encoding_t encoding)
{
  if (encoding == GLR_READER_ENCODING_UTF16_BE)
    {
      return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    }

  return (uint16_t)(((uint16_t)data[1] << 8) | data[0]);
}

static int
resolve_encoding (glr_reader_t *reader)
{
  if (reader == NULL)
    {
      return -1;
    }

  reader->bom_bytes = 0;
  if (reader->configured_encoding == GLR_READER_ENCODING_UTF16_AUTO)
    {
      reader->active_encoding = GLR_READER_ENCODING_UTF16_LE;
      if (reader->input_length >= 2)
        {
          if (reader->input[0] == 0xFE && reader->input[1] == 0xFF)
            {
              reader->active_encoding = GLR_READER_ENCODING_UTF16_BE;
              reader->bom_bytes = 2;
            }
          else if (reader->input[0] == 0xFF && reader->input[1] == 0xFE)
            {
              reader->active_encoding = GLR_READER_ENCODING_UTF16_LE;
              reader->bom_bytes = 2;
            }
        }
    }
  else
    {
      reader->active_encoding = reader->configured_encoding;
    }

  reader->offset = reader->bom_bytes;
  return 0;
}

static glr_reader_status_t
make_fallback_name (uint32_t codepoint, char **name_out)
{
  const char *unicode_name;
  char buffer[32];
  int length;

  if (name_out == NULL)
    {
      return GLR_READER_STATUS_INVALID_ARGUMENT;
    }

  *name_out = NULL;
  unicode_name = glr_lexer_unicode_name (codepoint);
  if (unicode_name != NULL)
    {
      *name_out = glr_reader_strdup (unicode_name);
      return *name_out != NULL ? GLR_READER_STATUS_OK
                               : GLR_READER_STATUS_NO_MEMORY;
    }

  length = snprintf (buffer, sizeof (buffer), "U+%04X", (unsigned)codepoint);
  if (length < 0 || (size_t)length >= sizeof (buffer))
    {
      return GLR_READER_STATUS_INVALID_SEQUENCE;
    }

  *name_out = glr_reader_strdup (buffer);
  return *name_out != NULL ? GLR_READER_STATUS_OK : GLR_READER_STATUS_NO_MEMORY;
}

static glr_reader_status_t
decode_codepoint (glr_reader_t *reader, size_t offset, uint32_t *codepoint,
                  size_t *bytes_consumed)
{
  uint16_t first;
  uint16_t second;

  if (reader == NULL || codepoint == NULL || bytes_consumed == NULL)
    {
      return GLR_READER_STATUS_INVALID_ARGUMENT;
    }

  if (offset >= reader->input_length)
    {
      return GLR_READER_STATUS_EOF;
    }

  if ((reader->input_length - offset) < 2)
    {
      return GLR_READER_STATUS_INVALID_ENCODING;
    }

  first = read_u16 (reader->input + offset, reader->active_encoding);
  if (first >= 0xD800 && first <= 0xDBFF)
    {
      if ((reader->input_length - offset) < 4)
        {
          return GLR_READER_STATUS_INVALID_SEQUENCE;
        }

      second = read_u16 (reader->input + offset + 2, reader->active_encoding);
      if (second < 0xDC00 || second > 0xDFFF)
        {
          return GLR_READER_STATUS_INVALID_SEQUENCE;
        }

      *codepoint = 0x10000 + ((((uint32_t)first - 0xD800) << 10)
                              | ((uint32_t)second - 0xDC00));
      *bytes_consumed = 4;
      return GLR_READER_STATUS_OK;
    }

  if (first >= 0xDC00 && first <= 0xDFFF)
    {
      return GLR_READER_STATUS_INVALID_SEQUENCE;
    }

  *codepoint = first;
  *bytes_consumed = 2;
  return GLR_READER_STATUS_OK;
}

glr_reader_t *
glr_reader_create (void)
{
  glr_reader_t *reader = calloc (1, sizeof (*reader));
  if (reader != NULL)
    {
      reader->configured_encoding = GLR_READER_ENCODING_UTF16_LE;
      reader->active_encoding = GLR_READER_ENCODING_UTF16_LE;
    }
  return reader;
}

void
glr_reader_destroy (glr_reader_t *reader)
{
  free (reader);
}

int
glr_reader_set_input (glr_reader_t *reader, const void *input, size_t input_length)
{
  if (reader == NULL || (input == NULL && input_length != 0))
    {
      return -1;
    }

  reader->input = input;
  reader->input_length = input_length;
  return resolve_encoding (reader);
}

void
glr_reader_reset (glr_reader_t *reader)
{
  if (reader != NULL)
    {
      reader->offset = reader->bom_bytes;
    }
}

void
glr_reader_set_encoding (glr_reader_t *reader, glr_reader_encoding_t encoding)
{
  if (reader != NULL)
    {
      reader->configured_encoding = encoding;
      (void)resolve_encoding (reader);
    }
}

glr_reader_encoding_t
glr_reader_get_encoding (const glr_reader_t *reader)
{
  return reader != NULL ? reader->active_encoding : GLR_READER_ENCODING_UTF16_LE;
}

int
glr_reader_set_lexer_hooks (glr_reader_t *reader, glr_lexer_hooks_t *hooks)
{
  if (reader == NULL)
    {
      return -1;
    }

  reader->hooks = hooks;
  return 0;
}

glr_lexer_hooks_t *
glr_reader_get_lexer_hooks (const glr_reader_t *reader)
{
  return reader != NULL ? reader->hooks : NULL;
}

void
glr_reader_token_clear (glr_reader_token_t *token)
{
  if (token != NULL)
    {
      free (token->terminal_name);
      memset (token, 0, sizeof (*token));
    }
}

glr_reader_status_t
glr_reader_next (glr_reader_t *reader, glr_reader_token_t *token)
{
  glr_reader_status_t status;
  glr_lexer_event_t event;
  glr_lexer_response_t response;
  uint32_t codepoint;
  size_t default_bytes_consumed;
  char *name = NULL;

  if (reader == NULL || token == NULL)
    {
      return GLR_READER_STATUS_INVALID_ARGUMENT;
    }

  glr_reader_token_clear (token);
  status = decode_codepoint (reader, reader->offset, &codepoint,
                             &default_bytes_consumed);
  if (status != GLR_READER_STATUS_OK)
    {
      return status;
    }

  event.codepoint = codepoint;
  event.unicode_name = glr_lexer_unicode_name (codepoint);
  event.input = reader->input;
  event.input_length = reader->input_length;
  event.byte_offset = reader->offset;
  event.default_bytes_consumed = default_bytes_consumed;
  event.user_data = glr_lexer_hooks_get_user_data (reader->hooks);

  if (glr_lexer_hooks_dispatch (reader->hooks, &event, &response))
    {
      name = glr_reader_strdup (response.terminal_name);
      if (name == NULL)
        {
          return GLR_READER_STATUS_NO_MEMORY;
        }
      token->from_hook = true;
      token->bytes_consumed = response.bytes_consumed;
    }
  else
    {
      status = make_fallback_name (codepoint, &name);
      if (status != GLR_READER_STATUS_OK)
        {
          return status;
        }
      token->from_hook = false;
      token->bytes_consumed = default_bytes_consumed;
    }

  if (reader->offset + token->bytes_consumed > reader->input_length)
    {
      free (name);
      return GLR_READER_STATUS_INVALID_SEQUENCE;
    }

  token->terminal_name = name;
  token->codepoint = codepoint;
  token->byte_offset = reader->offset;
  reader->offset += token->bytes_consumed;
  return GLR_READER_STATUS_OK;
}

const char *
glr_reader_status_string (glr_reader_status_t status)
{
  switch (status)
    {
    case GLR_READER_STATUS_OK:
      return "ok";
    case GLR_READER_STATUS_EOF:
      return "end of input";
    case GLR_READER_STATUS_INVALID_ARGUMENT:
      return "invalid argument";
    case GLR_READER_STATUS_INVALID_ENCODING:
      return "invalid UTF-16 encoding";
    case GLR_READER_STATUS_INVALID_SEQUENCE:
      return "invalid UTF-16 sequence";
    case GLR_READER_STATUS_NO_MEMORY:
      return "out of memory";
    }

  return "unknown";
}

size_t
glr_reader_get_offset (const glr_reader_t *reader)
{
  return reader != NULL ? reader->offset : 0;
}

size_t
glr_reader_remaining (const glr_reader_t *reader)
{
  if (reader == NULL || reader->input == NULL)
    {
      return 0;
    }
  if (reader->offset >= reader->input_length)
    {
      return 0;
    }
  return reader->input_length - reader->offset;
}

bool
glr_reader_at_eof (const glr_reader_t *reader)
{
  if (reader == NULL)
    {
      return true;
    }
  return reader->offset >= reader->input_length;
}
