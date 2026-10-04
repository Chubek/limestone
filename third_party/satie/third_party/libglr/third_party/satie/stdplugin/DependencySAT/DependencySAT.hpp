#pragma once

#include "SatiePlugin.hpp"

namespace satie::stdplugin
{
class DependencySAT final : public Plugin
{
public:
  PluginInfo info () const override
  {
    return {"dependency-sat", "1.0.0", "Check a simple dependency selection with Satie."};
  }

  void install (PluginHost &host) override
  {
    host.register_command ("dependency.sat", [] (const PluginArguments &args) {
      if (args.empty ())
        throw std::invalid_argument (
            "dependency.sat expects one or more DIMACS clauses without trailing zeroes");
      CNF cnf;
      for (std::size_t clause_index = 0; clause_index < args.size (); ++clause_index)
        {
          const auto &clause_text = args[clause_index];
          Clause clause;
          std::istringstream input (clause_text);
          std::string token;
          while (input >> token)
            {
              Lit literal = 0;
              const auto [end, error] =
                  std::from_chars (token.data (), token.data () + token.size (), literal);
              if (error != std::errc{} || end != token.data () + token.size () || literal == 0)
                throw std::invalid_argument (
                    "dependency.sat clause " + std::to_string (clause_index + 1) +
                    " contains an invalid literal");
              clause.push_back (literal);
            }
          if (clause.empty ())
            throw std::invalid_argument (
                "dependency.sat clauses must contain at least one literal");
          cnf.add_clause (std::move (clause));
        }
      return solve_cdcl (cnf).satisfiable () ? std::string ("SAT") : std::string ("UNSAT");
    });
  }
};
} // namespace satie::stdplugin
