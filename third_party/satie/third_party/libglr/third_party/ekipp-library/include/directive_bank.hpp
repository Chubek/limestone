// directive_bank.hpp - Standard Directive Library for EKIPP
// A collection of commonly-used preprocessor directives
// Version: 2.0.0
// License: MIT

#ifndef EKIPP_DIRECTIVE_BANK_HPP
#define EKIPP_DIRECTIVE_BANK_HPP

#include "ekipp.hpp"
#include <array>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <cmath>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace ekipp {
namespace directive_bank {

// ============================================================================
// Utility functions
// ============================================================================

namespace detail {

inline std::string exec_command(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;

#ifdef _WIN32
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
#else
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
#endif

    if (!pipe) {
        throw DirectiveError("failed to execute command: " + cmd);
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

inline std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw DirectiveError("cannot open file: " + path.string());
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

inline std::string format_time(const std::string& format_str) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;

#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    std::array<char, 256> buffer;
    std::strftime(buffer.data(), buffer.size(), format_str.c_str(), &tm_now);
    return std::string(buffer.data());
}

inline std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }

    auto end = s.end();
    do {
        --end;
    } while (std::distance(start, end) > 0 && std::isspace(static_cast<unsigned char>(*end)));

    return std::string(start, end + 1);
}

// Simple expression evaluator for eval directive
inline double evaluate_expression(const std::string& expr) {
    // Very simple evaluator - supports +, -, *, /, (), numbers
    // This is a placeholder - a real implementation would use a proper parser
    std::string cleaned = trim(expr);
    
    // Try to parse as a simple number first
    try {
        size_t pos;
        double result = std::stod(cleaned, &pos);
        if (pos == cleaned.length()) {
            return result;
        }
    } catch (...) {
        // Not a simple number, would need full expression parsing
    }
    
    throw DirectiveError("eval: complex expressions not yet implemented");
}

} // namespace detail

// ============================================================================
// include - Include and process external file
// ============================================================================

inline Directive make_include() {
    return Directive::fluent()
        << Directive::Name("include")
        << Directive::Description("Include and process an external file")
        << Directive::Category("file")
        << Directive::NumParams(1)
        << Directive::IO("r")
        << Directive::Arg(directives::arg("path", "string", false))
        << Directive::Semantics([](Arguments& args, Context& ctx) -> std::string {
            auto path_str = args.raw(0);
            std::filesystem::path path(path_str);

            // Try relative to current source
            if (!path.is_absolute() && !ctx.location.source_name.empty()) {
                auto parent = std::filesystem::path(ctx.location.source_name).parent_path();
                if (!parent.empty()) {
                    auto candidate = parent / path;
                    if (std::filesystem::exists(candidate)) {
                        path = candidate;
                    }
                }
            }

            // Try include directories
            if (!std::filesystem::exists(path) && ctx.configuration) {
                for (const auto& inc_dir : ctx.configuration->params.include_dirs) {
                    auto candidate = inc_dir / path_str;
                    if (std::filesystem::exists(candidate)) {
                        path = candidate;
                        break;
                    }
                }
            }

            if (!std::filesystem::exists(path)) {
                throw DirectiveError("include file not found: " + path_str);
            }

            auto content = detail::read_file(path);

            SourceLocation nested_loc = ctx.location;
            nested_loc.source_name = path.string();
            nested_loc.line = 1;
            nested_loc.column = 1;

            return ctx.preprocessor ? ctx.preprocessor->process(content, nested_loc.source_name) : content;
        });
}

// ============================================================================
// define - Define a macro in the symbol table
// ============================================================================

inline Directive make_define() {
    return Directive::fluent()
        << Directive::Name("define")
        << Directive::Description("Define an object-like or function-like macro")
        << Directive::Category("macro")
        << Directive::MinParams(2)
        << Directive::IO("w")
        << Directive::Semantics([](Arguments& args, Context& ctx) -> std::string {
            if (!ctx.symbols) {
                throw DirectiveError("define requires an active symbol table");
            }

            std::string signature = detail::trim(args.raw(0));
            std::string body = args.raw(1);

            auto open = signature.find('(');
            auto close = signature.rfind(')');

            if (open == std::string::npos || close == std::string::npos || close < open) {
                // Object-like macro
                ctx.symbols->define_object(signature, body);
                return std::string();
            }

            // Function-like macro
            std::string name = detail::trim(signature.substr(0, open));
            std::string params_str = signature.substr(open + 1, close - open - 1);
            
            std::vector<std::string> parameters;
            bool variadic = false;
            
            std::stringstream ss(params_str);
            std::string item;
            while (std::getline(ss, item, ',')) {
                item = detail::trim(item);
                if (item.empty()) continue;
                
                if (item == "...") {
                    variadic = true;
                    parameters.push_back("__VA_ARGS__");
                } else if (item.size() >= 3 && item.substr(item.size() - 3) == "...") {
                    variadic = true;
                    parameters.push_back(detail::trim(item.substr(0, item.size() - 3)));
                } else {
                    parameters.push_back(item);
                }
            }

            ctx.symbols->define_function(name, parameters, body, variadic);
            return std::string();
        });
}

// ============================================================================
// date - Insert current date/time
// ============================================================================

inline Directive make_date() {
    return Directive::fluent()
        << Directive::Name("date")
        << Directive::Description("Insert current date/time with optional format")
        << Directive::Category("utility")
        << Directive::MaxParams(1)
        << Directive::IO("r")
        << Directive::Semantics([](Arguments& args, Context&) -> std::string {
            std::string format = args.size() > 0 ? args.raw(0) : "%Y-%m-%d %H:%M:%S";
            return detail::format_time(format);
        });
}

// ============================================================================
// exec - Execute shell command and insert output
// ============================================================================

inline Directive make_exec() {
    return Directive::fluent()
        << Directive::Name("exec")
        << Directive::Description("Execute shell command and insert output")
        << Directive::Category("system")
        << Directive::NumParams(1)
        << Directive::IO("r")
        << Directive::Arg(directives::arg("command", "string", false))
        << Directive::Semantics([](Arguments& args, Context&) -> std::string {
            return detail::exec_command(args.raw(0));
        });
}

// ============================================================================
// eval - Evaluate numeric expression
// ============================================================================

inline Directive make_eval() {
    return Directive::fluent()
        << Directive::Name("eval")
        << Directive::Description("Evaluate numeric expression")
        << Directive::Category("math")
        << Directive::NumParams(1)
        << Directive::IO("r")
        << Directive::Arg(directives::arg("expression", "string", false))
        << Directive::Semantics([](Arguments& args, Context&) -> std::string {
            double result = detail::evaluate_expression(args.raw(0));
            std::ostringstream oss;
            oss << result;
            return oss.str();
        });
}

// ============================================================================
// env - Get environment variable
// ============================================================================

inline Directive make_env() {
    return Directive::fluent()
        << Directive::Name("env")
        << Directive::Description("Get environment variable value")
        << Directive::Category("system")
        << Directive::MinParams(1)
        << Directive::MaxParams(2)
        << Directive::IO("r")
        << Directive::Semantics([](Arguments& args, Context&) -> std::string {
            const char* value = std::getenv(args.raw(0).c_str());
            if (value) {
                return std::string(value);
            }
            return args.size() > 1 ? args.raw(1) : std::string();
        });
}

// ============================================================================
// Register all directives
// ============================================================================

inline void register_all(DirectiveRegistry& registry) {
    registry.registerDirective(make_include());
    registry.registerDirective(make_define());
    registry.registerDirective(make_date());
    registry.registerDirective(make_exec());
    registry.registerDirective(make_eval());
    registry.registerDirective(make_env());
}

inline std::vector<Directive> all() {
    return {
        make_include(),
        make_define(),
        make_date(),
        make_exec(),
        make_eval(),
        make_env()
    };
}

} // namespace directive_bank
} // namespace ekipp

#endif // EKIPP_DIRECTIVE_BANK_HPP
