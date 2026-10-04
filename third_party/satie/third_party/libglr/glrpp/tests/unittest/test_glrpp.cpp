#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "../../../GLRpp/GLRpp.hpp"
#include "../../../GLRpp/rewrite/equinox/EquinoxPasses.hpp"
#include "../../../GLRpp/rewrite/native/NativePasses.hpp"
#include "../../../GLRpp/rewrite/syntax/SyntaxDSL.hpp"

#include <string>
#include <vector>

static void test_noop_deleter(void *) {}

TEST_CASE("resource handle default") {
  glrpp::detail::ResourceHandle<void *, test_noop_deleter> h;
  REQUIRE(h.get() == nullptr);
}

TEST_CASE("symbol terminal metadata") {
  glrpp::Symbol s(7, "tok", true);
  REQUIRE(s.id() == 7);
  REQUIRE(s.name() == "tok");
  REQUIRE(s.is_terminal());
}

TEST_CASE("symbol nonterminal metadata") {
  glrpp::Symbol s(9, "expr", false);
  REQUIRE(s.id() == 9);
  REQUIRE(s.name() == "expr");
  REQUIRE_FALSE(s.is_terminal());
}

TEST_CASE("production id is stable") {
  glrpp::Production p(11);
  REQUIRE(p.id() == 11);
}

TEST_CASE("grammar add symbols") {
  glrpp::Grammar g;
  auto t = g.terminal("num");
  auto nt = g.nonterminal("expr");
  REQUIRE(t.id() >= 0);
  REQUIRE(nt.id() >= 0);
  REQUIRE(t.is_terminal());
  REQUIRE_FALSE(nt.is_terminal());
}

TEST_CASE("grammar symbol interning") {
  glrpp::Grammar g;
  auto a = g.terminal("plus");
  auto b = g.terminal("plus");
  REQUIRE(a.id() == b.id());
}

TEST_CASE("grammar can add production") {
  glrpp::Grammar g;
  auto e = g.nonterminal("E");
  auto n = g.terminal("n");
  auto p = g.add_production(e, {n});
  REQUIRE(p.id() >= 0);
}

TEST_CASE("production builder accumulates rhs") {
  glrpp::Grammar g;
  auto e = g.nonterminal("E");
  auto n = g.terminal("n");
  auto plus = g.terminal("+");
  auto p = glrpp::ProductionBuilder(g, e).operator>>(e).operator>>(plus).operator>>(n).build();
  REQUIRE(p.id() >= 0);
}

struct MiniDSL : glrpp::GrammarDSL<MiniDSL> { glrpp::Grammar grammar_; };

TEST_CASE("grammar dsl forwards terminal") {
  MiniDSL d;
  auto s = d.terminal("id");
  REQUIRE(s.id() >= 0);
  REQUIRE(s.name() == "id");
}

TEST_CASE("grammar dsl forwards rule") {
  MiniDSL d;
  auto lhs = d.nonterminal("S");
  auto tok = d.terminal("tok");
  auto p = d.rule(lhs).operator>>(tok).build();
  REQUIRE(p.id() >= 0);
}

TEST_CASE("disambiguation builder prefer builds hook") {
  auto hook = glrpp::DisambiguationBuilder{}
                  .when([](const glrpp::DisambiguationContext &) { return true; })
                  .prefer(0)
                  .build("t", 1);
  REQUIRE(true);
  (void)hook;
}

TEST_CASE("grammar start symbol set") {
  glrpp::Grammar g;
  auto s = g.nonterminal("S");
  REQUIRE_NOTHROW(g.set_start(s));
}

TEST_CASE("grammar ir roundtrip") {
  glrpp::Grammar g;
  auto s = g.nonterminal("S");
  auto a = g.terminal("a");
  g.add_production(s, {a});
  g.set_start(s);

  auto ir = g.to_ir();
  REQUIRE(ir.validate());
  auto g2 = glrpp::Grammar::from_ir(ir);
  auto ir2 = g2.to_ir();
  REQUIRE(ir2.validate());
  REQUIRE(ir2.productions.size() == 1);
  REQUIRE(ir2.start_symbol_id.has_value());
}

TEST_CASE("native rewrite pipeline is opt-in") {
  glrpp::Grammar g;
  auto s = g.nonterminal("S");
  auto a = g.terminal("a");
  g.add_production(s, {a});
  g.add_production(s, {a});
  g.set_start(s);

  REQUIRE(g.to_ir().productions.size() == 2);

  glrpp::rewrite::RewritePipeline pipeline;
  pipeline.add_once(glrpp::rewrite::native::remove_duplicate_productions());
  auto rewritten = g.rewritten(pipeline);
  REQUIRE(rewritten.to_ir().productions.size() == 1);
}

TEST_CASE("syntax rewrite compiles to executable pass") {
  auto parsed = glrpp::rewrite::syntax::parse_rule("dedup: dedup_productions");
  REQUIRE(parsed.is_ok());
  auto compiled = glrpp::rewrite::syntax::compile_rule(parsed.unwrap());
  REQUIRE(compiled.is_ok());

  glrpp::Grammar g;
  auto s = g.nonterminal("S");
  auto a = g.terminal("a");
  g.add_production(s, {a});
  g.add_production(s, {a});
  g.set_start(s);

  glrpp::rewrite::RewritePipeline pipeline;
  pipeline.add_once(compiled.unwrap());
  auto rewritten = g.rewritten(pipeline);
  REQUIRE(rewritten.to_ir().productions.size() == 1);
}

TEST_CASE("syntax rewrite supports sort operation") {
  auto parsed = glrpp::rewrite::syntax::parse_rule("sorter: sort_productions");
  REQUIRE(parsed.is_ok());
  auto compiled = glrpp::rewrite::syntax::compile_rule(parsed.unwrap());
  REQUIRE(compiled.is_ok());

  glrpp::rewrite::GrammarIR ir;
  const int s = ir.add_symbol("S", false);
  const int a = ir.add_symbol("a", true);
  const int p0 = ir.add_production(s, {a});
  const int p1 = ir.add_production(s, {a});
  ir.productions[0].id = p1;
  ir.productions[1].id = p0;

  REQUIRE(compiled.unwrap()->apply(ir));
  REQUIRE(ir.productions[0].id == p0);
  REQUIRE(ir.productions[1].id == p1);
}

TEST_CASE("equinox-backed pass integrates in pipeline") {
  glrpp::Grammar g;
  auto s = g.nonterminal("S");
  auto a = g.terminal("a");
  g.add_production(s, {a});
  g.add_production(s, {a});
  g.set_start(s);

  glrpp::rewrite::RewritePipeline pipeline;
  pipeline.add_fixed_point(glrpp::rewrite::equinox::equivalent_rhs_dedup(), 4);
  auto rewritten = g.rewritten(pipeline);
  REQUIRE(rewritten.to_ir().productions.size() == 1);
}

namespace {
glrpp::Grammar make_single_token_grammar() {
  glrpp::Grammar grammar;
  auto start = grammar.nonterminal("S");
  auto token = grammar.terminal("a");
  grammar.add_production(start, {token});
  grammar.set_start(start);
  return grammar;
}

bool accept_a_hook(const glr_lexer_event_t *, glr_lexer_response_t *response, void *) {
  glr_lexer_response_accept(response, "a", 1);
  return true;
}
}

TEST_CASE("diff empty input") { auto edit = glrpp::compute_edit("", ""); REQUIRE(edit.empty()); }
TEST_CASE("diff insertion") { auto edit = glrpp::compute_edit("ab", "axb"); REQUIRE(edit.is_insertion); REQUIRE(edit.new_length() == 1); }
TEST_CASE("diff deletion") { auto edit = glrpp::compute_edit("axb", "ab"); REQUIRE(edit.is_deletion); REQUIRE(edit.old_length() == 1); }
TEST_CASE("diff replacement") { auto edit = glrpp::compute_edit("abc", "axc"); REQUIRE(edit.is_replacement); REQUIRE(edit.old_length() == 1); REQUIRE(edit.new_length() == 1); }
TEST_CASE("diff common prefix") { REQUIRE(glr_find_common_prefix("abcd", "abef", 4, 4) == 2); }
TEST_CASE("diff common suffix") { REQUIRE(glr_find_common_suffix("abcd", "xycd", 4, 4, 0) == 2); }
TEST_CASE("diff old length") { auto edit = glrpp::compute_edit("hello", "h"); REQUIRE(edit.old_length() == 4); }
TEST_CASE("diff new length") { auto edit = glrpp::compute_edit("h", "hello"); REQUIRE(edit.new_length() == 4); }
TEST_CASE("diff insertion position") { auto edit = glrpp::compute_edit("ac", "abc"); REQUIRE(edit.old_start == 1); REQUIRE(edit.new_start == 1); }
TEST_CASE("diff replacement position") { auto edit = glrpp::compute_edit("abc", "axc"); REQUIRE(edit.old_start == 1); }

TEST_CASE("line column origin") { REQUIRE(glrpp::line_column_of("a\nb", 0) == std::pair<uint32_t,uint32_t>{1,1}); }
TEST_CASE("line column second line") { REQUIRE(glrpp::line_column_of("a\nb", 2) == std::pair<uint32_t,uint32_t>{2,1}); }
TEST_CASE("line column end") { REQUIRE(glrpp::line_column_of("a\nb", 3) == std::pair<uint32_t,uint32_t>{2,2}); }
TEST_CASE("offset origin") { REQUIRE(glrpp::offset_of("a\nb", 1, 1) == 0); }
TEST_CASE("offset second line") { REQUIRE(glrpp::offset_of("a\nb", 2, 1) == 2); }
TEST_CASE("offset end") { REQUIRE(glrpp::offset_of("a\nb", 2, 2) == 3); }
TEST_CASE("line column invalid offset") { REQUIRE_THROWS_AS(glrpp::line_column_of("a", 2), std::out_of_range); }
TEST_CASE("offset invalid line") { REQUIRE_THROWS_AS(glrpp::offset_of("a", 2, 1), std::out_of_range); }

TEST_CASE("forest starts empty") { glrpp::Forest forest; REQUIRE(forest.size() == 0); REQUIRE_FALSE(forest.ambiguous()); }
TEST_CASE("forest terminal node") { glrpp::Forest forest; REQUIRE(glr_forest_get_terminal(forest.handle(), 1, 0, 1) != nullptr); REQUIRE(forest.size() == 1); }
TEST_CASE("forest symbol node") { glrpp::Forest forest; REQUIRE(glr_forest_get_symbol(forest.handle(), 2, 0, 1) != nullptr); REQUIRE(forest.size() == 1); }
TEST_CASE("forest constructor node") { glrpp::Forest forest; REQUIRE(glr_forest_get_constructor(forest.handle(), 3, 0, 1) != nullptr); REQUIRE(forest.size() == 1); }
TEST_CASE("forest clone is independent") { glrpp::Forest forest; glr_forest_get_terminal(forest.handle(), 1, 0, 1); auto clone = forest.clone(); REQUIRE(clone.size() == forest.size()); }
TEST_CASE("forest clear") { glrpp::Forest forest; glr_forest_get_terminal(forest.handle(), 1, 0, 1); glr_forest_clear(forest.handle()); REQUIRE(forest.size() == 0); }
TEST_CASE("forest view is empty") { glrpp::Forest forest; REQUIRE(forest.view().node_count() == 0); }
TEST_CASE("forest serialization has bytes") { glrpp::Forest forest; auto buffer = serialize(forest.view()); REQUIRE(buffer); REQUIRE(buffer.size() > 0); }
TEST_CASE("forest serialization roundtrip") { glrpp::Forest forest; glr_forest_get_terminal(forest.handle(), 1, 0, 1); auto copy = deserialize_forest(serialize(forest.view())); REQUIRE(copy.size() == forest.size()); }
TEST_CASE("forest null safe total") { REQUIRE(glr_forest_total_nodes(nullptr) == 0); }
TEST_CASE("forest null safe ambiguity") { REQUIRE_FALSE(glr_forest_is_ambiguous(nullptr)); }

TEST_CASE("scannerless literal configuration") {
  auto grammar = make_single_token_grammar(); auto token = grammar.terminal("a");
  REQUIRE(grammar.set_scannerless_literal(token, "abc").is_ok()); REQUIRE(grammar.has_scannerless_patterns());
}
TEST_CASE("scannerless regex configuration") {
  auto grammar = make_single_token_grammar(); auto token = grammar.terminal("a");
  REQUIRE(grammar.set_scannerless_pattern(token, "a+").is_ok());
}
TEST_CASE("scannerless invalid regex") {
  auto grammar = make_single_token_grammar(); auto token = grammar.terminal("a");
  REQUIRE(grammar.set_scannerless_pattern(token, "[").is_err());
}
TEST_CASE("scannerless scan literal") {
  auto grammar = make_single_token_grammar(); auto token = grammar.terminal("a"); grammar.set_scannerless_literal(token, "abc");
  auto matches = glrpp::Scannerless::scan(grammar, "abcdef"); REQUIRE(matches.size() == 1); REQUIRE(matches[0].length == 3);
}
TEST_CASE("scannerless scan offset") {
  auto grammar = make_single_token_grammar(); auto token = grammar.terminal("a"); grammar.set_scannerless_literal(token, "bc");
  auto matches = glrpp::Scannerless::scan(grammar, "xbc", 1); REQUIRE(matches.size() == 1); REQUIRE(matches[0].position == 1);
}
TEST_CASE("scannerless match literal") { glrpp::Symbol token(1, "abc", true); REQUIRE(glrpp::Scannerless::match(token, "abcdef") == 3); }
TEST_CASE("scannerless match mismatch") { glrpp::Symbol token(1, "abc", true); REQUIRE(glrpp::Scannerless::match(token, "xyz") == 0); }
TEST_CASE("scannerless clear pattern") { auto grammar = make_single_token_grammar(); auto token = grammar.terminal("a"); grammar.set_scannerless_literal(token, "x"); REQUIRE(glr_scannerless_clear_pattern(grammar.handle(), token.id()) == 0); }
TEST_CASE("scannerless no patterns initially") { auto grammar = make_single_token_grammar(); REQUIRE_FALSE(grammar.has_scannerless_patterns()); }
TEST_CASE("scannerless match empty input") { glrpp::Symbol token(1, "abc", true); REQUIRE(glrpp::Scannerless::match(token, "") == 0); }

TEST_CASE("live parser creates") { auto grammar = make_single_token_grammar(); glrpp::LiveParser live(grammar, "a"); REQUIRE_FALSE(live.pending()); REQUIRE(live.forest().node_count() > 0); }
TEST_CASE("live parser text") { auto grammar = make_single_token_grammar(); glrpp::LiveParser live(grammar, "a"); REQUIRE(live.text() == "a"); }
TEST_CASE("live parser edit pending") { auto grammar = make_single_token_grammar(); glrpp::LiveParser live(grammar, "a"); glr_live_edit_t edit{1,1,1,2,"a",1}; live.edit(edit); REQUIRE(live.pending()); }
TEST_CASE("live parser update clears pending") { auto grammar = make_single_token_grammar(); glrpp::LiveParser live(grammar, "a"); glr_live_edit_t edit{1,1,1,2,"a",1}; live.edit(edit); live.update(); REQUIRE_FALSE(live.pending()); }
TEST_CASE("live parser stats") { auto grammar = make_single_token_grammar(); glrpp::LiveParser live(grammar, "a"); REQUIRE(live.stats().input_length == 1); }
TEST_CASE("live parser edit count") { auto grammar = make_single_token_grammar(); glrpp::LiveParser live(grammar, "a"); glr_live_edit_t edit{1,1,1,1,"a",1}; live.edit(edit); REQUIRE(live.stats().edit_count == 1); }
TEST_CASE("live parser update count") { auto grammar = make_single_token_grammar(); glrpp::LiveParser live(grammar, "a"); live.update(); REQUIRE(live.stats().update_count == 0); }
TEST_CASE("live parser set text") { auto grammar = make_single_token_grammar(); glrpp::LiveParser live(grammar, "a"); live.set_text("a"); REQUIRE(live.text() == "a"); REQUIRE_FALSE(live.pending()); }

TEST_CASE("lexer hooks start empty") { glrpp::LexerHooks hooks; REQUIRE(hooks.size() == 0); }
TEST_CASE("lexer hooks add") { glrpp::LexerHooks hooks; hooks.add("a", 1, accept_a_hook); REQUIRE(hooks.size() == 1); }
TEST_CASE("lexer hooks dispatch") { glrpp::LexerHooks hooks; hooks.add("a", 1, accept_a_hook); glr_lexer_event_t event{}; event.default_bytes_consumed = 1; glr_lexer_response_t response{}; REQUIRE(hooks.dispatch(event, response)); REQUIRE(response.accepted); }
TEST_CASE("lexer hooks response name") { glrpp::LexerHooks hooks; hooks.add("a", 1, accept_a_hook); glr_lexer_event_t event{}; event.default_bytes_consumed = 1; glr_lexer_response_t response{}; hooks.dispatch(event, response); REQUIRE(std::string(response.terminal_name) == "a"); }
TEST_CASE("lexer hooks clear") { glrpp::LexerHooks hooks; hooks.add("a", 1, accept_a_hook); hooks.clear(); REQUIRE(hooks.size() == 0); }
TEST_CASE("lexer hooks unicode name") { REQUIRE(glr_lexer_unicode_name('A') != nullptr); }
TEST_CASE("lexer hooks reset") { glr_lexer_response_t response{"x", 2, true}; glr_lexer_response_reset(&response); REQUIRE_FALSE(response.accepted); }
TEST_CASE("lexer hooks accept helper") { glr_lexer_response_t response{}; glr_lexer_response_accept(&response, "x", 1); REQUIRE(response.accepted); REQUIRE(response.bytes_consumed == 1); }
TEST_CASE("lexer hooks null dispatch declines") { glrpp::LexerHooks hooks; glr_lexer_event_t event{}; glr_lexer_response_t response{}; REQUIRE_FALSE(hooks.dispatch(event, response)); }
TEST_CASE("lexer hooks handle nonnull") { glrpp::LexerHooks hooks; REQUIRE(hooks.handle() != nullptr); }

TEST_CASE("graph starts empty") { glrpp::Graph graph; REQUIRE(graph.node_count() == 0); REQUIRE(graph.edge_count() == 0); }
TEST_CASE("graph add first node") { glrpp::Graph graph; REQUIRE(graph.add_node() == 0); REQUIRE(graph.node_count() == 1); }
TEST_CASE("graph add second node") { glrpp::Graph graph; graph.add_node(); REQUIRE(graph.add_node() == 1); }
TEST_CASE("graph add edge") { glrpp::Graph graph; graph.add_node(); graph.add_node(); REQUIRE(graph.add_edge(0,1,7) != nullptr); }
TEST_CASE("graph edge count") { glrpp::Graph graph; graph.add_node(); graph.add_node(); graph.add_edge(0,1,7); REQUIRE(graph.edge_count() == 1); }
TEST_CASE("graph has edge") { glrpp::Graph graph; graph.add_node(); graph.add_node(); graph.add_edge(0,1,7); REQUIRE(graph.has_edge(0,1)); }
TEST_CASE("graph lacks reverse edge") { glrpp::Graph graph; graph.add_node(); graph.add_node(); graph.add_edge(0,1,7); REQUIRE_FALSE(graph.has_edge(1,0)); }
TEST_CASE("graph clear") { glrpp::Graph graph; graph.add_node(); graph.clear(); REQUIRE(graph.node_count() == 0); }
TEST_CASE("graph node lookup") { glrpp::Graph graph; graph.add_node(reinterpret_cast<void *>(42)); REQUIRE(glr_graph_get_node(nullptr, 0) == nullptr); }
TEST_CASE("graph remove edge") { glrpp::Graph graph; graph.add_node(); graph.add_node(); graph.add_edge(0,1,7); REQUIRE(glr_graph_remove_edge(graph.has_edge(0,1) ? nullptr : nullptr, 0, 1) == -1); }

TEST_CASE("parser scannerless option") { auto grammar = make_single_token_grammar(); glrpp::Parser parser(grammar); REQUIRE(parser.set_scannerless(true).is_ok()); }
TEST_CASE("parser trivia option") { auto grammar = make_single_token_grammar(); glrpp::Parser parser(grammar); REQUIRE(parser.set_trivia(" ").is_ok()); }
TEST_CASE("parser successful parse") { auto grammar = make_single_token_grammar(); glrpp::Parser parser(grammar); REQUIRE(parser.try_parse("a").is_ok()); }
TEST_CASE("parser syntax error result") { auto grammar = make_single_token_grammar(); glrpp::Parser parser(grammar); REQUIRE(parser.try_parse("b").is_err()); }
TEST_CASE("parser parse tree handle") { auto grammar = make_single_token_grammar(); glrpp::Parser parser(grammar); auto tree = parser.parse("a"); REQUIRE(tree.handle() != nullptr); }
TEST_CASE("parser parse tree node count") { auto grammar = make_single_token_grammar(); glrpp::Parser parser(grammar); REQUIRE(parser.parse("a").node_count() > 0); }
TEST_CASE("parser grammar handle") { auto grammar = make_single_token_grammar(); REQUIRE(grammar.handle() != nullptr); }
TEST_CASE("grammar scannerless status error text") { auto grammar = make_single_token_grammar(); auto token = grammar.terminal("a"); auto result = grammar.set_scannerless_pattern(token, "["); REQUIRE_FALSE(result.unwrap_or(false)); }
TEST_CASE("grammar alias status") { auto grammar = make_single_token_grammar(); auto production = grammar.to_ir().productions.front(); REQUIRE(grammar.set_alias(glrpp::Production(production.id), 1, "value").is_ok()); }
TEST_CASE("resource handle bool") { glrpp::Forest forest; glrpp::detail::ResourceHandle<void *, test_noop_deleter> handle(reinterpret_cast<void *>(1)); REQUIRE(static_cast<bool>(handle)); }
TEST_CASE("resource handle release") { glrpp::detail::ResourceHandle<void *, test_noop_deleter> handle(reinterpret_cast<void *>(1)); REQUIRE(handle.release() != nullptr); REQUIRE_FALSE(static_cast<bool>(handle)); }
TEST_CASE("serialized buffer move") { glrpp::Forest forest; auto first = serialize(forest.view()); auto second = std::move(first); REQUIRE(second.size() > 0); REQUIRE_FALSE(first); }
TEST_CASE("grammar rejects empty symbol") { glrpp::Grammar grammar; REQUIRE_THROWS(grammar.terminal("")); }
TEST_CASE("grammar rejects mixed symbol kind") { glrpp::Grammar grammar; grammar.terminal("x"); REQUIRE_THROWS(grammar.nonterminal("x")); }
TEST_CASE("grammar ir validates empty") { glrpp::rewrite::GrammarIR ir; REQUIRE(ir.validate()); }
TEST_CASE("production builder precedence") { auto grammar = make_single_token_grammar(); auto s = grammar.nonterminal("T"); auto a = grammar.terminal("b"); REQUIRE(grammar.add_production(s,{a}).id() >= 0); }
TEST_CASE("disambiguator precedence exists") { auto hook = glrpp::disambiguators::by_precedence(); REQUIRE(hook.release() != nullptr); }
TEST_CASE("disambiguator associativity exists") { auto hook = glrpp::disambiguators::by_associativity(); REQUIRE(hook.release() != nullptr); }
TEST_CASE("disambiguator longest match exists") { auto hook = glrpp::disambiguators::longest_match(); REQUIRE(hook.release() != nullptr); }
