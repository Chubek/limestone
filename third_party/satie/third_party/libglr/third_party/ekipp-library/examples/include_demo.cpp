#include "ekipp.hpp"
#include "directive_bank.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

int main() {
    ekipp::Configuration cfg;
    cfg.params.include_dirs.push_back("./templates");

    ekipp::Preprocessor pp(cfg);
    ekipp::directive_bank::register_all(pp.registry());

    std::string input = R"(
@include("header.txt")@

Main body content.

@include("footer.txt")@
)";

    std::cout << pp.process(input);
}
