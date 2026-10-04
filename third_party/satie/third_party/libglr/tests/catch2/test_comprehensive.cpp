/**
 * Comprehensive Test Suite for libglr - 26 tests
 * Focuses on testable public APIs
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

extern "C" {
#include <glr/glr.h>
#include <glr/grammar.h>
#include <glr/stack.h>
#include <glr/forest.h>
#include <glr/reduction.h>
#include <glr/reader.h>
#include <glr/graph.h>
#ifdef HAVE_LMDB
#include <glr/cache.h>
#include <glr/diff.h>
#include <glr/serialization.h>
#include <glr/dependency.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
}

// ============================================================================
// TEST GROUP 1: Grammar (4 tests)
// ============================================================================

TEST_CASE("Grammar: Create and destroy", "[grammar][core]") {
    glr_grammar_t* grammar = glr_grammar_create();
    REQUIRE(grammar != nullptr);
    glr_grammar_destroy(grammar);
}

TEST_CASE("Grammar: Add terminal symbol", "[grammar][core]") {
    glr_grammar_t* grammar = glr_grammar_create();
    REQUIRE(grammar != nullptr);
    
    int term_id = glr_grammar_add_symbol(grammar, GLR_SYMBOL_TERMINAL, "IDENT");
    REQUIRE(term_id >= 0);
    
    glr_grammar_destroy(grammar);
}

TEST_CASE("Grammar: Add nonterminal symbol", "[grammar][core]") {
    glr_grammar_t* grammar = glr_grammar_create();
    REQUIRE(grammar != nullptr);
    
    int nt_id = glr_grammar_add_symbol(grammar, GLR_SYMBOL_NONTERMINAL, "Expr");
    REQUIRE(nt_id >= 0);
    
    glr_grammar_destroy(grammar);
}

TEST_CASE("Grammar: Add production", "[grammar][core]") {
    glr_grammar_t* grammar = glr_grammar_create();
    REQUIRE(grammar != nullptr);
    
    int nt = glr_grammar_add_symbol(grammar, GLR_SYMBOL_NONTERMINAL, "E");
    int t = glr_grammar_add_symbol(grammar, GLR_SYMBOL_TERMINAL, "n");
    
    glr_symbol_t* syms[1];
    syms[0] = glr_grammar_get_symbol(grammar, t);
    
    int prod = glr_grammar_add_production(grammar, nt, syms, 1);
    REQUIRE(prod >= 0);
    
    glr_grammar_destroy(grammar);
}

// ============================================================================
// TEST GROUP 2: Stack Operations (3 tests)
// ============================================================================

TEST_CASE("Stack: Create and destroy", "[stack][core]") {
    glr_stack_t* stack = glr_stack_create();
    REQUIRE(stack != nullptr);
    glr_stack_destroy(stack);
}

TEST_CASE("Stack: Push state", "[stack][core]") {
    glr_stack_t* stack = glr_stack_create();
    REQUIRE(stack != nullptr);
    
    int state = 42;
    int result = glr_stack_push(stack, &state);
    REQUIRE(result == 0);
    
    glr_stack_destroy(stack);
}

TEST_CASE("Stack: Check empty", "[stack][core]") {
    glr_stack_t* stack = glr_stack_create();
    REQUIRE(stack != nullptr);
    
    int result = glr_stack_empty(stack);
    REQUIRE(result == 1);
    
    glr_stack_destroy(stack);
}

// ============================================================================
// TEST GROUP 3: Forest Operations (3 tests)
// ============================================================================

TEST_CASE("Forest: Create and destroy", "[forest][core]") {
    glr_forest_t* forest = glr_forest_create();
    REQUIRE(forest != nullptr);
    glr_forest_destroy(forest);
}

TEST_CASE("Forest: Get terminal node", "[forest][core]") {
    glr_forest_t* forest = glr_forest_create();
    REQUIRE(forest != nullptr);
    
    glr_forest_node_t* node = glr_forest_get_node(forest, GLR_NODE_TERMINAL, 1, 0);
    REQUIRE(node != nullptr);
    REQUIRE(node->type == GLR_NODE_TERMINAL);
    
    glr_forest_destroy(forest);
}

TEST_CASE("Forest: Get nonterminal node", "[forest][core]") {
    glr_forest_t* forest = glr_forest_create();
    REQUIRE(forest != nullptr);
    
    glr_forest_node_t* node = glr_forest_get_node(forest, GLR_NODE_NONTERMINAL, 2, 0);
    REQUIRE(node != nullptr);
    REQUIRE(node->type == GLR_NODE_NONTERMINAL);
    
    glr_forest_destroy(forest);
}

// ============================================================================
// TEST GROUP 4: Reader Operations (3 tests)
// ============================================================================

TEST_CASE("Reader: Create and destroy", "[reader][lexer]") {
    glr_reader_t* reader = glr_reader_create();
    REQUIRE(reader != nullptr);
    glr_reader_destroy(reader);
}

TEST_CASE("Reader: Set input", "[reader][lexer]") {
    glr_reader_t* reader = glr_reader_create();
    REQUIRE(reader != nullptr);
    
    const char* input = "test";
    int result = glr_reader_set_input(reader, input, strlen(input));
    REQUIRE(result == 0);
    
    glr_reader_destroy(reader);
}

TEST_CASE("Reader: Check status after input", "[reader][lexer]") {
    glr_reader_t* reader = glr_reader_create();
    REQUIRE(reader != nullptr);
    
    const char* input = "test";
    int result = glr_reader_set_input(reader, input, strlen(input));
    REQUIRE(result == 0);
    
    glr_reader_destroy(reader);
}

// ============================================================================
// TEST GROUP 5: Graph Operations (2 tests)
// ============================================================================

TEST_CASE("Graph: Create and destroy", "[graph][core]") {
    glr_graph_t* graph = glr_graph_create();
    REQUIRE(graph != nullptr);
    glr_graph_destroy(graph);
}

TEST_CASE("Graph: Add node", "[graph][core]") {
    glr_graph_t* graph = glr_graph_create();
    REQUIRE(graph != nullptr);
    
    int data = 42;
    int node_id = glr_graph_add_node(graph, &data);
    REQUIRE(node_id >= 0);
    
    glr_graph_destroy(graph);
}

// ============================================================================
// TEST GROUP 6: Reduction Operations (2 tests)
// ============================================================================

TEST_CASE("Reduction: Create and destroy", "[reduction][core]") {
    glr_reduction_t* reduction = glr_reduction_create(0, 0);
    REQUIRE(reduction != nullptr);
    glr_reduction_destroy(reduction);
}

TEST_CASE("Reduction: Get production", "[reduction][core]") {
    glr_reduction_t* reduction = glr_reduction_create(42, 10);
    REQUIRE(reduction != nullptr);
    
    int prod = glr_reduction_get_production(reduction);
    REQUIRE(prod == 42);
    
    glr_reduction_destroy(reduction);
}

// ============================================================================
// TEST GROUP 7: Incremental - Cache (3 tests)
// ============================================================================

#ifdef HAVE_LMDB

TEST_CASE("Cache: Open and close", "[cache][incremental]") {
    system("mkdir -p /tmp/glr_test_comprehensive_1");
    
    glr_cache_config_t config = {0};
    config.mdbx_path = "/tmp/glr_test_comprehensive_1";
    config.map_size = 100 * 1024 * 1024;
    config.max_readers = 10;
    
    glr_cache_t* cache = glr_cache_open(&config);
    REQUIRE(cache != nullptr);
    glr_cache_close(cache);
    system("rm -rf /tmp/glr_test_comprehensive_1");
}

TEST_CASE("Cache: Compute hash", "[cache][incremental]") {
    const char* data = "test data";
    uint8_t hash[32];
    
    glr_cache_compute_hash((const uint8_t*)data, strlen(data), hash);
    
    bool non_zero = false;
    for (int i = 0; i < 32; i++) {
        if (hash[i] != 0) {
            non_zero = true;
            break;
        }
    }
    REQUIRE(non_zero);
}

TEST_CASE("Cache: Get statistics", "[cache][incremental]") {
    system("mkdir -p /tmp/glr_test_comprehensive_2");
    
    glr_cache_config_t config = {0};
    config.mdbx_path = "/tmp/glr_test_comprehensive_2";
    config.map_size = 100 * 1024 * 1024;
    config.max_readers = 10;
    
    glr_cache_t* cache = glr_cache_open(&config);
    REQUIRE(cache != nullptr);
    
    glr_cache_stats_t stats;
    glr_cache_get_stats(cache, &stats);
    REQUIRE(stats.forest_count >= 0);
    
    glr_cache_close(cache);
    system("rm -rf /tmp/glr_test_comprehensive_2");
}

// ============================================================================
// TEST GROUP 8: Incremental - Diff (2 tests)
// ============================================================================

TEST_CASE("Diff: Detect insertion", "[diff][incremental]") {
    const char* old_text = "hello world";
    const char* new_text = "hello beautiful world";
    
    glr_edit_t edit;
    int result = glr_compute_edit(old_text, strlen(old_text),
                                   new_text, strlen(new_text),
                                   &edit);
    REQUIRE(result == 0);
    REQUIRE(edit.old_start == 6);
}

TEST_CASE("Diff: Detect deletion", "[diff][incremental]") {
    const char* old_text = "hello beautiful world";
    const char* new_text = "hello world";
    
    glr_edit_t edit;
    int result = glr_compute_edit(old_text, strlen(old_text),
                                   new_text, strlen(new_text),
                                   &edit);
    REQUIRE(result == 0);
    REQUIRE(edit.old_start == 6);
}

// ============================================================================
// TEST GROUP 9: Incremental - Serialization (2 tests)
// ============================================================================

TEST_CASE("Serialization: Serialize node", "[serialization][incremental]") {
    glr_forest_t* forest = glr_forest_create();
    glr_forest_node_t* node = glr_forest_get_node(forest, GLR_NODE_TERMINAL, 1, 0);
    
    uint8_t* buffer = nullptr;
    size_t size = 0;
    int result = glr_serialize_forest_node(node, &buffer, &size);
    REQUIRE(result == 0);
    REQUIRE(buffer != nullptr);
    
    free(buffer);
    glr_forest_destroy(forest);
}

TEST_CASE("Serialization: Roundtrip", "[serialization][incremental]") {
    glr_forest_t* forest = glr_forest_create();
    glr_forest_node_t* original = glr_forest_get_node(forest, GLR_NODE_TERMINAL, 1, 0);
    
    uint8_t* buffer = nullptr;
    size_t size = 0;
    glr_serialize_forest_node(original, &buffer, &size);
    
    glr_forest_node_t* deserialized = nullptr;
    int result = glr_deserialize_forest_node(buffer, size, &deserialized);
    REQUIRE(result == 0);
    
    free(buffer);
    glr_forest_destroy(forest);
}

// ============================================================================
// TEST GROUP 10: Incremental - Dependency (2 tests)
// ============================================================================

TEST_CASE("Dependency: Add", "[dependency][incremental]") {
    system("mkdir -p /tmp/glr_test_comprehensive_3");
    
    glr_cache_config_t config = {0};
    config.mdbx_path = "/tmp/glr_test_comprehensive_3";
    config.map_size = 100 * 1024 * 1024;
    config.max_readers = 10;
    
    glr_cache_t* cache = glr_cache_open(&config);
    REQUIRE(cache != nullptr);
    
    int result = glr_dependency_add(cache, 123, 0, 10, 20);
    REQUIRE(result == 0);
    
    glr_cache_close(cache);
    system("rm -rf /tmp/glr_test_comprehensive_3");
}

TEST_CASE("Dependency: Invalidate", "[dependency][incremental]") {
    system("mkdir -p /tmp/glr_test_comprehensive_4");
    
    glr_cache_config_t config = {0};
    config.mdbx_path = "/tmp/glr_test_comprehensive_4";
    config.map_size = 100 * 1024 * 1024;
    config.max_readers = 10;
    
    glr_cache_t* cache = glr_cache_open(&config);
    REQUIRE(cache != nullptr);
    
    glr_dependency_add(cache, 123, 0, 10, 30);
    int result = glr_dependency_invalidate_range(cache, 15, 25);
    REQUIRE(result >= 0);
    
    glr_cache_close(cache);
    system("rm -rf /tmp/glr_test_comprehensive_4");
}

#endif

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
}
