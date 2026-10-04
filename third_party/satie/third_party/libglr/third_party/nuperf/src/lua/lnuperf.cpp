/**
 * @file lnuperf.cpp
 * @brief Lua C module entry point for NuPERF
 * 
 * Provides the luaopen_lnuperf() entry point for loading NuPERF as a
 * Lua C module. Can be used both as a standalone .so/.dll module and
 * embedded directly into applications.
 */

#include "nuperf/nuperf-api.h"
#include "nuperf/nuperf-types.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

inline void throw_on_error(nuperf_status_t st) {
    if (st != NUPERF_OK) {
        throw sol::error(nuperf_strerror(st));
    }
}

/**
 * @brief RAII wrapper for nuperf_keyset_t exposed to Lua
 */
class LuaKeyset {
public:
    LuaKeyset() {
        throw_on_error(nuperf_keyset_create(&ks_));
    }

    ~LuaKeyset() {
        if (ks_) {
            nuperf_keyset_destroy(ks_);
            ks_ = nullptr;
        }
    }

    LuaKeyset(const LuaKeyset&) = delete;
    LuaKeyset& operator=(const LuaKeyset&) = delete;

    LuaKeyset(LuaKeyset&& other) noexcept : ks_(other.ks_) {
        other.ks_ = nullptr;
    }

    LuaKeyset& operator=(LuaKeyset&& other) noexcept {
        if (this != &other) {
            if (ks_) {
                nuperf_keyset_destroy(ks_);
            }
            ks_ = other.ks_;
            other.ks_ = nullptr;
        }
        return *this;
    }

    void reserve(std::size_t n) {
        throw_on_error(nuperf_keyset_reserve(ks_, n));
    }

    void clear() {
        nuperf_keyset_clear(ks_);
    }

    void add_string(const std::string& s) {
        throw_on_error(nuperf_keyset_add_string(ks_, s.c_str(), s.size()));
    }

    void add_binary(const sol::object& obj) {
        if (obj.is<std::string>()) {
            const std::string s = obj.as<std::string>();
            throw_on_error(
                nuperf_keyset_add_binary(
                    ks_,
                    reinterpret_cast<const uint8_t*>(s.data()),
                    s.size()));
            return;
        }

        if (obj.get_type() == sol::type::table) {
            sol::table t = obj.as<sol::table>();
            std::vector<uint8_t> bytes;
            bytes.reserve(t.size());
            for (std::size_t i = 1; i <= t.size(); ++i) {
                bytes.push_back(static_cast<uint8_t>(t.get<int>(i)));
            }
            throw_on_error(
                nuperf_keyset_add_binary(
                    ks_,
                    bytes.data(),
                    bytes.size()));
            return;
        }

        throw sol::error("add_binary expects a string or array-like table of bytes");
    }

    void add_u32(uint32_t v) {
        throw_on_error(nuperf_keyset_add_u32(ks_, v));
    }

    void add_u64(uint64_t v) {
        throw_on_error(nuperf_keyset_add_u64(ks_, v));
    }

    std::size_t size() const {
        return nuperf_keyset_size(ks_);
    }

    int key_type() const {
        return static_cast<int>(nuperf_keyset_key_type(ks_));
    }

    std::string get_string(std::size_t index) const {
        const char* data = nullptr;
        std::size_t len = 0;
        throw_on_error(nuperf_keyset_get_string(ks_, index, &data, &len));
        return std::string(data, len);
    }

    std::string get_binary(std::size_t index) const {
        const uint8_t* data = nullptr;
        std::size_t len = 0;
        throw_on_error(nuperf_keyset_get_binary(ks_, index, &data, &len));
        return std::string(reinterpret_cast<const char*>(data), len);
    }

    uint32_t get_u32(std::size_t index) const {
        uint32_t value = 0;
        throw_on_error(nuperf_keyset_get_u32(ks_, index, &value));
        return value;
    }

    uint64_t get_u64(std::size_t index) const {
        uint64_t value = 0;
        throw_on_error(nuperf_keyset_get_u64(ks_, index, &value));
        return value;
    }

    nuperf_keyset_t* handle() const { return ks_; }

private:
    nuperf_keyset_t* ks_ = nullptr;
};

/**
 * @brief RAII wrapper for nuperf_table_t exposed to Lua
 */
class LuaTable {
public:
    LuaTable() {
        throw_on_error(nuperf_table_create(&tbl_));