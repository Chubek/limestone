#include "ekipp.hpp"
#include "directive_bank.hpp"
#include <algorithm>
#include <iostream>

int main() {
    ekipp::Preprocessor pp;
    ekipp::directive_bank::register_all(pp.registry());

    auto upper = ekipp::Directive::fluent()
        << ekipp::Directive::Name("upper")
        << ekipp::Directive::NumParams(1)
        << ekipp::Directive::Semantics(
            [](ekipp::Arguments& args, ekipp::Context&) {
                std::string s = args.raw(0);
                std::transform(s.begin(), s.end(), s.begin(), ::toupper);
                return s;
            });

    pp.registry().registerDirective(upper);

    std::string input = "Result: @upper(hello world)@";
    std::cout << pp.process(input);
}
