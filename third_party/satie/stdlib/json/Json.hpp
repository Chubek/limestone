#pragma once

#include <cctype>
#include <cstdio>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace satie::stdlib::json
{

struct Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;

/// JSON value: null, bool, number, string, array, or object.
struct Value
{
  enum class Kind
  {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
  };

  Kind kind = Kind::Null;
  bool boolean = false;
  double number = 0.0;
  std::string text;
  std::shared_ptr<Array> items;
  std::shared_ptr<Object> fields;

  static Value null () { return Value{}; }
  static Value boolean_value (bool value)
  {
    Value out;
    out.kind = Kind::Bool;
    out.boolean = value;
    return out;
  }
  static Value number_value (double value)
  {
    Value out;
    out.kind = Kind::Number;
    out.number = value;
    return out;
  }
  static Value string_value (std::string value)
  {
    Value out;
    out.kind = Kind::String;
    out.text = std::move (value);
    return out;
  }
  static Value array_value (Array value)
  {
    Value out;
    out.kind = Kind::Array;
    out.items = std::make_shared<Array> (std::move (value));
    return out;
  }
  static Value object_value (Object value)
  {
    Value out;
    out.kind = Kind::Object;
    out.fields = std::make_shared<Object> (std::move (value));
    return out;
  }
};

inline std::string escape_string (const std::string &text)
{
  std::string out = "\"";
  for (char c : text)
    switch (c)
      {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
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
        if (static_cast<unsigned char> (c) < 0x20)
          {
            char hex[7];
            std::snprintf (hex, sizeof (hex), "\\u%04x", c);
            out += hex;
          }
        else
          out.push_back (c);
      }
  return out + "\"";
}

inline std::string dump (const Value &value)
{
  switch (value.kind)
    {
    case Value::Kind::Null:
      return "null";
    case Value::Kind::Bool:
      return value.boolean ? "true" : "false";
    case Value::Kind::Number:
      {
        std::ostringstream oss;
        oss << value.number;
        return oss.str ();
      }
    case Value::Kind::String:
      return escape_string (value.text);
    case Value::Kind::Array:
      {
        std::string out = "[";
        for (std::size_t i = 0; i < value.items->size (); ++i)
          {
            if (i != 0)
              out += ",";
            out += dump ((*value.items)[i]);
          }
        return out + "]";
      }
    case Value::Kind::Object:
      {
        std::string out = "{";
        bool first = true;
        for (const auto &[key, item] : *value.fields)
          {
            if (!first)
              out += ",";
            first = false;
            out += escape_string (key) + ":" + dump (item);
          }
        return out + "}";
      }
  }
  return "null";
}

class Parser
{
public:
  explicit Parser (const std::string &text) : text_ (text) {}

  Value parse ()
  {
    skip_ws ();
    Value value = parse_value ();
    skip_ws ();
    if (pos_ != text_.size ())
      throw std::invalid_argument ("json::parse trailing content");
    return value;
  }

private:
  Value parse_value ()
  {
    if (match ('{'))
      return parse_object ();
    if (match ('['))
      return parse_array ();
    if (peek () == '"')
      return Value::string_value (parse_string ());
    if (peek () == 't' || peek () == 'f' || peek () == 'n')
      return parse_literal ();
    return parse_number ();
  }

  Value parse_object ()
  {
    Object fields;
    skip_ws ();
    if (match ('}'))
      return Value::object_value (std::move (fields));
    while (true)
      {
        skip_ws ();
        if (peek () != '"')
          throw std::invalid_argument ("json::parse expected an object key");
        std::string key = parse_string ();
        skip_ws ();
        if (!match (':'))
          throw std::invalid_argument ("json::parse expected ':'");
        skip_ws ();
        fields.emplace (std::move (key), parse_value ());
        skip_ws ();
        if (match (','))
          continue;
        if (match ('}'))
          return Value::object_value (std::move (fields));
        throw std::invalid_argument ("json::parse expected ',' or '}'");
      }
  }

  Value parse_array ()
  {
    Array items;
    skip_ws ();
    if (match (']'))
      return Value::array_value (std::move (items));
    while (true)
      {
        skip_ws ();
        items.push_back (parse_value ());
        skip_ws ();
        if (match (','))
          continue;
        if (match (']'))
          return Value::array_value (std::move (items));
        throw std::invalid_argument ("json::parse expected ',' or ']'");
      }
  }

  std::string parse_string ()
  {
    if (!match ('"'))
      throw std::invalid_argument ("json::parse expected a string");
    std::string out;
    while (pos_ < text_.size () && text_[pos_] != '"')
      {
        if (text_[pos_] == '\\')
          {
            ++pos_;
            if (pos_ >= text_.size ())
              break;
            switch (text_[pos_++])
              {
              case '"':
                out.push_back ('"');
                break;
              case '\\':
                out.push_back ('\\');
                break;
              case 'n':
                out.push_back ('\n');
                break;
              case 'r':
                out.push_back ('\r');
                break;
              case 't':
                out.push_back ('\t');
                break;
              default:
                throw std::invalid_argument ("json::parse bad escape");
              }
          }
        else
          out.push_back (text_[pos_++]);
      }
    if (!match ('"'))
      throw std::invalid_argument ("json::parse unterminated string");
    return out;
  }

  Value parse_literal ()
  {
    if (text_.compare (pos_, 4, "true") == 0)
      {
        pos_ += 4;
        return Value::boolean_value (true);
      }
    if (text_.compare (pos_, 5, "false") == 0)
      {
        pos_ += 5;
        return Value::boolean_value (false);
      }
    if (text_.compare (pos_, 4, "null") == 0)
      {
        pos_ += 4;
        return Value::null ();
      }
    throw std::invalid_argument ("json::parse bad literal");
  }

  Value parse_number ()
  {
    std::size_t start = pos_;
    if (peek () == '-')
      ++pos_;
    while (pos_ < text_.size () &&
           (std::isdigit (static_cast<unsigned char> (text_[pos_])) != 0 ||
            text_[pos_] == '.' || text_[pos_] == 'e' || text_[pos_] == 'E' ||
            text_[pos_] == '+' || text_[pos_] == '-'))
      ++pos_;
    try
      {
        return Value::number_value (std::stod (text_.substr (start, pos_ - start)));
      }
    catch (const std::exception &)
      {
        throw std::invalid_argument ("json::parse bad number");
      }
  }

  void skip_ws ()
  {
    while (pos_ < text_.size () &&
           std::isspace (static_cast<unsigned char> (text_[pos_])) != 0)
      ++pos_;
  }
  char peek () const { return pos_ < text_.size () ? text_[pos_] : '\0'; }
  bool match (char c)
  {
    if (peek () == c)
      {
        ++pos_;
        return true;
      }
    return false;
  }

  std::string text_;
  std::size_t pos_ = 0;
};

inline Value parse (const std::string &text) { return Parser (text).parse (); }

} // namespace satie::stdlib::json
