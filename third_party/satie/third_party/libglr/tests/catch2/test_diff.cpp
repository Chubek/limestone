#include <glr/config.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#ifdef HAVE_LMDB

extern "C" {
#include <glr/diff.h>
}
#include <cstring>

TEST_CASE("Diff: Compute edit regions", "[diff][basic]") {
    const char* old_text = "hello world";
    const char* new_text = "hello beautiful world";
    
    glr_edit_t edit;
    int ret = glr_compute_edit(
        old_text, strlen(old_text),
        new_text, strlen(new_text),
        &edit
    );
    
    REQUIRE(ret == 0);
    REQUIRE(edit.old_start <= strlen(old_text));
    REQUIRE(edit.old_end <= strlen(old_text));
    REQUIRE(edit.new_start <= strlen(new_text));
    REQUIRE(edit.new_end <= strlen(new_text));
}

TEST_CASE("Diff: No change", "[diff][nochange]") {
    const char* text = "unchanged text";
    
    glr_edit_t edit;
    int ret = glr_compute_edit(
        text, strlen(text),
        text, strlen(text),
        &edit
    );
    
    REQUIRE(ret == 0);
    REQUIRE(glr_edit_is_empty(&edit));
}

TEST_CASE("Diff: Insertion at start", "[diff][insert]") {
    const char* old_text = "world";
    const char* new_text = "hello world";
    
    glr_edit_t edit;
    int ret = glr_compute_edit(
        old_text, strlen(old_text),
        new_text, strlen(new_text),
        &edit
    );
    
    REQUIRE(ret == 0);
    REQUIRE(edit.old_start == 0);
    REQUIRE(edit.new_start == 0);
}

TEST_CASE("Diff: Deletion at end", "[diff][delete]") {
    const char* old_text = "hello world";
    const char* new_text = "hello";
    
    glr_edit_t edit;
    int ret = glr_compute_edit(
        old_text, strlen(old_text),
        new_text, strlen(new_text),
        &edit
    );
    
    REQUIRE(ret == 0);
    REQUIRE(edit.old_end == strlen(old_text));
}

TEST_CASE("Diff: Empty strings", "[diff][empty]") {
    glr_edit_t edit;
    
    SECTION("Both empty") {
        int ret = glr_compute_edit("", 0, "", 0, &edit);
        REQUIRE(ret == 0);
        REQUIRE(glr_edit_is_empty(&edit));
    }
    
    SECTION("Old empty") {
        int ret = glr_compute_edit("", 0, "new", 3, &edit);
        REQUIRE(ret == 0);
        REQUIRE(edit.new_end == 3);
    }
    
    SECTION("New empty") {
        int ret = glr_compute_edit("old", 3, "", 0, &edit);
        REQUIRE(ret == 0);
        REQUIRE(edit.old_end == 3);
    }
}

#else

TEST_CASE("Diff: Disabled without LMDB", "[diff][disabled]") {
    REQUIRE(true);
}

#endif

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
}
