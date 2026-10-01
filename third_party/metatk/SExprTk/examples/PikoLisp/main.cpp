#include "SExprTk.hpp"
#include <iostream>

static sexprtk::Atom eval_cell(const sexprtk::Cell& cell);

static sexprtk::Atom eval_list(const sexprtk::List& list) {
    if (list.empty()) return sexprtk::Atom{};
    const auto& head = list.front().head;
    if (head.is_symbol() && head.as_string() == "define") {
        return sexprtk::Atom(std::string("<defined>"), sexprtk::NodeKind::Symbol);
    }
    if (head.is_symbol() && head.as_string() == "list") {
        auto result = std::make_shared<sexprtk::List>();
        for (std::size_t i = 1; i < list.size(); ++i) result->push(eval_cell(list[i]));
        return sexprtk::Atom(result);
    }
    if (head.is_symbol() && head.as_string() == "+") {
        std::int64_t sum = 0;
        for (std::size_t i = 1; i < list.size(); ++i) sum += eval_cell(list[i]).as_int();
        return sexprtk::Atom(sum);
    }
    return sexprtk::Atom(std::string("<unevaluated>"), sexprtk::NodeKind::Symbol);
}

static sexprtk::Atom eval_cell(const sexprtk::Cell& cell) {
    if (cell.head.is_list()) return eval_list(cell.head.as_list());
    if (cell.head.is_symbol() && cell.head.as_string() == "quote") {
        if (!cell.tail.empty()) return cell.tail[0].head;
        return sexprtk::Atom{};
    }
    return cell.head;
}

int main() {
    sexprtk::SExprTk rt;
    auto source = sexprtk::Source::from_string(
        "(define (f x) (list x x)) (+ 1 2 3) (define (g) 'value)");
    auto cartable = rt.parse(source);

    std::cout << "parsed: " << sexprtk::Serializer::to_string(cartable.root) << "\n\n";
    for (const auto& cell : cartable.root.cells) {
        std::cout << "eval: " << sexprtk::Serializer::to_string(eval_cell(cell)) << '\n';
    }
}
