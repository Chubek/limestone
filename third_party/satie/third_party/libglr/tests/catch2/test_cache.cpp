#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#ifdef HAVE_LMDB
extern "C" {
#include <glr/cache.h>
#include <glr/forest.h>
}
#endif

#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_LMDB

static const char* TEST_CACHE_DIR = "/tmp/libglr_test_cache";

TEST_CASE("Cache: Basic operations", "[cache][basic]") {
    system("rm -rf /tmp/libglr_test_cache");
    mkdir(TEST_CACHE_DIR, 0755);
    
    glr_cache_config_t config = GLR_CACHE_DEFAULT_CONFIG;
    config.mdbx_path = TEST_CACHE_DIR;
    config.map_size = 10 * 1024 * 1024;
    
    glr_cache_t* cache = glr_cache_open(&config);
    REQUIRE(cache != nullptr);
    
    glr_cache_close(cache);
    system("rm -rf /tmp/libglr_test_cache");
}

TEST_CASE("Cache: Hash computation", "[cache][hash]") {
    const char* text1 = "hello world";
    const char* text2 = "hello world";
    const char* text3 = "hello world!";
    
    uint8_t hash1[32], hash2[32], hash3[32];
    
    glr_cache_compute_hash((const uint8_t*)text1, strlen(text1), hash1);
    glr_cache_compute_hash((const uint8_t*)text2, strlen(text2), hash2);
    glr_cache_compute_hash((const uint8_t*)text3, strlen(text3), hash3);
    
    REQUIRE(memcmp(hash1, hash2, 32) == 0);
    REQUIRE(memcmp(hash1, hash3, 32) != 0);
}

#else

TEST_CASE("Cache: Disabled without LMDB", "[cache][disabled]") {
    REQUIRE(true);
}

#endif

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
}
