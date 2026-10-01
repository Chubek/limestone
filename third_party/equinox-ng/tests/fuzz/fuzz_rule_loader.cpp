#include <cstdint>
#include <fstream>
#include <memory>
#include <string>

#include "QaMRpp.hpp"

static std::unique_ptr<qamrpp::Context> g_ctx;
static const char* kTmp = "/tmp/eqvsg_fuzz_rules.eq";

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (!g_ctx) {
        g_ctx = std::make_unique<qamrpp::Context>();
        g_ctx->load_library("bin/libqamrpp-sexpr.so");
        g_ctx->load_library("bin/libqamrpp-leqvsg.so");
    }
    {
        std::ofstream out(kTmp, std::ios::binary);
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    }
    g_ctx->assign_name("__rules", std::make_shared<qamrpp::Value>(std::string(kTmp)));
    try {
        (void)g_ctx->run("return eqvsg_load_rewrite_rules(__rules)\n");
    } catch (...) {}
    return 0;
}
