#pragma once

#include "simdette/core/batch.hpp"

#include <arm_sve.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>

namespace simdette {

namespace detail {

template <>
struct batch_width<float, sve> {
    static constexpr std::size_t value = SZBF1(f32) / sizeof(float);
};

template <>
struct batch_width<int32_t, sve> {
    static constexpr std::size_t value = SZBF1(i32) / sizeof(int32_t);
};

template <>
struct batch_width<uint32_t, sve> {
    static constexpr std::size_t value = SZBF1(u32) / sizeof(uint32_t);
};

} // namespace detail

template <>
struct batch<float, sve> {
    using value_type = float;
    using arch_type = sve;

    static constexpr std::size_t width = detail::batch_width_v<float, sve>;

    svfloat32_t value;

    SIMDETTE_ALWAYS_INLINE batch() noexcept = default;

    SIMDETTE_ALWAYS_INLINE explicit batch(svfloat32_t v) noexcept : value(v) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(float scalar) noexcept {
        value = svdup_n_f32(scalar);
    }

    SIMDETTE_ALWAYS_INLINE explicit batch(float const* ptr) noexcept {
        svfloat32_t temp = svld1_f32(svtrue_b32(), ptr);
        value = temp;
    }

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_aligned(float const* ptr) noexcept {
        return batch(svld1_f32(svtrue_b32(), ptr));
    }

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_unaligned(float const* ptr) noexcept {
        return batch(svld1_f32(svtrue_b32(), ptr));
    }

    SIMDETTE_ALWAYS_INLINE void store_aligned(float* ptr) noexcept const {
        svst1_f32(svtrue_b32(), ptr, value);
    }

    SIMDETTE_ALWAYS_INLINE void store_unaligned(float* ptr) noexcept const {
        svst1_f32(svtrue_b32(), ptr, value);
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float extract(std::size_t index) const noexcept {
        alignas(64) std::array<float, width> lanes{};
        svst1_f32(svtrue_b32(), lanes.data(), value);
        return lanes[index];
    }

    SIMDETTE_ALWAYS_INLINE void to_array(float* ptr) const noexcept {
        svst1_f32(svtrue_b32(), ptr, value);
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+(batch const& o) const noexcept {
        return batch(svpadd_f32(svtrue_b32(), value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-(batch const& o) const noexcept {
        return batch(svsub_f32(svtrue_b32(), value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator*(batch const& o) const noexcept {
        return batch(svmul_f32(svtrue_b32(), value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator/(batch const& o) const noexcept {
        return batch(svpdiv_f32(svtrue_b32(), value, o.value));
    }

    SIMDETTE_ALWAYS_INLINE batch& operator+=(batch const& o) noexcept {
        value = svpadd_f32(svtrue_b32(), value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator-=(batch const& o) noexcept {
        value = svsub_f32(svtrue_b32(), value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator*=(batch const& o) noexcept {
        value = svmul_f32(svtrue_b32(), value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator/=(batch const& o) noexcept {
        value = svpdiv_f32(svtrue_b32(), value, o.value);
        return *this;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<(batch const& o) const noexcept {
        return batch(svzip1_f32(svzip1_f32(svtrue_b32(), svlt_f32(svtrue_b32(), value, o.value),
                                          svfalse_f32()), svfalse_f32()));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<=(batch const& o) const noexcept {
        return batch(svzip1_f32(svzip1_f32(svtrue_b32(), svle_f32(svtrue_b32(), value, o.value),
                                            svfalse_f32()), svfalse_f32()));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>(batch const& o) const noexcept {
        return batch(svzip1_f32(svzip1_f32(svtrue_b32(), svgt_f32(svtrue_b32(), value, o.value),
                                          svfalse_f32()), svfalse_f32()));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>=(batch const& o) const noexcept {
        return batch(svzip1_f32(svzip1_f32(svtrue_b32(), svge_f32(svtrue_b32(), value, o.value),
                                            svfalse_f32()), svfalse_f32()));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator==(batch const& o) const noexcept {
        return batch(svzip1_f32(svzip1_f32(svtrue_b32(), sveq_f32(svtrue_b32(), value, o.value),
                                          svfalse_f32()), svfalse_f32()));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!=(batch const& o) const noexcept {
        return batch(svzip1_f32(svzip1_f32(svtrue_b32(), svne_f32(svtrue_b32(), value, o.value),
                                          svfalse_f32()), svfalse_f32()));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&&(batch const& o) const noexcept {
        svbool_t mask = svand_b32(svtrue(), svcmpne_z_f32_n_f32(svfalse(), value, 0.0f));
        svbool_t mask2 = svand_b32(svtrue(), svcmpne_z_f32_n_f32(svfalse(), o.value, 0.0f));
        return batch(svpadd_f32(mask & mask2, value, value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator||(batch const& o) const noexcept {
        svbool_t mask = svorr_b32(svtrue(), svcmpne_z_f32_n_f32(svfalse(), value, 0.0f),
                                  svcmpne_z_f32_n_f32(svfalse(), o.value, 0.0f));
        return batch(svpadd_f32(mask, value, value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!() const noexcept {
        svbool_t mask = svcmpeq_z_f32(svfalse(), value, 0.0f);
        return batch(svzip1_f32(svzip1_f32(mask, svfalse_f32()), svfalse_f32()));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-() const noexcept {
        return batch(svsub_f32(svtrue(), svdup_n_f32(0.0f), value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+() const noexcept { return *this; }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_add() const noexcept {
        alignas(64) std::array<float, width> lanes{};
        svst1_f32(svtrue_b32(), lanes.data(), value);
        float sum = 0.0f;
        for (size_t i = 0; i < width; ++i) sum += lanes[i];
        return sum;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_min() const noexcept {
        alignas(64) std::array<float, width> lanes{};
        svst1_f32(svtrue_b32(), lanes.data(), value);
        float mn = lanes[0];
        for (size_t i = 1; i < width; ++i) mn = std::min(mn, lanes[i]);
        return mn;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_max() const noexcept {
        alignas(64) std::array<float, width> lanes{};
        svst1_f32(svtrue_b32(), lanes.data(), value);
        float mx = lanes[0];
        for (size_t i = 1; i < width; ++i) mx = std::max(mx, lanes[i]);
        return mx;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool all() const noexcept {
        svbool_t mask = svcmpeq_z_f32(svfalse(), value, 0.0f);
        return svmaxv_n_f32(svtrue(), value) != 0.0f && svminv_n_f32(svtrue(), value) != 0.0f;
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool any() const noexcept {
        return svmaxv_n_f32(svtrue(), value) != 0.0f;
    }
};

} // namespace simdette
