#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

#include "QaMRpp.hpp"

namespace {

struct TempFile {
    std::string path;
    TempFile(const std::string& p, const std::string& data) : path(p) {
        std::ofstream out(path, std::ios::binary);
        REQUIRE(out.good());
        out << data;
        out.close();
    }
    ~TempFile() { std::remove(path.c_str()); }
};

static std::unique_ptr<qamrpp::Context> mkctx() {
    auto ctx = std::make_unique<qamrpp::Context>();
    REQUIRE(ctx->load_library("bin/libqamrpp-sexpr.so"));
    REQUIRE(ctx->load_library("bin/libqamrpp-leqvsg.so"));
    return ctx;
}

static std::string run_rewrite(qamrpp::Context& ctx, const std::string& input) {
    ctx.assign_name("__in", std::make_shared<qamrpp::Value>(input));
    auto r = ctx.run("return eqvsg_rewrite(__in)\n");
    REQUIRE(r);
    return r->to_string();
}

static void load_rules(qamrpp::Context& ctx, const std::string& path) {
    ctx.assign_name("__rules", std::make_shared<qamrpp::Value>(path));
    auto r = ctx.run("return eqvsg_load_rewrite_rules(__rules)\n");
    REQUIRE(r);
}

static void load_expr(qamrpp::Context& ctx, const std::string& path) {
    ctx.assign_name("__expr", std::make_shared<qamrpp::Value>(path));
    auto r = ctx.run("return eqvsg_load_expr_definition(__expr)\n");
    REQUIRE(r);
}

} // namespace

TEST_CASE("rewrite_identity_without_rules") {
    auto ctx = mkctx();
    REQUIRE(run_rewrite(*ctx, "(+ 1 2)") == "(+ 1 2)");
}

TEST_CASE("single_rule_rewrite") {
    auto ctx = mkctx();
    TempFile rules("/tmp/rules1.eq", "(+ 1 2) => 3\n");
    load_rules(*ctx, rules.path);
    REQUIRE(run_rewrite(*ctx, "(+ 1 2)") == "3");
}

TEST_CASE("ordered_first_match") {
    auto ctx = mkctx();
    TempFile rules("/tmp/rules2.eq", "a => b\na => c\n");
    load_rules(*ctx, rules.path);
    REQUIRE(run_rewrite(*ctx, "a") == "b");
}

TEST_CASE("fixpoint_two_step") {
    auto ctx = mkctx();
    TempFile rules("/tmp/rules3.eq", "a => b\nb => c\n");
    load_rules(*ctx, rules.path);
    REQUIRE(run_rewrite(*ctx, "a") == "c");
}

TEST_CASE("comment_and_blank_ignored") {
    auto ctx = mkctx();
    TempFile rules("/tmp/rules4.eq", "# c1\n\nfoo => bar\n");
    load_rules(*ctx, rules.path);
    REQUIRE(run_rewrite(*ctx, "foo") == "bar");
}

TEST_CASE("trim_around_arrow") {
    auto ctx = mkctx();
    TempFile rules("/tmp/rules5.eq", "  x   =>   y  \n");
    load_rules(*ctx, rules.path);
    REQUIRE(run_rewrite(*ctx, "x") == "y");
}

TEST_CASE("empty_rhs_allowed") {
    auto ctx = mkctx();
    TempFile rules("/tmp/rules6.eq", "abc => \n");
    load_rules(*ctx, rules.path);
    REQUIRE(run_rewrite(*ctx, "abc") == "");
}

TEST_CASE("multiple_inputs_same_rules") {
    auto ctx = mkctx();
    TempFile rules("/tmp/rules7.eq", "cat => dog\n");
    load_rules(*ctx, rules.path);
    REQUIRE(run_rewrite(*ctx, "cat") == "dog");
    REQUIRE(run_rewrite(*ctx, "tomcat") == "tomdog");
}

TEST_CASE("reload_rules_replaces_old") {
    auto ctx = mkctx();
    TempFile r1("/tmp/rules8a.eq", "a => b\n");
    TempFile r2("/tmp/rules8b.eq", "a => c\n");
    load_rules(*ctx, r1.path);
    REQUIRE(run_rewrite(*ctx, "a") == "b");
    load_rules(*ctx, r2.path);
    REQUIRE(run_rewrite(*ctx, "a") == "c");
}

TEST_CASE("load_expr_definition_smoke") {
    auto ctx = mkctx();
    TempFile expr("/tmp/expr1.eq", "expr-def-1\n");
    load_expr(*ctx, expr.path);
    REQUIRE(run_rewrite(*ctx, "k") == "k");
}

TEST_CASE("missing_rules_file_throws") {
    auto ctx = mkctx();
    ctx->assign_name("__rules", std::make_shared<qamrpp::Value>(std::string("/tmp/no_such_rules.eq")));
    REQUIRE_THROWS(ctx->run("return eqvsg_load_rewrite_rules(__rules)\n"));
}

TEST_CASE("missing_expr_file_throws") {
    auto ctx = mkctx();
    ctx->assign_name("__expr", std::make_shared<qamrpp::Value>(std::string("/tmp/no_such_expr.eq")));
    REQUIRE_THROWS(ctx->run("return eqvsg_load_expr_definition(__expr)\n"));
}

TEST_CASE("malformed_rule_without_arrow_throws") {
    auto ctx = mkctx();
    TempFile rules("/tmp/rules9.eq", "abc\n");
    ctx->assign_name("__rules", std::make_shared<qamrpp::Value>(rules.path));
    REQUIRE_THROWS(ctx->run("return eqvsg_load_rewrite_rules(__rules)\n"));
}

TEST_CASE("empty_lhs_throws") {
    auto ctx = mkctx();
    TempFile rules("/tmp/rules10.eq", "   => z\n");
    ctx->assign_name("__rules", std::make_shared<qamrpp::Value>(rules.path));
    REQUIRE_THROWS(ctx->run("return eqvsg_load_rewrite_rules(__rules)\n"));
}

TEST_CASE("substring_rewrite_occurs") {
    auto ctx = mkctx();
    TempFile rules("/tmp/rules11.eq", "ab => X\n");
    load_rules(*ctx, rules.path);
    REQUIRE(run_rewrite(*ctx, "zabz") == "zXz");
}

TEST_CASE("only_one_replacement_per_pass") {
    auto ctx = mkctx();
    TempFile rules("/tmp/rules12.eq", "aa => a\n");
    load_rules(*ctx, rules.path);
    REQUIRE(run_rewrite(*ctx, "aaaa") == "a");
}

TEST_CASE("large_input_stability") {
    auto ctx = mkctx();
    TempFile rules("/tmp/rules13.eq", "x => y\n");
    load_rules(*ctx, rules.path);
    std::string in(4096, 'x');
    auto out = run_rewrite(*ctx, in);
    REQUIRE(out.size() == 4096);
    REQUIRE(out.front() == 'y');
}

TEST_CASE("unicode_bytes_pass_through") {
    auto ctx = mkctx();
    TempFile rules("/tmp/rules14.eq", "alpha => beta\n");
    load_rules(*ctx, rules.path);
    REQUIRE(run_rewrite(*ctx, "\xE2\x98\x83 alpha") == "\xE2\x98\x83 beta");
}

TEST_CASE("define_expr_accepts_two_strings") {
    auto ctx = mkctx();
    ctx->assign_name("__a", std::make_shared<qamrpp::Value>(std::string("name")));
    ctx->assign_name("__b", std::make_shared<qamrpp::Value>(std::string("schema")));
    auto r = ctx->run("return eqvsg_define_expr(__a, __b)\n");
    REQUIRE(r);
    REQUIRE(r->to_string() == "nil");
}

TEST_CASE("define_expr_wrong_arity_throws") {
    auto ctx = mkctx();
    REQUIRE_THROWS(ctx->run("return eqvsg_define_expr()\n"));
}

TEST_CASE("rewrite_wrong_arity_throws") {
    auto ctx = mkctx();
    REQUIRE_THROWS(ctx->run("return eqvsg_rewrite()\n"));
}

TEST_CASE("load_rules_wrong_arity_throws") {
    auto ctx = mkctx();
    REQUIRE_THROWS(ctx->run("return eqvsg_load_rewrite_rules()\n"));
}
