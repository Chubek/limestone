#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>

#include "QaMRpp.hpp"

static std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open file: " + path);
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}


static bool load_library_with_home_fallback(qamrpp::Context& ctx, const std::string& libname) {
    const char* home = std::getenv("HOME");
    const std::string primary = home ? (std::string(home) + "/.qamrpp/eqvsglib/" + libname) : "";
    const std::string fallback = "bin/" + libname;

    if (!primary.empty() && ctx.load_library(primary)) {
        return true;
    }

    return ctx.load_library(fallback);
}

static void print_usage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " <expr-definition.eq> <rewrite-rules.eq> <input.sexpr>\n\n"
        << "Description:\n"
        << "  Loads the QaMRpp native libraries (in order):\n"
        << "    - $HOME/.qamrpp/eqvsglib/libqamrpp-sexpr.so (fallback: bin/libqamrpp-sexpr.so)\n"
        << "    - $HOME/.qamrpp/eqvsglib/libqamrpp-leqvsg.so (fallback: bin/libqamrpp-leqvsg.so)\n"
        << "  Then evaluates QaMRpp code to load the expression definition,\n"
        << "  load rewrite rules, and rewrite the given S-expression input.\n";
}

int main(int argc, char** argv) {
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string expr_def_path = argv[1];
    const std::string rules_path    = argv[2];
    const std::string input_path    = argv[3];

    try {
        const std::string input_expr = read_file(input_path);

        qamrpp::Context ctx;

        if (!load_library_with_home_fallback(ctx, "libqamrpp-sexpr.so")) {
            std::cerr << "Failed to load library: $HOME/.qamrpp/eqvsglib/libqamrpp-sexpr.so or bin/libqamrpp-sexpr.so\n";
            return 1;
        }

        if (!load_library_with_home_fallback(ctx, "libqamrpp-leqvsg.so")) {
            std::cerr << "Failed to load library: $HOME/.qamrpp/eqvsglib/libqamrpp-leqvsg.so or bin/libqamrpp-leqvsg.so\n";
            return 1;
        }

        ctx.assign_name("__eqvsg_expr_def_path", std::make_shared<qamrpp::Value>(expr_def_path));
        ctx.assign_name("__eqvsg_rules_path", std::make_shared<qamrpp::Value>(rules_path));
        ctx.assign_name("__eqvsg_input_expr", std::make_shared<qamrpp::Value>(input_expr));

        std::ostringstream script;
        script
            << "eqvsg_load_expr_definition(__eqvsg_expr_def_path)\n"
            << "eqvsg_load_rewrite_rules(__eqvsg_rules_path)\n"
            << "return eqvsg_rewrite(__eqvsg_input_expr)\n";

        auto result = ctx.run(script.str());
        if (!result) {
            std::cerr << "EQVSG error: QaMRpp script returned null result\n";
            return 1;
        }

        std::cout << result->to_string() << '\n';
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "EQVSG error: " << ex.what() << '\n';
        return 1;
    }
}
