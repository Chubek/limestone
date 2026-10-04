#include <glr/config.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#ifdef HAVE_LMDB

extern "C" {
#include <glr/serialization.h>
#include <glr/forest.h>
}
#include <cstring>

TEST_CASE("Serialization: Forest round-trip", "[serialization][forest]") {
    glr_forest_t* original = glr_forest_create();
    REQUIRE(original != nullptr);
    
    uint8_t* buffer = nullptr;
    size_t size = 0;
    int ret = glr_serialize_forest(original, &buffer, &size);
    
    REQUIRE(ret == 0);
    REQUIRE(buffer != nullptr);
    REQUIRE(size > 0);
    
    glr_forest_t* deserialized = nullptr;
    ret = glr_deserialize_forest(buffer, size, &deserialized);
    REQUIRE(ret == 0);
    REQUIRE(deserialized != nullptr);
    
    free(buffer);
    glr_forest_destroy(original);
    glr_forest_destroy(deserialized);
}

TEST_CASE("Serialization: Invalid input", "[serialization][error]") {
    SECTION("Null forest") {
        uint8_t* buffer = nullptr;
        size_t size = 0;
        int ret = glr_serialize_forest(nullptr, &buffer, &size);
        REQUIRE(ret == -1);
    }
    
    SECTION("Invalid buffer") {
        uint8_t bad_buffer[10] = {0};
        glr_forest_t* forest = nullptr;
        int ret = glr_deserialize_forest(bad_buffer, 10, &forest);
        REQUIRE(ret == -1);
    }
}

TEST_CASE("Serialization: Size estimation", "[serialization][size]") {
    glr_forest_t* forest = glr_forest_create();
    REQUIRE(forest != nullptr);
    
    uint8_t* buffer = nullptr;
    size_t size = 0;
    int ret = glr_serialize_forest(forest, &buffer, &size);
    
    REQUIRE(ret == 0);
    REQUIRE(size >= sizeof(glr_serialized_forest_header_t));
    
    free(buffer);
    glr_forest_destroy(forest);
}

#else

TEST_CASE("Serialization: Disabled without LMDB", "[serialization][disabled]") {
    REQUIRE(true);
}

#endif

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
}
