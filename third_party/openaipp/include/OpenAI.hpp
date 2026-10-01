/// @file include/OpenAI.hpp
/// @brief Header-only C++20 client for the OpenAI API.
///
/// This library provides a complete, type-safe client for the OpenAI API
/// organized in three layers:
/// - @ref openapipp::low — raw endpoint functions matching the openapi spec.
/// - @ref openapipp::high — typed wrappers with structs, enums, and client classes.
/// - @ref openapipp::dsl — fluent DSL built on DSLtk for composable API calls.


#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <httplib.h>

static_assert (__cplusplus >= 202002L, "openaipp requires C++20");

// Optional high-level DSL layer (header-only dependency)
#include <MetaTk/DSLtk/DSLtk.hpp>

/// @brief Raw endpoint functions matching the OpenAI API spec.
///
/// Every function in this namespace corresponds to exactly one OpenAI endpoint.
/// Parameters are plain values, and every function returns parsed
/// `nlohmann::json`. The schema in `third_party/openai-openapi` is
/// authoritative; always match it when adding or changing endpoints.
namespace openapipp::low
{

/// @brief Exception thrown on HTTP transport or API-level errors.
///
/// Carries the HTTP status code, the underlying httplib transport error,
/// and the response body (which often contains an OpenAI error message).
struct HttpError final : std::runtime_error
{
  /// @brief HTTP status code (e.g., 200, 429, 500).
  int http_status = 0;
  /// @brief Underlying transport error from cpp-httplib.
  httplib::Error transport_error = httplib::Error::Unknown;
  /// @brief Raw response body, often containing a JSON error object.
  std::string response_body;

  /// @brief Construct an HttpError with a message.
  explicit HttpError (std::string message)
      : std::runtime_error (std::move (message))
  {
  }
};

/// @brief Configuration for the HTTP client connecting to the OpenAI API.
///
/// Fields default to environment variables where applicable.
/// Use @ref from_env() to populate all fields from the environment.
struct ClientConfig
{
  /// @brief Base URL of the OpenAI API. Defaults to @c OPENAI_BASE env var.
  std::string base_url = std::getenv ("OPENAI_BASE");
  /// @brief API key. Set from @c OPENAI_API_KEY.
  std::string api_key;
  /// @brief Organization ID. Set from @c OPENAI_ORG_ID.
  std::string organization;
  /// @brief Project ID. Set from @c OPENAI_PROJECT_ID.
  std::string project;
  /// @brief User-Agent header. Defaults to @c OPENAI_USER_AGENT env var.
  std::string user_agent = std::getenv ("OPENAI_USER_AGENT");
  /// @brief HTTP request timeout in seconds. Defaults to 60.
  int timeout_seconds = 60;
};

/// @brief Throw if a value is empty.
/// @param value The value to check.
/// @param name Human-readable name for the error message.
/// @throws std::invalid_argument if @p value is empty.
inline void
require_nonempty (std::string_view value, std::string_view name)
{
  if (value.empty ())
    throw std::invalid_argument (std::string (name) + " must not be empty");
}

/// @brief URL-encode a path segment.
/// @param value The raw path segment.
/// @return The percent-encoded path segment.
inline std::string
url_encode_path_segment (std::string_view value)
{
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve (value.size ());

  for (unsigned char c : value)
    {
      const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                              || (c >= '0' && c <= '9') || c == '-' || c == '_'
                              || c == '.' || c == '~';
      if (unreserved)
        out.push_back (static_cast<char> (c));
      else
        {
          out.push_back ('%');
          out.push_back (hex[c >> 4]);
          out.push_back (hex[c & 0x0f]);
        }
    }

  return out;
}

/// @brief Escape special characters in a JSON string value.
/// @param value The raw string to escape.
/// @return The JSON-escaped string (without surrounding quotes).
inline std::string
json_escape (std::string_view value)
{
  std::string out;
  out.reserve (value.size () + 2);

  for (unsigned char c : value)
    {
      switch (c)
        {
        case '"':
          out += "\\\"";
          break;
        case '\\':
          out += "\\\\";
          break;
        case '\b':
          out += "\\b";
          break;
        case '\f':
          out += "\\f";
          break;
        case '\n':
          out += "\\n";
          break;
        case '\r':
          out += "\\r";
          break;
        case '\t':
          out += "\\t";
          break;
        default:
          if (c < 0x20)
            {
              out += "\\u00";
              static constexpr char hex[] = "0123456789abcdef";
              out.push_back (hex[c >> 4]);
              out.push_back (hex[c & 0x0f]);
            }
          else
            out.push_back (static_cast<char> (c));
        }
    }

  return out;
}

/// @brief Wrap a string in JSON double-quotes with escaping.
/// @param value The raw string.
/// @return A quoted and escaped JSON string.
inline std::string
json_string (std::string_view value)
{
  return '"' + json_escape (value) + '"';
}

/// @brief Build a JSON array from a vector of strings.
/// @param values The strings to include.
/// @return A JSON array string.
inline std::string
json_array (const std::vector<std::string> &values)
{
  std::string out = "[";
  bool first = true;
  for (const auto &value : values)
    {
      if (!first)
        out += ',';
      first = false;
      out += json_string (value);
    }
  out += ']';
  return out;
}

/// @brief A single key-value pair for building JSON objects.
struct JsonField
{
  /// @brief The field name (key).
  std::string name;
  /// @brief The field value as a pre-serialized JSON string.
  std::string value_json;
};

/// @brief Build a JSON object from field pairs.
/// @param fields An initializer list of JsonField name/value pairs.
/// @return A JSON object string.
inline std::string
json_object (std::initializer_list<JsonField> fields)
{
  std::string out = "{";
  bool first = true;
  for (const auto &field : fields)
    {
      if (field.name.empty () || field.value_json.empty ())
        continue;
      if (!first)
        out += ',';
      first = false;
      out += json_string (field.name);
      out += ':';
      out += field.value_json;
    }
  out += '}';
  return out;
}

/// @brief Serialize a double to a JSON number.
/// @param value A finite double value.
/// @return The JSON number string.
/// @throws std::invalid_argument if @p value is not finite.
inline std::string
json_number (double value)
{
  if (!std::isfinite (value))
    throw std::invalid_argument ("JSON number must be finite");

  std::ostringstream oss;
  oss.precision (std::numeric_limits<double>::max_digits10);
  oss << value;
  return oss.str ();
}

/// @brief Read an environment variable as a std::string.
/// @param name The environment variable name (may be nullptr).
/// @return The variable value, or an empty string if unset.
inline std::string
getenv_string (const char *name)
{
  if (name == nullptr)
    return {};
  if (const char *v = std::getenv (name))
    return std::string (v);
  return {};
}

/// @brief Build a ClientConfig from environment variables.
///
/// Reads @c OPENAI_API_KEY, @c OPENAI_ORG_ID, and @c OPENAI_PROJECT_ID.
/// @return A populated ClientConfig.
inline ClientConfig
from_env ()
{
  ClientConfig cfg;
  cfg.api_key = getenv_string ("OPENAI_API_KEY");
  cfg.organization = getenv_string ("OPENAI_ORG_ID");
  cfg.project = getenv_string ("OPENAI_PROJECT_ID");
  return cfg;
}

/// @brief Parsed components of a URL.
struct ParsedUrl
{
  /// @brief URL scheme (e.g., "http" or "https").
  std::string scheme;
  /// @brief Hostname.
  std::string host;
  /// @brief Port number (0 if not specified).
  int port = 0;
  /// @brief Path component after the host.
  std::string base_path;
};

/// @brief Parse a base URL into its components.
/// @param base_url The URL string (must include scheme, e.g., https://).
/// @return A ParsedUrl with the decomposed parts.
/// @throws std::invalid_argument if the URL has no scheme or an unsupported one.
inline ParsedUrl
parse_base_url (std::string_view base_url)
{
  // Very small URL parser: scheme://host[:port][/path]
  auto scheme_pos = base_url.find ("://");
  if (scheme_pos == std::string_view::npos)
    throw std::invalid_argument (
        "base_url must contain scheme (e.g. https://)");

  ParsedUrl out;
  out.scheme = std::string (base_url.substr (0, scheme_pos));
  if (out.scheme != "http" && out.scheme != "https")
    throw std::invalid_argument ("unsupported URL scheme: " + out.scheme);

#if !defined(CPPHTTPLIB_OPENSSL_SUPPORT)
  if (out.scheme == "https")
    throw std::invalid_argument (
        "https base_url requires CPPHTTPLIB_OPENSSL_SUPPORT; enable "
        "OPENAIPP_ENABLE_OPENSSL or use http");
#endif

  auto rest = base_url.substr (scheme_pos + 3);
  auto slash = rest.find ('/');
  auto hostport
      = (slash == std::string_view::npos) ? rest : rest.substr (0, slash);
  out.base_path = (slash == std::string_view::npos)
                      ? std::string{}
                      : std::string (rest.substr (slash));
  if (!out.base_path.empty () && out.base_path.back () == '/')
    out.base_path.pop_back ();

  auto colon = hostport.rfind (':');
  if (colon != std::string_view::npos)
    {
      out.host = std::string (hostport.substr (0, colon));
      auto port_sv = hostport.substr (colon + 1);
      if (port_sv.empty ())
        throw std::invalid_argument ("base_url has empty port");
      try
        {
          out.port = std::stoi (std::string (port_sv));
        }
      catch (...)
        {
          throw std::invalid_argument ("base_url port must be an integer");
        }
      if (out.port <= 0 || out.port > 65535)
        throw std::invalid_argument ("base_url port must be in 1..65535");
    }
  else
    {
      out.host = std::string (hostport);
      if (out.scheme == "https")
        out.port = 443;
      else if (out.scheme == "http")
        out.port = 80;
    }

  if (out.host.empty ())
    throw std::invalid_argument ("base_url host is empty");

  return out;
}

inline std::string
append_query (std::string path, std::initializer_list<JsonField> params)
{
  bool first = true;
  for (const auto &param : params)
    {
      if (param.name.empty () || param.value_json.empty ())
        continue;
      path += first ? '?' : '&';
      first = false;
      path += url_encode_path_segment (param.name);
      path += '=';
      path += url_encode_path_segment (param.value_json);
    }
  return path;
}

inline httplib::Headers
default_headers (const ClientConfig &cfg)
{
  httplib::Headers headers;
  if (!cfg.user_agent.empty ())
    headers.emplace ("User-Agent", cfg.user_agent);
  headers.emplace ("Accept", "application/json");
  headers.emplace ("Content-Type", "application/json");
  if (!cfg.api_key.empty ())
    headers.emplace ("Authorization", "Bearer " + cfg.api_key);
  if (!cfg.organization.empty ())
    headers.emplace ("OpenAI-Organization", cfg.organization);
  if (!cfg.project.empty ())
    headers.emplace ("OpenAI-Project", cfg.project);
  return headers;
}

/// @brief HTTP client wrapping cpp-httplib for the OpenAI API.
///
/// Manages headers (Authorization, Organization, etc.), URL construction,
/// and HTTP method calls. Constructed from a @ref ClientConfig.
class HttpClient
{
public:
  explicit HttpClient (ClientConfig cfg)
      : cfg_ (std::move (cfg)), parsed_ (parse_base_url (cfg_.base_url)),
        cli_ (parsed_.scheme + "://" + parsed_.host + ":"
              + std::to_string (parsed_.port))
  {
    cli_.set_read_timeout (cfg_.timeout_seconds, 0);
    cli_.set_write_timeout (cfg_.timeout_seconds, 0);
    cli_.set_connection_timeout (cfg_.timeout_seconds, 0);
  }

  const ClientConfig &
  config () const
  {
    return cfg_;
  }

  std::string
  get (std::string_view path)
  {
    return request_json ("GET", path, std::nullopt);
  }

  std::string
  del (std::string_view path)
  {
    return request_json ("DELETE", path, std::nullopt);
  }

  std::string
  post (std::string_view path, std::string body)
  {
    return request_json ("POST", path, std::move (body));
  }

  std::string
  patch (std::string_view path, std::string body)
  {
    return request_json ("PATCH", path, std::move (body));
  }

private:
  std::string
  request_json (std::string_view method, std::string_view path,
                std::optional<std::string> body)
  {
    auto headers = default_headers (cfg_);
    const std::string full_path = parsed_.base_path + std::string (path);

    httplib::Result res;
    if (method == "GET")
      res = cli_.Get (full_path, headers);
    else if (method == "DELETE")
      res = cli_.Delete (full_path, headers);
    else if (method == "POST")
      res = cli_.Post (full_path, headers, body.value_or (""),
                       "application/json");
    else if (method == "PATCH")
      res = cli_.Patch (full_path, headers, body.value_or (""),
                        "application/json");
    else
      throw std::invalid_argument ("unsupported HTTP method");

    if (!res)
      {
        HttpError err ("transport error: "
                       + httplib::to_string (res.error ()));
        err.transport_error = res.error ();
        throw err;
      }

    if (res->status < 200 || res->status >= 300)
      {
        HttpError err ("http error: status=" + std::to_string (res->status));
        err.http_status = res->status;
        err.response_body = res->body;
        throw err;
      }

    return res->body;
  }

private:
  ClientConfig cfg_;
  ParsedUrl parsed_;
  httplib::Client cli_;
};

// ------------------------------------------------------------
// Low-level endpoints (naive, string-in/string-out)
// ------------------------------------------------------------

inline std::string
/// @brief List available models.
/// @param http The HTTP client.
/// @return JSON array of model objects.

models_list (HttpClient &http)
{
  return http.get ("/models");
}

inline std::string
/// @brief Retrieve a model by ID.
/// @param http The HTTP client.
/// @param model_id The model identifier.
/// @return JSON object with model metadata.

models_retrieve (HttpClient &http, std::string_view model)
{
  require_nonempty (model, "model");
  return http.get ("/models/" + url_encode_path_segment (model));
}

inline std::string
/// @brief Delete a fine-tuned model.
/// @param http The HTTP client.
/// @param model_id The model identifier.
/// @return JSON deletion confirmation.

models_delete (HttpClient &http, std::string_view model)
{
  require_nonempty (model, "model");
  return http.del ("/models/" + url_encode_path_segment (model));
}

inline std::string
/// @brief Create a response using the Responses API.
/// @param http The HTTP client.
/// @param body_json JSON request body with model and input.
/// @return JSON response with the generated output.

responses_create (HttpClient &http, std::string body_json)
{
  return http.post ("/responses", std::move (body_json));
}

inline std::string
responses_retrieve (HttpClient &http, std::string_view response_id)
{
  require_nonempty (response_id, "response_id");
  return http.get ("/responses/" + url_encode_path_segment (response_id));
}

inline std::string
responses_delete (HttpClient &http, std::string_view response_id)
{
  require_nonempty (response_id, "response_id");
  return http.del ("/responses/" + url_encode_path_segment (response_id));
}

inline std::string
responses_cancel (HttpClient &http, std::string_view response_id)
{
  require_nonempty (response_id, "response_id");
  return http.post (
      "/responses/" + url_encode_path_segment (response_id) + "/cancel", "{}");
}

inline std::string
/// @brief Create a chat completion.
/// @param http The HTTP client.
/// @param body_json JSON request body with model and messages.
/// @return JSON response with the completion.

chat_completions_create (HttpClient &http, std::string body_json)
{
  return http.post ("/chat/completions", std::move (body_json));
}

inline std::string
/// @brief Create embeddings for input text.
/// @param http The HTTP client.
/// @param body_json JSON request body with model and input.
/// @return JSON response with embedding vectors.

embeddings_create (HttpClient &http, std::string body_json)
{
  return http.post ("/embeddings", std::move (body_json));
}

inline std::string
/// @brief Classify text for harmful content.
/// @param http The HTTP client.
/// @param body_json JSON request body with input text.
/// @return JSON response with moderation categories and scores.

moderations_create (HttpClient &http, std::string body_json)
{
  return http.post ("/moderations", std::move (body_json));
}

inline std::string
images_generate (HttpClient &http, std::string body_json)
{
  return http.post ("/images/generations", std::move (body_json));
}

inline std::string
images_edit (HttpClient &http, std::string body_json)
{
  return http.post ("/images/edits", std::move (body_json));
}

inline std::string
images_variation (HttpClient &http, std::string body_json)
{
  return http.post ("/images/variations", std::move (body_json));
}

inline std::string
audio_speech_create (HttpClient &http, std::string body_json)
{
  return http.post ("/audio/speech", std::move (body_json));
}

inline std::string
audio_transcription_create (HttpClient &http, std::string body_json)
{
  return http.post ("/audio/transcriptions", std::move (body_json));
}

inline std::string
audio_translation_create (HttpClient &http, std::string body_json)
{
  return http.post ("/audio/translations", std::move (body_json));
}

inline std::string
/// @brief List uploaded files.
/// @param http The HTTP client.
/// @return JSON array of file objects.

files_list (HttpClient &http)
{
  return http.get ("/files");
}

inline std::string
files_retrieve (HttpClient &http, std::string_view file_id)
{
  require_nonempty (file_id, "file_id");
  return http.get ("/files/" + url_encode_path_segment (file_id));
}

inline std::string
files_content (HttpClient &http, std::string_view file_id)
{
  require_nonempty (file_id, "file_id");
  return http.get ("/files/" + url_encode_path_segment (file_id) + "/content");
}

inline std::string
/// @brief Delete an uploaded file.
/// @param http The HTTP client.
/// @param file_id The file identifier.
/// @return JSON deletion confirmation.

files_delete (HttpClient &http, std::string_view file_id)
{
  require_nonempty (file_id, "file_id");
  return http.del ("/files/" + url_encode_path_segment (file_id));
}

inline std::string
uploads_create (HttpClient &http, std::string body_json)
{
  return http.post ("/uploads", std::move (body_json));
}

inline std::string
uploads_cancel (HttpClient &http, std::string_view upload_id)
{
  require_nonempty (upload_id, "upload_id");
  return http.post (
      "/uploads/" + url_encode_path_segment (upload_id) + "/cancel", "{}");
}

inline std::string
uploads_complete (HttpClient &http, std::string_view upload_id,
                  std::string body_json)
{
  require_nonempty (upload_id, "upload_id");
  return http.post ("/uploads/" + url_encode_path_segment (upload_id)
                        + "/complete",
                    std::move (body_json));
}

inline std::string
/// @brief List batch jobs.
/// @param http The HTTP client.
/// @return JSON array of batch objects.

batches_list (HttpClient &http)
{
  return http.get ("/batches");
}

inline std::string
/// @brief Create a batch job.
/// @param http The HTTP client.
/// @param body_json JSON request body with input_file_id and endpoint.
/// @return JSON response with the created batch.

batches_create (HttpClient &http, std::string body_json)
{
  return http.post ("/batches", std::move (body_json));
}

inline std::string
/// @brief Retrieve a batch job.
/// @param http The HTTP client.
/// @param batch_id The batch identifier.
/// @return JSON object with batch metadata.

batches_retrieve (HttpClient &http, std::string_view batch_id)
{
  require_nonempty (batch_id, "batch_id");
  return http.get ("/batches/" + url_encode_path_segment (batch_id));
}

inline std::string
/// @brief Cancel a batch job.
/// @param http The HTTP client.
/// @param batch_id The batch identifier.
/// @return JSON response with the cancelled batch.

batches_cancel (HttpClient &http, std::string_view batch_id)
{
  require_nonempty (batch_id, "batch_id");
  return http.post (
      "/batches/" + url_encode_path_segment (batch_id) + "/cancel", "{}");
}

inline std::string
/// @brief List fine-tuning jobs.
/// @param http The HTTP client.
/// @return JSON array of fine-tuning job objects.

fine_tuning_jobs_list (HttpClient &http)
{
  return http.get ("/fine_tuning/jobs");
}

inline std::string
/// @brief Create a fine-tuning job.
/// @param http The HTTP client.
/// @param body_json JSON request body with model and training_file.
/// @return JSON response with the fine-tuning job.

fine_tuning_jobs_create (HttpClient &http, std::string body_json)
{
  return http.post ("/fine_tuning/jobs", std::move (body_json));
}

inline std::string
/// @brief Retrieve a fine-tuning job.
/// @param http The HTTP client.
/// @param fine_tuning_job_id The fine-tuning job identifier.
/// @return JSON object with job metadata.

fine_tuning_jobs_retrieve (HttpClient &http,
                           std::string_view fine_tuning_job_id)
{
  require_nonempty (fine_tuning_job_id, "fine_tuning_job_id");
  return http.get ("/fine_tuning/jobs/"
                   + url_encode_path_segment (fine_tuning_job_id));
}

inline std::string
/// @brief Cancel a fine-tuning job.
/// @param http The HTTP client.
/// @param fine_tuning_job_id The fine-tuning job identifier.
/// @return JSON response with the cancelled job.

fine_tuning_jobs_cancel (HttpClient &http, std::string_view fine_tuning_job_id)
{
  require_nonempty (fine_tuning_job_id, "fine_tuning_job_id");
  return http.post ("/fine_tuning/jobs/"
                        + url_encode_path_segment (fine_tuning_job_id)
                        + "/cancel",
                    "{}");
}

inline std::string
vector_stores_list (HttpClient &http)
{
  return http.get ("/vector_stores");
}

inline std::string
vector_stores_create (HttpClient &http, std::string body_json)
{
  return http.post ("/vector_stores", std::move (body_json));
}

inline std::string
vector_stores_retrieve (HttpClient &http, std::string_view vector_store_id)
{
  require_nonempty (vector_store_id, "vector_store_id");
  return http.get ("/vector_stores/"
                   + url_encode_path_segment (vector_store_id));
}

inline std::string
vector_stores_modify (HttpClient &http, std::string_view vector_store_id,
                      std::string body_json)
{
  require_nonempty (vector_store_id, "vector_store_id");
  return http.post ("/vector_stores/"
                        + url_encode_path_segment (vector_store_id),
                    std::move (body_json));
}

inline std::string
vector_stores_delete (HttpClient &http, std::string_view vector_store_id)
{
  require_nonempty (vector_store_id, "vector_store_id");
  return http.del ("/vector_stores/"
                   + url_encode_path_segment (vector_store_id));
}

inline std::string
/// @brief List assistants.
/// @param http The HTTP client.
/// @return JSON array of assistant objects.

assistants_list (HttpClient &http)
{
  return http.get ("/assistants");
}

inline std::string
/// @brief Create an assistant.
/// @param http The HTTP client.
/// @param body_json JSON request body with model and instructions.
/// @return JSON response with the created assistant.

assistants_create (HttpClient &http, std::string body_json)
{
  return http.post ("/assistants", std::move (body_json));
}

inline std::string
/// @brief Retrieve an assistant.
/// @param http The HTTP client.
/// @param assistant_id The assistant identifier.
/// @return JSON object with assistant metadata.

assistants_retrieve (HttpClient &http, std::string_view assistant_id)
{
  require_nonempty (assistant_id, "assistant_id");
  return http.get ("/assistants/" + url_encode_path_segment (assistant_id));
}

inline std::string
/// @brief Modify an assistant.
/// @param http The HTTP client.
/// @param assistant_id The assistant identifier.
/// @param body_json JSON request body with fields to update.
/// @return JSON response with the modified assistant.

assistants_modify (HttpClient &http, std::string_view assistant_id,
                   std::string body_json)
{
  require_nonempty (assistant_id, "assistant_id");
  return http.post ("/assistants/" + url_encode_path_segment (assistant_id),
                    std::move (body_json));
}

inline std::string
/// @brief Delete an assistant.
/// @param http The HTTP client.
/// @param assistant_id The assistant identifier.
/// @return JSON deletion confirmation.

assistants_delete (HttpClient &http, std::string_view assistant_id)
{
  require_nonempty (assistant_id, "assistant_id");
  return http.del ("/assistants/" + url_encode_path_segment (assistant_id));
}

inline std::string
/// @brief Create a thread.
/// @param http The HTTP client.
/// @param body_json Optional JSON request body (defaults to empty object).
/// @return JSON response with the created thread.

threads_create (HttpClient &http, std::string body_json = "{}")
{
  return http.post ("/threads", std::move (body_json));
}

inline std::string
/// @brief Retrieve a thread.
/// @param http The HTTP client.
/// @param thread_id The thread identifier.
/// @return JSON object with thread metadata.

threads_retrieve (HttpClient &http, std::string_view thread_id)
{
  require_nonempty (thread_id, "thread_id");
  return http.get ("/threads/" + url_encode_path_segment (thread_id));
}

inline std::string
/// @brief Modify a thread.
/// @param http The HTTP client.
/// @param thread_id The thread identifier.
/// @param body_json JSON request body with fields to update.
/// @return JSON response with the modified thread.

threads_modify (HttpClient &http, std::string_view thread_id,
                std::string body_json)
{
  require_nonempty (thread_id, "thread_id");
  return http.post ("/threads/" + url_encode_path_segment (thread_id),
                    std::move (body_json));
}

inline std::string
/// @brief Delete a thread.
/// @param http The HTTP client.
/// @param thread_id The thread identifier.
/// @return JSON deletion confirmation.

threads_delete (HttpClient &http, std::string_view thread_id)
{
  require_nonempty (thread_id, "thread_id");
  return http.del ("/threads/" + url_encode_path_segment (thread_id));
}

inline std::string
/// @brief List messages in a thread.
/// @param http The HTTP client.
/// @param thread_id The thread identifier.
/// @return JSON array of message objects.

thread_messages_list (HttpClient &http, std::string_view thread_id)
{
  require_nonempty (thread_id, "thread_id");
  return http.get ("/threads/" + url_encode_path_segment (thread_id)
                   + "/messages");
}

inline std::string
/// @brief Create a message in a thread.
/// @param http The HTTP client.
/// @param thread_id The thread identifier.
/// @param body_json JSON request body with role and content.
/// @return JSON response with the created message.

thread_messages_create (HttpClient &http, std::string_view thread_id,
                        std::string body_json)
{
  require_nonempty (thread_id, "thread_id");
  return http.post ("/threads/" + url_encode_path_segment (thread_id)
                        + "/messages",
                    std::move (body_json));
}

inline std::string
/// @brief List runs in a thread.
/// @param http The HTTP client.
/// @param thread_id The thread identifier.
/// @return JSON array of run objects.

thread_runs_list (HttpClient &http, std::string_view thread_id)
{
  require_nonempty (thread_id, "thread_id");
  return http.get ("/threads/" + url_encode_path_segment (thread_id)
                   + "/runs");
}

inline std::string
/// @brief Create a run on a thread.
/// @param http The HTTP client.
/// @param thread_id The thread identifier.
/// @param body_json JSON request body with assistant_id.
/// @return JSON response with the created run.

thread_runs_create (HttpClient &http, std::string_view thread_id,
                    std::string body_json)
{
  require_nonempty (thread_id, "thread_id");
  return http.post ("/threads/" + url_encode_path_segment (thread_id)
                        + "/runs",
                    std::move (body_json));
}

inline std::string
/// @brief Retrieve a run.
/// @param http The HTTP client.
/// @param thread_id The thread identifier.
/// @param run_id The run identifier.
/// @return JSON object with run metadata.

thread_runs_retrieve (HttpClient &http, std::string_view thread_id,
                      std::string_view run_id)
{
  require_nonempty (thread_id, "thread_id");
  require_nonempty (run_id, "run_id");
  return http.get ("/threads/" + url_encode_path_segment (thread_id) + "/runs/"
                   + url_encode_path_segment (run_id));
}

inline std::string
/// @brief Cancel a run.
/// @param http The HTTP client.
/// @param thread_id The thread identifier.
/// @param run_id The run identifier.
/// @return JSON response with the cancelled run.

thread_runs_cancel (HttpClient &http, std::string_view thread_id,
                    std::string_view run_id)
{
  require_nonempty (thread_id, "thread_id");
  require_nonempty (run_id, "run_id");
  return http.post ("/threads/" + url_encode_path_segment (thread_id)
                        + "/runs/" + url_encode_path_segment (run_id)
                        + "/cancel",
                    "{}");
}

} // namespace openapipp::low

/// @brief Typed, high-level client for the OpenAI API.
///
/// Wraps the low-level endpoint functions with classes, structs, and enums.
/// Request and response bodies become C++ types, string enums become C++
/// enums, and endpoints become methods on client classes. Every construct
/// delegates to @ref openapipp::low — do not duplicate HTTP or JSON logic here.
namespace openapipp::high
{

using openapipp::low::ClientConfig;
using openapipp::low::HttpClient;

/// @brief Wrapper for an API response body.
struct ResponseText
{
  /// @brief The raw JSON response string.
  std::string json;
};

/// @brief High-level typed client for the OpenAI API.
///
/// Wraps @ref openapipp::low endpoint functions with typed methods.
/// Construct via @c Client::from_env() or with an explicit @ref ClientConfig.
class Client
{
public:
  /// @brief Construct a Client from a configuration.
  explicit Client (ClientConfig cfg) : http_ (std::move (cfg)) {}

  /// @brief Construct a Client from environment variables.
  /// @return A Client configured from @c OPENAI_API_KEY, etc.
  static Client
  from_env ()
  {
    return Client (openapipp::low::from_env ());
  }

  /// @brief List available models.
  /// @return JSON array of model objects.
  std::string
  models_list_json ()
  {
    return openapipp::low::models_list (http_);
  }

  /// @brief Create a response via the Responses API.
  /// @param body_json JSON request body.
  /// @return A ResponseText wrapping the JSON response.
  ResponseText
  responses_create_json (std::string body_json)
  {
    return { openapipp::low::responses_create (http_, std::move (body_json)) };
  }

  /// @brief Create a chat completion.
  /// @param body_json JSON request body with model and messages.
  /// @return A ResponseText wrapping the JSON response.
  ResponseText
  chat_completions_create_json (std::string body_json)
  {
    return { openapipp::low::chat_completions_create (http_,
                                                    std::move (body_json)) };
  }

  /// @brief Create embeddings for input text.
  /// @param body_json JSON request body with model and input.
  /// @return A ResponseText wrapping the JSON response.
  ResponseText
  embeddings_create_json (std::string body_json)
  {
    return { openapipp::low::embeddings_create (http_, std::move (body_json)) };
  }

  /// @brief List uploaded files.
  /// @return JSON array of file objects.
  std::string
  files_list_json ()
  {
    return openapipp::low::files_list (http_);
  }

  /// @brief Delete an uploaded file.
  /// @param file_id The file identifier.
  /// @return JSON deletion confirmation.
  std::string
  files_delete_json (std::string_view file_id)
  {
    return openapipp::low::files_delete (http_, file_id);
  }

  /// @brief Access the underlying HTTP client (const).
  /// @return Const reference to the HttpClient.
  const HttpClient &
  http () const
  {
    return http_;
  }
  /// @brief Access the underlying HTTP client (mutable).
  /// @return Mutable reference to the HttpClient.
  HttpClient &
  http ()
  {
    return http_;
  }

private:
  HttpClient http_;
};

} // namespace openapipp::high

/// @brief Fluent DSL for composing OpenAI API calls.
///
/// Built on DSLtk, this namespace provides a composable, pipe-based DSL for
/// making API calls. Use @ref openapipp::dsl::OpenAI as the base DSL type
/// and chain operations with `operator|`. The DSL supports:
/// - @c dsl::Pipeline for pipe-based chaining.
/// - @c dsl::Operators for composable predicates.
/// - @c dsl::PatternMatch for compile-time dispatch.
/// - @c dsl::Result<T,E> for monadic error handling.
namespace openapipp::dsl
{

namespace dsl = ::dsl;

/// @brief Base DSL type for composing OpenAI API calls.
///
/// Inherits @c dsl::Pipeline for pipe-based chaining. Add more DSLtk
/// feature mixins to the template argument list to extend capabilities.
struct OpenAI : dsl::DSL<OpenAI, dsl::Pipeline>
{
};

/// @brief Pipe stage that lists available models.
/// @return A pipe stage that, when applied to a Client, returns the models JSON.
inline constexpr auto
models_list ()
{
  return dsl::pipe ([] (openapipp::high::Client &c)
                      { return c.models_list_json (); });
}

/// @brief Pipe stage that creates a response.
/// @param body_json JSON request body.
/// @return A pipe stage that, when applied to a Client, returns the response JSON.
inline constexpr auto
responses_create (std::string body_json)
{
  return dsl::pipe ([b = std::move (body_json)] (openapipp::high::Client &c)
                      { return c.responses_create_json (b).json; });
}

} // namespace openapipp::dsl
