#pragma once

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "Common.hpp"

namespace satie::stdlib::latex
{

inline std::string escape (std::string_view text)
{
  std::string out;
  for (char c : text)
    switch (c)
      {
      case '\\':
        out += R"(\textbackslash{})";
        break;
      case '{':
        out += R"(\{)";
        break;
      case '}':
        out += R"(\})";
        break;
      case '$':
        out += R"(\$)";
        break;
      case '&':
        out += R"(\&)";
        break;
      case '%':
        out += R"(\%)";
        break;
      case '#':
        out += R"(\#)";
        break;
      case '_':
        out += R"(\_)";
        break;
      case '~':
        out += R"(\textasciitilde{})";
        break;
      case '^':
        out += R"(\textasciicircum{})";
        break;
      default:
        out.push_back (c);
      }
  return out;
}

inline std::string math (const std::string &body) { return "\\(" + body + "\\)"; }

inline std::string frac (const std::string &num, const std::string &den)
{
  return "\\frac{" + num + "}{" + den + "}";
}

inline std::string env (const std::string &name, const std::string &body)
{
  return "\\begin{" + name + "}\n" + body + "\\end{" + name + "}\n";
}

inline std::string document (const std::string &body, const std::string &title = {})
{
  std::string head = "\\documentclass{article}\n";
  if (!title.empty ())
    head += "\\title{" + escape (title) + "}\n";
  return head + "\\begin{document}\n" + body + "\\end{document}\n";
}

/// Renders a CNF as a math-mode conjunction of disjunctions.
inline std::string cnf_to_latex (const CNF &cnf)
{
  std::string out;
  for (std::size_t i = 0; i < cnf.clauses ().size (); ++i)
    {
      if (i != 0)
        out += " \\land ";
      out += "(";
      for (std::size_t j = 0; j < cnf.clauses ()[i].size (); ++j)
        {
          if (j != 0)
            out += " \\lor ";
          const Lit lit = cnf.clauses ()[i][j];
          if (lit < 0)
            out += "\\lnot ";
          out += "x_{" + std::to_string (literal_var (lit)) + "}";
        }
      out += ")";
    }
  return math (out);
}

} // namespace satie::stdlib::latex
