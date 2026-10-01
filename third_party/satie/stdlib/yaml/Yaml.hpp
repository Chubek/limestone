#pragma once

#include <cctype>
#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace satie::stdlib::yaml
{

struct Value;
using Sequence = std::vector<Value>;
using Mapping = std::vector<std::pair<std::string, Value>>;

/// YAML subset value: null, bool, integer, double, string, sequence, or
/// mapping. The parser accepts block mappings/sequences with space
/// indentation, `#` comments, quoted and plain scalars; flow collections
/// (`[]`/`{}`) are rejected. Emitted output is canonical block style.
struct Value
{
  enum class Kind
  {
    Null,
    Bool,
    Integer,
    Double,
    String,
    Sequence,
    Mapping
  };

  Kind kind = Kind::Null;
  bool boolean = false;
  std::int64_t integer = 0;
  double number = 0.0;
  std::string text;
  std::shared_ptr<Sequence> items;
  std::shared_ptr<Mapping> fields;

  static Value null () { return Value{}; }
  static Value boolean_value (bool value)
  {
    Value out;
    out.kind = Kind::Bool;
    out.boolean = value;
    return out;
  }
  static Value integer_value (std::int64_t value)
  {
    Value out;
    out.kind = Kind::Integer;
    out.integer = value;
    return out;
  }
  static Value double_value (double value)
  {
    Value out;
    out.kind = Kind::Double;
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
  static Value sequence_value (Sequence value)
  {
    Value out;
    out.kind = Kind::Sequence;
    out.items = std::make_shared<Sequence> (std::move (value));
    return out;
  }
  static Value mapping_value (Mapping value)
  {
    Value out;
    out.kind = Kind::Mapping;
    out.fields = std::make_shared<Mapping> (std::move (value));
    return out;
  }
};

inline std::string dump_scalar (const Value &value)
{
  switch (value.kind)
    {
    case Value::Kind::Null:
      return "null";
    case Value::Kind::Bool:
      return value.boolean ? "true" : "false";
    case Value::Kind::Integer:
      return std::to_string (value.integer);
    case Value::Kind::Double:
      {
        std::ostringstream oss;
        oss << value.number;
        return oss.str ();
      }
    case Value::Kind::String:
      {
        // Quote when the scalar could misparse (leading/trailing space,
        // special leading characters, or embedded ": ").
        const bool needs_quotes =
            value.text.empty () || value.text.front () == ' ' ||
            value.text.back () == ' ' || value.text.find (": ") != std::string::npos ||
            value.text.find (" #") != std::string::npos ||
            std::string ("-?:,[]{}&*!|>'\"%@`").find (value.text.front ()) !=
                std::string::npos;
        if (!needs_quotes)
          return value.text;
        std::string out = "'";
        for (char c : value.text)
          out += c == '\'' ? "''" : std::string (1, c);
        return out + "'";
      }
    default:
      throw std::invalid_argument ("yaml::dump_scalar needs a scalar");
    }
}

inline void dump_into (const Value &value, std::string &out, int indent);

inline void dump_indent (std::string &out, int indent)
{
  out.append (static_cast<std::size_t> (indent), ' ');
}

inline void dump_into (const Value &value, std::string &out, int indent)
{
  switch (value.kind)
    {
    case Value::Kind::Sequence:
      if (value.items->empty ())
        {
          out += "[]\n";
          return;
        }
      for (const Value &item : *value.items)
        {
          dump_indent (out, indent);
          out += "-";
          if (item.kind == Value::Kind::Sequence || item.kind == Value::Kind::Mapping)
            {
              out += "\n";
              dump_into (item, out, indent + 2);
            }
          else
            out += " " + dump_scalar (item) + "\n";
        }
      return;
    case Value::Kind::Mapping:
      if (value.fields->empty ())
        {
          out += "{}\n";
          return;
        }
      for (const auto &[key, item] : *value.fields)
        {
          dump_indent (out, indent);
          out += key + ":";
          if (item.kind == Value::Kind::Sequence || item.kind == Value::Kind::Mapping)
            {
              out += "\n";
              dump_into (item, out, indent + 2);
            }
          else
            out += " " + dump_scalar (item) + "\n";
        }
      return;
    default:
      out += dump_scalar (value) + "\n";
    }
}

inline std::string dump (const Value &value)
{
  std::string out;
  dump_into (value, out, 0);
  return out;
}

class Parser
{
public:
  explicit Parser (const std::string &text) { split (text); }

  Value parse ()
  {
    skip_blank ();
    if (pos_ >= lines_.size ())
      return Value::null ();
    Value value = parse_block (0);
    skip_blank ();
    if (pos_ < lines_.size ())
      throw std::invalid_argument ("yaml::parse trailing content");
    return value;
  }

private:
  struct Line
  {
    int indent = 0;
    std::string text;
  };

  void split (const std::string &text)
  {
    std::string current;
    auto flush = [&] {
      std::string no_comment = strip_comment (current);
      std::size_t indent = 0;
      while (indent < no_comment.size () && no_comment[indent] == ' ')
        ++indent;
      std::string body = no_comment.substr (indent);
      while (!body.empty () && (body.back () == ' ' || body.back () == '\t'))
        body.pop_back ();
      if (!body.empty () && body != "---" && body != "...")
        lines_.push_back ({ static_cast<int> (indent), body });
      current.clear ();
    };
    for (char c : text)
      if (c == '\n')
        flush ();
      else
        current.push_back (c);
    flush ();
  }

  static std::string strip_comment (const std::string &line)
  {
    bool single = false, dbl = false;
    for (std::size_t i = 0; i < line.size (); ++i)
      {
        const char c = line[i];
        if (c == '\'' && !dbl)
          single = !single;
        else if (c == '"' && !single)
          dbl = !dbl;
        else if (c == '#' && !single && !dbl && i > 0 && line[i - 1] == ' ')
          return line.substr (0, i);
      }
    return line;
  }

  void skip_blank ()
  {
    while (pos_ < lines_.size () && lines_[pos_].text.empty ())
      ++pos_;
  }

  Value parse_block (int indent)
  {
    if (pos_ >= lines_.size () || lines_[pos_].indent != indent)
      throw std::invalid_argument ("yaml::parse bad indentation");
    if (lines_[pos_].text.rfind ("- ", 0) == 0 || lines_[pos_].text == "-")
      return parse_sequence (indent);
    return parse_mapping (indent);
  }

  Value parse_sequence (int indent)
  {
    Sequence items;
    while (pos_ < lines_.size () && lines_[pos_].indent == indent &&
           (lines_[pos_].text.rfind ("- ", 0) == 0 || lines_[pos_].text == "-"))
      {
        std::string rest = lines_[pos_].text.size () > 2 ? lines_[pos_].text.substr (2) : "";
        ++pos_;
        if (rest.empty ())
          {
            skip_blank ();
            items.push_back (parse_block (indent + 2));
          }
        else
          items.push_back (parse_scalar (rest));
      }
    return Value::sequence_value (std::move (items));
  }

  Value parse_mapping (int indent)
  {
    Mapping fields;
    while (pos_ < lines_.size () && lines_[pos_].indent == indent &&
           lines_[pos_].text.rfind ("- ", 0) != 0)
      {
        const std::string line = lines_[pos_].text;
        const std::size_t colon = find_colon (line);
        if (colon == std::string::npos)
          throw std::invalid_argument ("yaml::parse expected 'key: value'");
        std::string key = line.substr (0, colon);
        std::string rest = line.substr (colon + 1);
        while (!rest.empty () && rest.front () == ' ')
          rest.erase (rest.begin ());
        ++pos_;
        if (rest.empty ())
          {
            skip_blank ();
            if (pos_ < lines_.size () && lines_[pos_].indent > indent)
              fields.emplace_back (key, parse_block (lines_[pos_].indent));
            else
              fields.emplace_back (key, Value::null ());
          }
        else
          fields.emplace_back (key, parse_scalar (rest));
      }
    return Value::mapping_value (std::move (fields));
  }

  static std::size_t find_colon (const std::string &line)
  {
    bool single = false, dbl = false;
    for (std::size_t i = 0; i < line.size (); ++i)
      {
        const char c = line[i];
        if (c == '\'' && !dbl)
          single = !single;
        else if (c == '"' && !single)
          dbl = !dbl;
        else if (c == ':' && !single && !dbl &&
                 (i + 1 >= line.size () || line[i + 1] == ' '))
          return i;
      }
    return std::string::npos;
  }

  static Value parse_scalar (const std::string &text)
  {
    if (text == "null" || text == "~" || text.empty ())
      return Value::null ();
    if (text == "true")
      return Value::boolean_value (true);
    if (text == "false")
      return Value::boolean_value (false);
    if (text.size () >= 2 && text.front () == '\'' && text.back () == '\'')
      {
        std::string out;
        for (std::size_t i = 1; i + 1 < text.size (); ++i)
          if (text[i] == '\'' && text[i + 1] == '\'')
            {
              out.push_back ('\'');
              ++i;
            }
          else
            out.push_back (text[i]);
        return Value::string_value (out);
      }
    if (text.size () >= 2 && text.front () == '"' && text.back () == '"')
      return Value::string_value (text.substr (1, text.size () - 2));
    try
      {
        std::size_t used = 0;
        const long long integer = std::stoll (text, &used);
        if (used == text.size ())
          return Value::integer_value (integer);
      }
    catch (const std::exception &)
      {
      }
    try
      {
        std::size_t used = 0;
        const double number = std::stod (text, &used);
        if (used == text.size ())
          return Value::double_value (number);
      }
    catch (const std::exception &)
      {
      }
    return Value::string_value (text);
  }

  std::vector<Line> lines_;
  std::size_t pos_ = 0;
};

inline Value parse (const std::string &text) { return Parser (text).parse (); }

} // namespace satie::stdlib::yaml
