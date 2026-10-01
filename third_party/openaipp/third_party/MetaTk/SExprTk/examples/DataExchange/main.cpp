#include "SExprTk.hpp"
#include <iostream>

int main() {
    sexprtk::SExprTk rt;

    auto source = sexprtk::Source::from_string(
        "(exchange (id 1) (payload \"ok\") (meta (version 2)))");
    auto cartable = rt.parse(source);

    std::cout << "sexpr: " << sexprtk::Serializer::to_string(cartable.root) << '\n';
    std::cout << "json:  " << sexprtk::Serializer::to_json(cartable.root) << '\n';

    sexprtk::ShapeAnalyzer shape;
    auto analysis = shape.analyze(cartable);
    std::cout << "atoms: " << analysis.get_count("atoms") << '\n';
    std::cout << "lists: " << analysis.get_count("lists") << '\n';
}
