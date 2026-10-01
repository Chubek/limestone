#pragma once

#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace satie::stdlib::html
{

inline std::string escape (std::string_view text)
{
  std::string out;
  for (char c : text)
    switch (c)
      {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      default:
        out.push_back (c);
      }
  return out;
}

inline std::string tag (const std::string &name, const std::string &body,
                        const std::map<std::string, std::string> &attrs = {})
{
  std::string out = "<" + name;
  for (const auto &[key, value] : attrs)
    out += " " + key + "=\"" + escape (value) + "\"";
  return out + ">" + body + "</" + name + ">";
}

inline std::string table (const std::vector<std::string> &headers,
                          const std::vector<std::vector<std::string>> &rows)
{
  std::string head;
  for (const std::string &header : headers)
    head += tag ("th", escape (header));
  std::string body;
  for (const std::vector<std::string> &row : rows)
    {
      std::string cells;
      for (const std::string &cell : row)
        cells += tag ("td", escape (cell));
      body += tag ("tr", cells);
    }
  return tag ("table", tag ("tr", head) + body);
}

inline std::string page (const std::string &title, const std::string &body)
{
  return "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><title>" + escape (title) +
         "</title></head><body>" + body + "</body></html>\n";
}

} // namespace satie::stdlib::html
