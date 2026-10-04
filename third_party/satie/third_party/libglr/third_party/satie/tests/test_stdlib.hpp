#pragma once

#include "catch_shim.hpp"

#include "Satie.h"
#include "SatiePlugin.hpp"
#include "Stdlib.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#ifndef SATIE_STDLIB_DIR
#define SATIE_STDLIB_DIR "."
#endif

namespace
{
std::string read_text_file (const std::string &path)
{
  std::ifstream in (path);
  if (!in)
    throw std::runtime_error ("cannot open " + path);
  std::ostringstream buffer;
  buffer << in.rdbuf ();
  return buffer.str ();
}

/// Runs a stdlib Lua module with `snippet` appended (the module's trailing
/// `return` line is dropped so the snippet sees the module table).
std::string run_lua_module (const std::string &relative, const std::string &snippet)
{
  std::string code = read_text_file (std::string (SATIE_STDLIB_DIR) + "/" + relative);
  const std::string marker = "\nreturn ";
  const std::size_t pos = code.rfind (marker);
  if (pos != std::string::npos)
    code = code.substr (0, pos + 1);
  satie::PluginHost host;
  qamrpp::Context context;
  satie::LuaPlugin::install_library (context, host);
  auto value = context.run (code + "\n" + snippet);
  return value ? value->to_string () : "nil";
}
} // namespace

// ---- arith -----------------------------------------------------------------
TEST_CASE ("stdlib arith gcd and lcm") {
  REQUIRE (satie::stdlib::arith::gcd (12, 18) == 6);
  REQUIRE (satie::stdlib::arith::lcm (4, 6) == 12);
  REQUIRE (satie::stdlib::arith::gcd (0, 5) == 5);
}

TEST_CASE ("stdlib arith pow and factorial detect overflow") {
  REQUIRE (satie::stdlib::arith::pow_int (2, 10).value () == 1024);
  REQUIRE (!satie::stdlib::arith::pow_int (2, 63).has_value ());
  REQUIRE (satie::stdlib::arith::factorial (5).value () == 120);
  REQUIRE (!satie::stdlib::arith::factorial (21).has_value ());
}

TEST_CASE ("stdlib arith primality and clamp") {
  REQUIRE (satie::stdlib::arith::is_prime (13));
  REQUIRE (!satie::stdlib::arith::is_prime (15));
  REQUIRE (satie::stdlib::arith::clamp_val (9, 0, 5) == 5);
  REQUIRE (satie::stdlib::arith::sign_of (-3) == -1);
}

TEST_CASE ("stdlib arith lua") {
  REQUIRE (run_lua_module ("arith/arith.lua", "return Arith.gcd(12, 18)") == "6");
  REQUIRE (run_lua_module ("arith/arith.lua", "return Arith.lcm(4, 6)") == "12");
  REQUIRE (run_lua_module ("arith/arith.lua", "return Arith.fact(5)") == "120");
}

// ---- array -----------------------------------------------------------------
TEST_CASE ("stdlib array helpers") {
  const std::vector<int> items{ 1, 2, 2, 3 };
  REQUIRE (satie::stdlib::array::contains (items, 2));
  REQUIRE (satie::stdlib::array::index_of (items, 3) == 3);
  REQUIRE (satie::stdlib::array::remove_duplicates (items) == std::vector<int> ({ 1, 2, 3 }));
  REQUIRE (satie::stdlib::array::slice (items, 1, 3) == std::vector<int> ({ 2, 2 }));
}

TEST_CASE ("stdlib array lua") {
  REQUIRE (run_lua_module ("array/array.lua", "return Array.index_of({5, 6, 7}, 6)") == "2");
}

// ---- constants / sys / variables --------------------------------------------
TEST_CASE ("stdlib constants and sys versions agree") {
  REQUIRE (std::string (satie::stdlib::constants::version ()) == "0.1.0");
  REQUIRE (std::string (satie::stdlib::sys::version ()) == "0.1.0");
  REQUIRE (satie::stdlib::sys::cpu_count () >= 1);
  REQUIRE (satie::stdlib::sys::unixtime () > 0);
}

TEST_CASE ("stdlib variables supply with rollback") {
  satie::stdlib::variables::VarSupply supply (10);
  REQUIRE (supply.fresh () == 10);
  const std::int32_t point = supply.checkpoint ();
  supply.fresh ();
  supply.rollback (point);
  REQUIRE (supply.fresh () == 11);
}

TEST_CASE ("stdlib sys and constants lua") {
  REQUIRE (run_lua_module ("sys/sys.lua", "return Sys.version_string()") == "0.1.0");
  REQUIRE (run_lua_module ("constants/constants.lua", "return Constants.pi > 3") == "true");
  REQUIRE (run_lua_module ("variables/variables.lua",
                           "local s = Variables.new(10) Variables.fresh(s) "
                           "Variables.rollback(s, 10) return Variables.fresh(s)") == "10");
}

// ---- fmt / list / map ---------------------------------------------------------
TEST_CASE ("stdlib fmt formats and pads") {
  REQUIRE (satie::stdlib::fmt::format ("{} + {} = {}", 1, 2, 3) == "1 + 2 = 3");
  REQUIRE (satie::stdlib::fmt::format ("{{}}", 1) == "{}");
  REQUIRE (satie::stdlib::fmt::join (std::vector<int>{ 1, 2 }, ",") == "1,2");
  REQUIRE (satie::stdlib::fmt::pad_left ("ab", 4) == "  ab");
  REQUIRE (satie::stdlib::fmt::trim ("  x  ") == "x");
}

TEST_CASE ("stdlib list transforms") {
  const std::vector<int> items{ 1, 2, 3 };
  auto doubled = satie::stdlib::list::transform (items, [] (int v) { return v * 2; });
  REQUIRE (doubled == std::vector<int> ({ 2, 4, 6 }));
  REQUIRE (satie::stdlib::list::fold (items, 0, [] (int a, int b) { return a + b; }) == 6);
  REQUIRE (satie::stdlib::list::range (0, 3) == std::vector<int> ({ 0, 1, 2 }));
}

TEST_CASE ("stdlib map assoc helpers") {
  std::vector<std::pair<std::string, int>> assoc;
  satie::stdlib::map::assoc_upsert (assoc, std::string ("a"), 1);
  satie::stdlib::map::assoc_upsert (assoc, std::string ("a"), 2);
  REQUIRE (satie::stdlib::map::assoc_lookup (assoc, std::string ("a")).value () == 2);
  REQUIRE (satie::stdlib::map::assoc_remove (assoc, std::string ("a")));
  REQUIRE (!satie::stdlib::map::assoc_lookup (assoc, std::string ("a")).has_value ());
}

TEST_CASE ("stdlib fmt list map lua") {
  REQUIRE (run_lua_module ("fmt/fmt.lua", "return Fmt.pad_left('ab', 4)") == "  ab");
  REQUIRE (run_lua_module ("list/list.lua",
                           "return List.fold({1, 2, 3, 4}, 0, "
                           "function(a, x) return a + x end)") == "10");
  REQUIRE (run_lua_module ("map/map.lua",
                           "local m = {} Map.upsert(m, 'a', 2) return Map.lookup(m, 'a')") == "2");
}

// ---- graphlib / graphviz -------------------------------------------------------
TEST_CASE ("stdlib graphlib bfs and topo") {
  satie::stdlib::graphlib::DiGraph graph;
  graph.add_edge (1, 2);
  graph.add_edge (2, 3);
  REQUIRE (graph.bfs (1) == std::vector<int> ({ 1, 2, 3 }));
  REQUIRE (graph.has_path (1, 3));
  REQUIRE (!graph.has_path (3, 1));
  REQUIRE (graph.topo_sort () == std::vector<int> ({ 1, 2, 3 }));
}

TEST_CASE ("stdlib graphviz dot output") {
  satie::stdlib::graphviz::Dot dot ("G");
  dot.node ("n", "N");
  dot.edge ("n", "n", "loop");
  const std::string text = dot.dump ();
  REQUIRE (text.find ("digraph G") != std::string::npos);
  REQUIRE (text.find ("\"n\" -> \"n\"") != std::string::npos);
}

TEST_CASE ("stdlib graphlib graphviz lua") {
  REQUIRE (run_lua_module ("graphlib/graphlib.lua",
                           "local g = Graphlib.new() Graphlib.add_edge(g, 1, 2) "
                           "Graphlib.add_edge(g, 2, 3) return Graphlib.bfs(g, 1)[3]") == "3");
  REQUIRE (run_lua_module ("graphviz/graphviz.lua",
                           "local g = Graphviz.new('G') Graphviz.node(g, 'n', 'N') "
                           "return Graphviz.dump(g)") ==
           "digraph G {\n  \"n\" [label=\"N\"];\n}\n");
}

// ---- html / latex ---------------------------------------------------------------
TEST_CASE ("stdlib html escapes and builds") {
  REQUIRE (satie::stdlib::html::escape ("<a>&") == "&lt;a&gt;&amp;");
  const std::string table =
      satie::stdlib::html::table ({ "h" }, { { "v" } });
  REQUIRE (table.find ("<td>v</td>") != std::string::npos);
}

TEST_CASE ("stdlib latex renders CNF") {
  satie::CNF cnf ({{ 1, -2 }});
  const std::string text = satie::stdlib::latex::cnf_to_latex (cnf);
  REQUIRE (text.find ("x_{1}") != std::string::npos);
  REQUIRE (text.find ("\\lnot") != std::string::npos);
}

TEST_CASE ("stdlib html latex lua") {
  REQUIRE (run_lua_module ("html/html.lua", "return Html.tag('b', 'x')") == "<b>x</b>");
  REQUIRE (run_lua_module ("latex/latex.lua", "return Latex.frac('1', '2')") ==
           "\\frac{1}{2}");
}

// ---- io / os / path ---------------------------------------------------------------
TEST_CASE ("stdlib io round-trips a file") {
  const std::string path = "/tmp/opencode/stdlib_io_test.txt";
  satie::stdlib::io::write_file (path, "a\nb\n");
  REQUIRE (satie::stdlib::io::read_file (path) == "a\nb\n");
  REQUIRE (satie::stdlib::io::read_lines (path) == std::vector<std::string> ({ "a", "b" }));
  REQUIRE (satie::stdlib::io::file_exists (path));
  std::filesystem::remove (path);
}

TEST_CASE ("stdlib os and path helpers") {
  REQUIRE (!satie::stdlib::os::current_dir ().empty ());
  REQUIRE (satie::stdlib::os::getenv_or ("SATIE_DEFINITELY_MISSING", "dflt") == "dflt");
  REQUIRE (satie::stdlib::path::extension ("a/b.cnf") == ".cnf");
  REQUIRE (satie::stdlib::path::basename ("a/b.cnf") == "b.cnf");
  REQUIRE (satie::stdlib::path::stem ("a/b.cnf") == "b");
}

// ---- json / yaml / openai -----------------------------------------------------------
TEST_CASE ("stdlib json round-trips") {
  satie::stdlib::json::Object fields;
  fields.emplace ("a", satie::stdlib::json::Value::number_value (1));
  const std::string text =
      satie::stdlib::json::dump (satie::stdlib::json::Value::object_value (fields));
  REQUIRE (text == "{\"a\":1}");
  satie::stdlib::json::Value back = satie::stdlib::json::parse (text);
  REQUIRE (back.fields->at ("a").number == 1);
}

TEST_CASE ("stdlib yaml parses and dumps") {
  satie::stdlib::yaml::Value root =
      satie::stdlib::yaml::parse ("a: 1\nb:\n  - x\n  - y\n");
  REQUIRE (root.fields->size () == 2);
  const std::string text = satie::stdlib::yaml::dump (root);
  REQUIRE (text.find ("a: 1") != std::string::npos);
  REQUIRE (text.find ("- x") != std::string::npos);
}

TEST_CASE ("stdlib openai builds and reads payloads") {
  const std::string payload = satie::stdlib::openai::chat_payload (
      "m", { satie::stdlib::openai::message ("user", "hi") });
  REQUIRE (payload.find ("\"model\":\"m\"") != std::string::npos);
  REQUIRE (satie::stdlib::openai::first_choice_text (
               "{\"choices\":[{\"message\":{\"content\":\"yo\"}}]}") == "yo");
  REQUIRE (satie::stdlib::openai::first_choice_text ("nope").empty ());
}

TEST_CASE ("stdlib json yaml openai lua") {
  REQUIRE (run_lua_module ("json/json.lua",
                           "return Json.encode_object({'a', 'b'}, {'1', 'true'})") ==
           "{\"a\":1,\"b\":true}");
  REQUIRE (run_lua_module ("yaml/yaml.lua", "return Yaml.mapping({'a'}, {'1'})") ==
           "a: 1\n");
  REQUIRE (run_lua_module ("openai/openai.lua",
                           "return Openai.chat_payload('m', {Openai.message('user', 'hi')})") ==
           "{\"model\":\"m\",\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}");
}

// ---- log / logsynth ---------------------------------------------------------------------
TEST_CASE ("stdlib log levels and counters") {
  satie::stdlib::log::Logger logger ("t", satie::stdlib::log::Level::Info);
  std::vector<satie::stdlib::log::Record> records;
  logger.add_sink ([&] (const satie::stdlib::log::Record &record) {
    records.push_back (record);
  });
  logger.debug ("hidden");
  logger.info ("shown");
  REQUIRE (records.size () == 1);
  REQUIRE (logger.count (satie::stdlib::log::Level::Info) == 1);
  REQUIRE (logger.count (satie::stdlib::log::Level::Debug) == 0);
}

TEST_CASE ("stdlib logsynth emits json lines") {
  std::ostringstream out;
  satie::stdlib::logsynth::EventLog log (out);
  log.event ("solve").field ("cost", std::int64_t (3)).field ("ok", true).emit ();
  REQUIRE (out.str () == "{\"event\":\"solve\",\"cost\":3,\"ok\":true}\n");
  REQUIRE (log.count () == 1);
}

TEST_CASE ("stdlib log logsynth lua") {
  REQUIRE (run_lua_module ("log/log.lua",
                           "local l = Log.new('t') Log.info(l, 'hi') "
                           "Log.debug(l, 'no') return Log.count(l, 1)") == "1");
  REQUIRE (run_lua_module ("logsynth/logsynth.lua",
                           "local l = Logsynth.new() Logsynth.emit(l, 'solve', "
                           "{Logsynth.field_int('cost', 3)}) return Logsynth.dump(l)") ==
           "{\"event\":\"solve\",\"cost\":3}\n");
}

// ---- regex / sched / datalog -------------------------------------------------------------------
TEST_CASE ("stdlib regex matches safely") {
  REQUIRE (satie::stdlib::regex::match_full ("abc123", "[a-z]+[0-9]+"));
  REQUIRE (!satie::stdlib::regex::match_full ("123", "[a-z]+"));
  REQUIRE (!satie::stdlib::regex::try_match_full ("x", "([a-z").has_value ());
  REQUIRE (satie::stdlib::regex::replace_all ("a1b2", "[0-9]", "#") == "a#b#");
  REQUIRE (satie::stdlib::regex::split_pattern ("a,b;c", "[,;]") ==
           std::vector<std::string> ({ "a", "b", "c" }));
}

TEST_CASE ("stdlib sched runs in dependency order") {
  satie::stdlib::sched::TaskGraph graph;
  std::vector<std::string> order;
  graph.add_task ("b", {}, [&] { order.push_back ("b"); return "b"; });
  graph.add_task ("a", { "b" }, [&] { order.push_back ("a"); return "a"; });
  auto results = graph.run ();
  REQUIRE ((order == std::vector<std::string>{ "b", "a" }));
  REQUIRE (results.at ("a") == "a");
}

TEST_CASE ("stdlib datalog derives transitively") {
  satie::stdlib::datalog::Database db;
  db.add_fact ("edge", { "a", "b" });
  db.add_fact ("edge", { "b", "c" });
  satie::stdlib::datalog::Rule base;
  base.head = satie::stdlib::datalog::atom ("path", { "X", "Y" }, { true, true });
  base.body = { satie::stdlib::datalog::atom ("edge", { "X", "Y" }, { true, true }) };
  satie::stdlib::datalog::Rule step;
  step.head = satie::stdlib::datalog::atom ("path", { "X", "Z" }, { true, true });
  step.body = { satie::stdlib::datalog::atom ("edge", { "X", "Y" }, { true, true }),
                satie::stdlib::datalog::atom ("path", { "Y", "Z" }, { true, true }) };
  db.add_rule (std::move (base));
  db.add_rule (std::move (step));
  db.evaluate ();
  REQUIRE (db.query ("path", { "a", "c" }));
  REQUIRE (!db.query ("path", { "c", "a" }));
}

TEST_CASE ("stdlib sched datalog lua") {
  REQUIRE (run_lua_module ("sched/sched.lua",
                           "local r = Sched.run_all({{name='b', deps={}, "
                           "run=function() return 2 end}, {name='a', deps={'b'}, "
                           "run=function() return 1 end}}) return r[1][1]") == "b");
  REQUIRE (run_lua_module ("datalog/datalog.lua",
                           "local h1 = {'path', {'X','Y'}} "
                           "local b1 = {'edge', {'X','Y'}} "
                           "local rules = {{head = h1, body = {b1}}} "
                           "local db = {edge = {{'a','b'}}} "
                           "Datalog.evaluate(db, rules, 10) "
                           "return Datalog.query(db, 'path', {'a','b'})") == "true");
}

// ---- encoding / simplifier / ffi / import -------------------------------------------------------------
TEST_CASE ("stdlib encoding cardinalities solve correctly") {
  satie::CNF cnf;
  for (satie::Clause &clause :
       satie::stdlib::encoding::encode_exactly_one ({ 1, 2, 3 }, 4))
    cnf.add_clause (std::move (clause));
  satie::SolveResult result = satie::solve (cnf);
  REQUIRE (result.satisfiable ());
  int true_count = 0;
  for (satie::Var v = 1; v <= 3; ++v)
    true_count += result.assignment.get_var (v) == satie::Value::TRUE ? 1 : 0;
  REQUIRE (true_count == 1);
}

TEST_CASE ("stdlib simplifier preprocesses CNF") {
  satie::CNF cnf ({{ 1, -1 }, { 1, 2 }, { 1, 2 }, { 2 }});
  satie::stdlib::simplifier::PreprocessResult result =
      satie::stdlib::simplifier::simplify_cnf (cnf);
  REQUIRE (!result.trivially_unsat);
  REQUIRE (result.stats.tautologies_removed == 1);
  REQUIRE (satie::solve (result.cnf.simplified ()).satisfiable ());
  satie::CNF contra ({{ 1 }, { -1 }});
  REQUIRE (satie::stdlib::simplifier::simplify_cnf (contra).trivially_unsat);
}

TEST_CASE ("stdlib ffi guards manage C handles") {
  auto solver = satie::stdlib::ffi::make_solver ();
  REQUIRE (solver != nullptr);
  const int clause[] = { 1 };
  REQUIRE (satie_solver_add_clause (solver.get (), clause, 1) == 0);
  REQUIRE (satie_solver_solve (solver.get (), SATIE_C_CDCL) == SATIE_C_SAT);
  auto module = satie::stdlib::ffi::make_module ("BV");
  REQUIRE (module != nullptr);
  auto plugin = satie::stdlib::ffi::make_plugin_context ();
  REQUIRE (plugin != nullptr);
}

TEST_CASE ("stdlib import loads lua files") {
  satie::PluginHost host;
  qamrpp::Context context;
  auto value = satie::stdlib::import::load_lua_file (
      context, std::string (SATIE_STDLIB_DIR) + "/arith/arith.lua");
  REQUIRE (value != nullptr);
}

TEST_CASE ("stdlib encoding simplifier lua") {
  REQUIRE (run_lua_module ("encoding/encoding.lua",
                           "return Encoding.cnf_str({{1, -2}, {3}}, true)") ==
           "p cnf 3 2\n1 -2 0\n3 0\n");
  REQUIRE (run_lua_module ("simplifier/simplifier.lua",
                           "local r = Simplifier.simplify({{1, -1}, {2}}) "
                           "return #r.clauses") == "1");
}
