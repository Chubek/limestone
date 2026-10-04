#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#ifdef HAVE_LMDB
extern "C" {
#include <glr/parser.h>
#include <glr/cache.h>
#include <glr/grammar.h>
}
#endif

#include <cstring>
#include <cstdio>
#include <sys/stat.h>

#ifdef HAVE_LMDB

TEST_CASE("Incremental: API availability", "[incremental][api]") {
    system("rm -rf /tmp/libglr_test_inc");
    mkdir("/tmp/libglr_test_inc", 0755);
    
    glr_grammar_t* grammar = glr_grammar_create();
    REQUIRE(grammar != nullptr);
    
    glr_parser_t* parser = glr_parser_create(grammar);
    REQUIRE(parser != nullptr);
    
    glr_cache_config_t config = GLR_CACHE_DEFAULT_CONFIG;
    config.mdbx_path = "/tmp/libglr_test_inc";
    config.map_size = 10 * 1024 * 1024;
    
    glr_cache_t* cache = glr_cache_open(&config);
    REQUIRE(cache != nullptr);
    
    glr_parser_set_cache(parser, cache);
    
    glr_cache_t* retrieved = glr_parser_get_cache(parser);
    REQUIRE(retrieved == cache);
    
    glr_parser_destroy(parser);
    glr_cache_close(cache);
    glr_grammar_destroy(grammar);
    
    system("rm -rf /tmp/libglr_test_inc");
}

#else

TEST_CASE("Incremental: Disabled without LMDB", "[incremental][disabled]") {
    REQUIRE(true);
}

#endif

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
}
