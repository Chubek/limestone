#include <nuperf/nuperf-api.h>

#include <CLI/CLI.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

nuperf_status_t add_key(nuperf_keyset_t *keyset, const std::string &line) {
    if (line.empty()) {
        return NUPERF_OK;
    }
    return nuperf_keyset_add_string(keyset, line.c_str());
}

int cmd_build(const std::string &input_path,
              const std::string &output_path,
              const std::string &method,
              const std::string &target,
              const std::vector<std::string> &options) {
    nuperf_keyset_t *keyset = nullptr;
    nuperf_table_t *table = nullptr;

    nuperf_status_t st = nuperf_keyset_create(&keyset);
    if (st != NUPERF_OK) {
        std::cerr << "keyset_create failed: " << nuperf_strerror(st) << "\n";
        return 1;
    }

    st = nuperf_table_create(&table);
    if (st != NUPERF_OK) {
        std::cerr << "table_create failed: " << nuperf_strerror(st) << "\n";
        nuperf_keyset_destroy(keyset);
        return 1;
    }

    auto cleanup = [&]() {
        nuperf_table_destroy(table);
        nuperf_keyset_destroy(keyset);
    };

    std::istream *in = &std::cin;
    std::ifstream input_file;
    if (!input_path.empty() && input_path != "-") {
        input_file.open(input_path);
        if (!input_file) {
            std::cerr << "failed to open input file: " << input_path << "\n";
            cleanup();
            return 1;
        }
        in = &input_file;
    }

    std::string line;
    while (std::getline(*in, line)) {
        st = add_key(keyset, line);
        if (st != NUPERF_OK) {
            std::cerr << "failed adding key: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
    }

    st = nuperf_table_set_keyset(table, keyset);
    if (st != NUPERF_OK) {
        std::cerr << "set_keyset failed: " << nuperf_strerror(st) << "\n";
        cleanup();
        return 1;
    }

    if (!method.empty()) {
        st = nuperf_table_set_method(table, method.c_str());
        if (st != NUPERF_OK) {
            std::cerr << "set_method failed: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
    }

    if (!target.empty()) {
        st = nuperf_table_set_target(table, target.c_str());
        if (st != NUPERF_OK) {
            std::cerr << "set_target failed: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
    }

    for (const std::string &opt : options) {
        const auto pos = opt.find('=');
        if (pos == std::string::npos) {
            std::cerr << "invalid option format (expected key=value): " << opt << "\n";
            cleanup();
            return 1;
        }
        st = nuperf_table_set_option(table, opt.substr(0, pos).c_str(), opt.substr(pos + 1).c_str());
        if (st != NUPERF_OK) {
            std::cerr << "set_option failed for '" << opt << "': " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
    }

    st = nuperf_table_build(table);
    if (st != NUPERF_OK) {
        std::cerr << "build failed: " << nuperf_strerror(st) << "\n";
        cleanup();
        return 1;
    }

    if (output_path.empty() || output_path == "-") {
        size_t size = 0;
        st = nuperf_table_emit_buffer(table, nullptr, &size);
        if (st != NUPERF_OK && st != NUPERF_ERR_BUFFER_TOO_SMALL) {
            std::cerr << "emit size query failed: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
        std::string out(size, '\0');
        st = nuperf_table_emit_buffer(table, out.data(), &size);
        if (st != NUPERF_OK) {
            std::cerr << "emit buffer failed: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
        std::cout.write(out.data(), static_cast<std::streamsize>(size));
    } else {
        st = nuperf_table_emit_file(table, output_path.c_str());
        if (st != NUPERF_OK) {
            std::cerr << "emit file failed: " << nuperf_strerror(st) << "\n";
            cleanup();
            return 1;
        }
    }

    cleanup();
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    nuperf_status_t st = nuperf_init();
    if (st != NUPERF_OK) {
        std::cerr << "nuperf_init failed: " << nuperf_strerror(st) << "\n";
        return 1;
    }

    CLI::App app{"NuPERF minimal perfect hash tool"};
    app.require_subcommand(1);

    auto *version_cmd = app.add_subcommand("version", "Print version");
    auto *methods_cmd = app.add_subcommand("list-methods", "List registered methods");
    auto *targets_cmd = app.add_subcommand("list-targets", "List registered targets");

    std::string input_path;
    std::string output_path{"-"};
    std::string method;
    std::string target;
    std::vector<std::string> options;

    auto *build_cmd = app.add_subcommand("build", "Build and emit a table");
    build_cmd->add_option("-i,--input", input_path, "Input path, '-' or omitted for stdin");
    build_cmd->add_option("-o,--output", output_path, "Output path, '-' for stdout");
    build_cmd->add_option("-m,--method", method, "Method name");
    build_cmd->add_option("-t,--target", target, "Target name");
    build_cmd->add_option("--option", options, "Method/target option in key=value form");

    CLI11_PARSE(app, argc, argv);

    int rc = 0;
    if (*version_cmd) {
        const nuperf_version_t v = nuperf_version();
        std::cout << v.major << "." << v.minor << "." << v.patch;
        if (v.suffix && *v.suffix) {
            std::cout << "-" << v.suffix;
        }
        std::cout << "\n";
    } else if (*methods_cmd) {
        const size_t n = nuperf_method_count();
        for (size_t i = 0; i < n; ++i) {
            const char *name = nuperf_method_name(i);
            if (name) {
                std::cout << name << "\n";
            }
        }
    } else if (*targets_cmd) {
        const size_t n = nuperf_target_count();
        for (size_t i = 0; i < n; ++i) {
            const char *name = nuperf_target_name(i);
            if (name) {
                std::cout << name << "\n";
            }
        }
    } else if (*build_cmd) {
        rc = cmd_build(input_path, output_path, method, target, options);
    }

    nuperf_shutdown();
    return rc;
}
