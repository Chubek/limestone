#include "Common.hpp"

#include <charconv>

namespace satie
{

CNF DimacsParser::parse ()
{
  CNF cnf;
  Clause current;
  std::optional<std::size_t> variables, clause_count;
  std::string line;
  std::size_t line_no = 0;
  bool saw_data = false;
  while (std::getline (input_, line))
    {
      ++line_no;
      std::size_t position = first_non_ws (line);
      if (position == std::string::npos || line[position] == 'c') continue;
      if (line[position] == '%') break;
      std::vector<std::pair<std::string_view, std::size_t>> tokens;
      while (position < line.size ())
        {
          while (position < line.size () && std::isspace (static_cast<unsigned char> (line[position])))
            ++position;
          if (position == line.size ()) break;
          const auto start = position;
          while (position < line.size () && !std::isspace (static_cast<unsigned char> (line[position])))
            ++position;
          tokens.emplace_back (std::string_view (line).substr (start, position - start), start + 1);
        }
      auto integer = [&] (std::size_t index) {
        const auto [token, column] = tokens[index];
        std::int64_t value = 0;
        const auto result = std::from_chars (token.data (), token.data () + token.size (), value);
        if (result.ec != std::errc{} || result.ptr != token.data () + token.size ())
          throw ParseError (line_no, column, "invalid integer");
        return value;
      };
      if (tokens[0].first == "p")
        {
          if (variables || saw_data)
            throw ParseError (line_no, tokens[0].second, "problem line must precede clauses");
          if (tokens.size () != 4 || tokens[1].first != "cnf")
            throw ParseError (line_no, tokens[0].second, "expected 'p cnf <vars> <clauses>'");
          const auto vars = integer (2), count = integer (3);
          if (vars < 0 || vars > std::numeric_limits<Var>::max () || count < 0)
            throw ParseError (line_no, tokens[2].second, "invalid problem counts");
          variables = static_cast<std::size_t> (vars);
          clause_count = static_cast<std::size_t> (count);
          continue;
        }
      saw_data = true;
      for (std::size_t i = 0; i < tokens.size (); ++i)
        {
          const auto raw = integer (i);
          if (raw <= std::numeric_limits<Lit>::min () || raw > std::numeric_limits<Lit>::max ())
            throw ParseError (line_no, tokens[i].second, "literal magnitude exceeds int32 variable range");
          const auto lit = static_cast<Lit> (raw);
          if (lit == 0)
            {
              cnf.add_clause (std::move (current));
              current.clear ();
            }
          else
            {
              if (variables && static_cast<std::size_t> (literal_var (lit)) > *variables)
                throw ParseError (line_no, tokens[i].second, "literal exceeds declared variable count");
              current.push_back (lit);
            }
        }
    }
  if (input_.bad ()) throw std::runtime_error ("failed to read DIMACS input");
  if (!current.empty ()) throw ParseError (line_no, 1, "unterminated clause; missing 0");
  if (clause_count && cnf.clause_count () != *clause_count)
    throw ParseError (line_no, 1, "clause count does not match problem line");
  if (variables) cnf.set_declared_variable_count (*variables);
  return cnf;
}

namespace
{
constexpr char kLibraryVersion[] = "0.1.0";
}

const char *satie_library_version () noexcept { return kLibraryVersion; }

std::string satie_library_build_info ()
{
  std::string info = std::string ("satie ") + kLibraryVersion;
#if defined(__clang__)
  info += " clang/" + std::string (__clang_version__);
#elif defined(__GNUC__)
  info += " gcc/" + std::to_string (__GNUC__) + "." + std::to_string (__GNUC_MINOR__);
#endif
  info += " cxx20";
  return info;
}

} // namespace satie
