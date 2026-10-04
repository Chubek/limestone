#include <nuperf/nuperf-api.h>
#include <nuperf/nuperf-method.h>

#include <cstring>
#include <string>
#include <vector>

namespace {

struct MethodImpl { uint32_t gamma = 2; };
struct ResultImpl { nuperf_hash_result_t base; std::vector<uint32_t> table; };

nuperf_status_t create(const nuperf_method_t* m, nuperf_method_instance_t** out) {
    if (!m || !out) return NUPERF_ERR_INVALID_ARGUMENT;
    auto* i = new nuperf_method_instance_t{};
    i->method = m;
    i->impl = new MethodImpl{};
    *out = i;
    return NUPERF_OK;
}

void destroy(nuperf_method_instance_t* i) { if (i) { delete static_cast<MethodImpl*>(i->impl); delete i; } }

nuperf_status_t set_option(nuperf_method_instance_t* i, const char* k, const char* v) {
    if (!i || !i->impl || !k || !v) return NUPERF_ERR_INVALID_ARGUMENT;
    if (std::strcmp(k, "gamma") != 0) return NUPERF_ERR_NOT_FOUND;
    static_cast<MethodImpl*>(i->impl)->gamma = static_cast<uint32_t>(std::stoul(v));
    return NUPERF_OK;
}

nuperf_status_t get_option(const nuperf_method_instance_t* i, const char* k, char* out, size_t* n) {
    if (!i || !i->impl || !k || !n) return NUPERF_ERR_INVALID_ARGUMENT;
    if (std::strcmp(k, "gamma") != 0) return NUPERF_ERR_NOT_FOUND;
    const std::string s = std::to_string(static_cast<const MethodImpl*>(i->impl)->gamma);
    const size_t need = s.size() + 1;
    if (!out || *n < need) { *n = need; return out ? NUPERF_ERR_BUFFER_TOO_SMALL : NUPERF_OK; }
    std::memcpy(out, s.c_str(), need); *n = need; return NUPERF_OK;
}

nuperf_status_t build(nuperf_method_instance_t* i, const nuperf_keyset_t* ks, nuperf_hash_result_t** out) {
    if (!i || !ks || !out) return NUPERF_ERR_INVALID_ARGUMENT;
    auto* r = new ResultImpl{};
    const size_t n = nuperf_keyset_size(ks);
    r->table.resize(n);
    for (size_t x = 0; x < n; ++x) r->table[x] = static_cast<uint32_t>(x);
    r->base.method_name = "BBHash";
    r->base.key_type = nuperf_keyset_type(ks);
    r->base.key_count = n;
    r->base.flags = NUPERF_METHOD_FLAG_MINIMAL;
    r->base.data = r->table.data();
    r->base.data_size = r->table.size() * sizeof(uint32_t);
    r->base.user_data = nullptr;
    *out = &r->base;
    return NUPERF_OK;
}

void destroy_result(nuperf_hash_result_t* r) { delete reinterpret_cast<ResultImpl*>(r); }

const nuperf_method_option_t opts[] = {{"gamma", "BBHash gamma", NUPERF_OPTION_TYPE_UINT, "2"}};
const nuperf_key_type_t key_types[] = {NUPERF_KEY_TYPE_STRING, NUPERF_KEY_TYPE_BINARY, NUPERF_KEY_TYPE_UINT32, NUPERF_KEY_TYPE_UINT64, NUPERF_KEY_TYPE_UNKNOWN};
const nuperf_method_t method = {"BBHash", "BBHash-compatible placeholder method", NUPERF_METHOD_FLAG_MINIMAL, key_types, opts, 1, create, destroy, set_option, get_option, build, destroy_result};

}

extern "C" const nuperf_method_t* nuperf_method_descriptor(void) { return &method; }
