// markdown_pp.cpp
// Markdown preprocessor with template variables and includes

#include "ekipp.hpp"
#include "directive_bank.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.md> [output.md]\n";
        return 1;
    }

    try {
        // Read input file
        std::ifstream input_file(argv[1]);
        if (!input_file) {
            std::cerr << "Error: Cannot open input file: " << argv[1] << "\n";
            return 1;
        }
        
        std::stringstream buffer;
        buffer << input_file.rdbuf();
        std::string input = buffer.str();
        input_file.close();

        // Configure preprocessor
        ekipp::Configuration config;
        config.params.include_dirs.push_back(".");
        config.params.include_dirs.push_back("templates/");
        
        ekipp::Preprocessor pp(config);
        ekipp::directive_bank::register_all(pp.registry());

        // Add custom directives for markdown
        auto toc_entry = ekipp::Directive::fluent()
            << ekipp::Directive::Name("toc")
            << ekipp::Directive::Description("Generate TOC entry")
            << ekipp::Directive::NumParams(2)
            << ekipp::Directive::Semantics([](ekipp::Arguments& args, ekipp::Context&) {
                std::string title = args.raw(0);
                std::string anchor = args.raw(1);
                return "- [" + title + "](#" + anchor + ")\n";
            });
        
        pp.registry().registerDirective(toc_entry);

        // Set source location
        ekipp::SourceLocation loc;
        loc.source_name = argv[1];

        // Process
        std::string output = pp.process(input, loc.source_name);

        // Write output
        if (argc >= 3) {
            std::ofstream output_file(argv[2]);
            if (!output_file) {
                std::cerr << "Error: Cannot write to output file: " << argv[2] << "\n";
                return 1;
            }
            output_file << output;
            output_file.close();
            std::cout << "Processed: " << argv[1] << " -> " << argv[2] << "\n";
        } else {
            std::cout << output;
        }

        return 0;

    } catch (const ekipp::DirectiveError& e) {
        std::cerr << "Preprocessing error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
