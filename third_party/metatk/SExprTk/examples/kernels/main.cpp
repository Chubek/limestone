#include "SExprTk.hpp"
#include <iostream>

int main() {
    sexprtk::SExprTk rt;
    auto source = sexprtk::Source::from_string("(+ 1 2) (* 6 7) \"hello world\"");

    std::cout << "input: " << rt.run(source) << '\n';

    // Kernels give the program MEANING by evaluating it in a real
    // runtime. Lua via QaMRpp, Scheme via S7.
    sexprtk::LuaKernel lua;
    sexprtk::S7Kernel s7;

    std::cout << "lua kernel meaning: " << rt.run(source, lua) << '\n';
    std::cout << "s7 kernel meaning:  " << rt.run(source, s7) << '\n';

    // Semanticizers do the same through the Semanticizer interface,
    // returning the full Semantics (denoted value + rendering).
    sexprtk::LuaKernelSemanticizer lua_sem;
    sexprtk::S7KernelSemanticizer s7_sem;

    auto arithmetic = sexprtk::Source::from_string("(+ 40 2)");
    auto lua_meaning = rt.semanticize(arithmetic, lua_sem);
    auto s7_meaning = rt.semanticize(arithmetic, s7_sem);

    if (lua_meaning.ok()) {
        std::cout << "lua semanticizer:   " << lua_meaning.rendered
                  << " (denotation is int=" << lua_meaning.value.as_int() << ")\n";
    } else {
        std::cout << "lua semanticizer:   unavailable: "
                  << (lua_meaning.errors.empty() ? "?" : lua_meaning.errors.front()) << '\n';
    }
    if (s7_meaning.ok()) {
        std::cout << "s7 semanticizer:    " << s7_meaning.rendered
                  << " (denotation is int=" << s7_meaning.value.as_int() << ")\n";
    } else {
        std::cout << "s7 semanticizer:    unavailable: "
                  << (s7_meaning.errors.empty() ? "?" : s7_meaning.errors.front()) << '\n';
    }

    // Analyzer + Transformer passes through their abstract interfaces.
    sexprtk::ShapeAnalyzer shape;
    sexprtk::ConstantFoldTransformer fold;
    auto analysis = rt.analyze(arithmetic, shape);
    std::cout << "analysis: atoms=" << analysis.get_count("atoms")
              << " lists=" << analysis.get_count("lists") << '\n';
    auto folded = rt.transform(sexprtk::Source::from_string("(+ 1 2 3)"), fold);
    std::cout << "constant-folded:    " << folded.to_string() << '\n';
}
