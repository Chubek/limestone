#include <string>
#include <vector>
#include <iostream>
#include "json_dom.hpp"

using Obj = std::vector<std::pair<std::string, std::any>>;
using Arr = std::vector<std::any>;

void dump(const std::any &v, int depth = 0) {
  std::string pad(2 * depth, ' ');
  if (auto o = std::any_cast<Obj>(&v)) {
    std::cout << pad << "{\n";
    for (auto &[k, val] : *o) {
      std::cout << pad << "  \"" << k << "\":\n";
      dump(val, depth + 2);
    }
    std::cout << pad << "}\n";
  } else if (auto a = std::any_cast<Arr>(&v)) {
    std::cout << pad << "[\n";
    for (auto &e : *a) dump(e, depth + 1);
    std::cout << pad << "]\n";
  } else if (auto s = std::any_cast<std::string>(&v)) {
    std::cout << pad << '"' << *s << "\"\n";
  } else if (auto d = std::any_cast<double>(&v)) {
    std::cout << pad << *d << "\n";
  } else if (auto b = std::any_cast<bool>(&v)) {
    std::cout << pad << (*b ? "true" : "false") << "\n";
  } else {
    std::cout << pad << "null\n";
  }
}

int main() {
  auto r = json_dom::parse(
      R"({"name": "Ada\tLovelace", "tags": ["math", "code"], "born": 1815, "alive": false, "note": null})");
  if (!r.ok || !r.full) { std::cerr << "parse failed at " << r.pos << "\n"; return 1; }
  dump(r.value);

  // Targeted lookup:
  auto &root = std::any_cast<const Obj &>(r.value);
  for (auto &[k, v] : root)
    if (k == "born")
      std::cout << "born = " << std::any_cast<double>(v) << "\n";
}
