// fuzz target 5
#include "../../../GLRpp/GLRpp.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  glrpp::Grammar g;
  auto lhs = g.nonterminal("S");
  auto t = g.terminal("t");
  g.set_start(lhs);
  if (size > 0) {
    std::vector<glrpp::Symbol> rhs;
    rhs.reserve(size % 8 + 1);
    for (size_t i = 0; i < (size % 8 + 1); ++i) {
      rhs.push_back((data[i % size] & 1) ? lhs : t);
    }
    try { (void)g.add_production(lhs, rhs); } catch (...) {}
  }
  auto h = glrpp::disambiguators::by_precedence();
  (void)h;
  return 0;
}
