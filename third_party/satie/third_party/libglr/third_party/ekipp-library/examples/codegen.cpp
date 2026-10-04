#include "ekipp.hpp"
#include "directive_bank.hpp"
#include <iostream>

int main() {
    ekipp::Preprocessor pp;
    ekipp::directive_bank::register_all(pp.registry());

    std::string template_code = R"(

@define(FIELD(name,type), type name;)@

struct User {
@FIELD(id,int)
@FIELD(name,std::string)
@FIELD(age,int)
};

)";

    std::cout << pp.process(template_code);
}
