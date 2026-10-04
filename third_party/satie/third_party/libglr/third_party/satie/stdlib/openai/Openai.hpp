#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "json/Json.hpp"

namespace satie::stdlib::openai
{

struct Message
{
  std::string role;
  std::string content;
};

inline Message message (std::string role, std::string content)
{
  if (role != "system" && role != "user" && role != "assistant")
    throw std::invalid_argument ("openai::message role must be system, user, or assistant");
  return { std::move (role), std::move (content) };
}

/// Builds a `/v1/chat/completions` request body (no network involved;
/// send it with any HTTP client).
inline std::string chat_payload (const std::string &model,
                                 const std::vector<Message> &messages,
                                 std::optional<double> temperature = std::nullopt,
                                 std::optional<int> max_tokens = std::nullopt)
{
  json::Array items;
  for (const Message &message : messages)
    {
      json::Object fields;
      fields.emplace ("role", json::Value::string_value (message.role));
      fields.emplace ("content", json::Value::string_value (message.content));
      items.push_back (json::Value::object_value (std::move (fields)));
    }
  json::Object root;
  root.emplace ("model", json::Value::string_value (model));
  root.emplace ("messages", json::Value::array_value (std::move (items)));
  if (temperature.has_value ())
    root.emplace ("temperature", json::Value::number_value (*temperature));
  if (max_tokens.has_value ())
    root.emplace ("max_tokens",
                  json::Value::number_value (static_cast<double> (*max_tokens)));
  return json::dump (json::Value::object_value (std::move (root)));
}

/// Extracts `choices[0].message.content` from a chat-completion response.
/// Returns empty when the shape is unexpected.
inline std::string first_choice_text (const std::string &response)
{
  try
    {
      json::Value root = json::parse (response);
      if (root.kind != json::Value::Kind::Object || root.fields == nullptr)
        return {};
      auto choices = root.fields->find ("choices");
      if (choices == root.fields->end () || choices->second.kind != json::Value::Kind::Array ||
          choices->second.items == nullptr || choices->second.items->empty ())
        return {};
      const json::Value &first = choices->second.items->front ();
      if (first.kind != json::Value::Kind::Object || first.fields == nullptr)
        return {};
      auto message = first.fields->find ("message");
      if (message == first.fields->end () || message->second.kind != json::Value::Kind::Object ||
          message->second.fields == nullptr)
        return {};
      auto content = message->second.fields->find ("content");
      if (content == message->second.fields->end () ||
          content->second.kind != json::Value::Kind::String)
        return {};
      return content->second.text;
    }
  catch (const std::exception &)
    {
      return {};
    }
}

} // namespace satie::stdlib::openai
