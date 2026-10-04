// src/util/hash.cpp

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace nuperf::util {

namespace {

constexpr uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037ull;
constexpr uint64_t FNV_PRIME_64 = 1099511628211ull;

constexpr uint32_t FMIX32_C1 = 0x85ebca6bu;
constexpr uint32_t FMIX32_C2 = 0xc2b2ae35u;

inline uint32_t rotl32(uint32_t x, int r) noexcept {
    return (x << r) | (x >> (32 - r));
}

inline uint32_t read_u32_le(const void* p) noexcept {
    uint32_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

inline uint64_t read_u64_le(const void* p) noexcept {
    uint64_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

inline uint32_t fmix32(uint32_t h) noexcept {
    h ^= h >> 16;
    h *= FMIX32_C1;
    h ^= h >> 13;
    h *= FMIX32_C2;
    h ^= h >> 16;
    return h;
}

} // namespace

uint64_t fnv1a_64(const void* data, std::size_t size) noexcept {
    const auto* p = static_cast<const uint8_t*>(data);
    uint64_t h = FNV_OFFSET_BASIS_64;

    for (std::size_t i = 0; i < size; ++i) {
        h ^= static_cast<uint64_t>(p[i]);
        h *= FNV_PRIME_64;
    }

    return h;
}

uint64_t fnv1a_64(const char* s) noexcept {
    if (!s) {
        return FNV_OFFSET_BASIS_64;
    }

    uint64_t h = FNV_OFFSET_BASIS_64;
    while (*s) {
        h ^= static_cast<uint8_t>(*s++);
        h *= FNV_PRIME_64;
    }

    return h;
}

uint32_t murmur3_32(const void* data, std::size_t size, uint32_t seed) noexcept {
    const auto* bytes = static_cast<const uint8_t*>(data);
    const std::size_t nblocks = size / 4;

    uint32_t h1 = seed;

    constexpr uint32_t c1 = 0xcc9e2d51u;
    constexpr uint32_t c2 = 0x1b873593u;

    for (std::size_t i = 0; i < nblocks; ++i) {
        uint32_t k1 = read_u32_le(bytes + i * 4);

        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = rotl32(h1, 13);
        h1 = h1 * 5u + 0xe6546b64u;
    }

    const auto* tail = bytes + nblocks * 4;
    uint32_t k1 = 0;

    switch (size & 3u) {
        case 3:
            k1 ^= static_cast<uint32_t>(tail[2]) << 16;
            [[fallthrough]];
        case 2:
            k1 ^= static_cast<uint32_t>(tail[1]) << 8;
            [[fallthrough]];
        case 1:
            k1 ^= static_cast<uint32_t>(tail[0]);
            k1 *= c1;
            k1 = rotl32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
            [[fallthrough]];
        default:
            break;
    }

    h1 ^= static_cast<uint32_t>(size);
    return fmix32(h1);
}

uint32_t hash_bytes(const void* data, std::size_t size, uint32_t seed) noexcept {
    return murmur3_32(data, size, seed);
}

uint32_t hash_string(const char* s, uint32_t seed) noexcept {
    if (!s) {
        return murmur3_32("", 0, seed);
    }

    return murmur3_32(s, std::strlen(s), seed);
}

uint32_t hash_u32(uint32_t value, uint32_t seed) noexcept {
    return murmur3_32(&value, sizeof(value), seed);
}

uint32_t hash_u64(uint64_t value, uint32_t seed) noexcept {
    return murmur3_32(&value, sizeof(value), seed);
}

} // namespace nuperf::util
