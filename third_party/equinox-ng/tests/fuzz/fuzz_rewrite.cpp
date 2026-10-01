#include <cstdint>
#include <fstream>
#include <memory>
#include <string>

#include "QaMRpp.hpp"

static std::unique_ptr<qamrpp::Context> g_ctx;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (!g_ctx) {
        g_ctx = std::make_unique<qamrpp::Context>();
        g_ctx->load_library("bin/libqamrpp-sexpr.so");
        g_ctx->load_library("bin/libqamrpp-leqvsg.so");
    }
    std::string s(reinterpret_cast<const char*>(data), size);
    g_ctx->assign_name("__in", std::make_shared<qamrpp::Value>(s));
    try {
        (void)g_ctx->run("return eqvsg_rewrite(__in)\n");
    } catch (...) {}
    return 0;
}
