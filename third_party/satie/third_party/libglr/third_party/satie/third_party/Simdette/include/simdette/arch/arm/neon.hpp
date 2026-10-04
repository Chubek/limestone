#pragma once

#include "simdette/core/batch.hpp"

#include <arm_neon.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>

namespace simdette {

namespace detail {

template <>
struct batch_width<float, neon> {
    static constexpr std::size_t value = 4;
};

template <>
struct batch_width<double, neon> {
    static constexpr std::size_t value = 2;
};

template <>
struct batch_width<int32_t, neon> {
    static constexpr std::size_t value = 4;
};

template <>
struct batch_width<uint32_t, neon> {
    static constexpr std::size_t value = 4;
};

} // namespace detail

template <>
struct batch<float, neon> {
    using value_type = float;
    using arch_type = neon;

    static constexpr std::size_t width = detail::batch_width_v<float, neon>;

    float32x4_t value;

    SIMDETTE_ALWAYS_INLINE batch() noexcept = default;

    SIMDETTE_ALWAYS_INLINE explicit batch(float32x4_t v) noexcept : value(v) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(float scalar) noexcept
        : value(vdupq_n_f32(scalar)) {}

    SIMDETTE_ALWAYS_INLINE explicit batch(float const* ptr) noexcept
        : value(vld1q_f32(ptr)) {}

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_aligned(float const* ptr) noexcept {
        return batch(vld1q_f32(ptr));
    }

    [[nodiscard]]
    static SIMDETTE_ALWAYS_INLINE batch load_unaligned(float const* ptr) noexcept {
        return batch(vld1q_f32(ptr));
    }

    SIMDETTE_ALWAYS_INLINE void store_aligned(float* ptr) noexcept const {
        vst1q_f32(ptr, value);
    }

    SIMDETTE_ALWAYS_INLINE void store_unaligned(float* ptr) noexcept const {
        vst1q_f32(ptr, value);
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float extract(std::size_t index) const noexcept {
        alignas(16) std::array<float, width> lanes{};
        vst1q_f32(lanes.data(), value);
        return lanes[index];
    }

    SIMDETTE_ALWAYS_INLINE void to_array(float* ptr) const noexcept {
        vst1q_f32(ptr, value);
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+(batch const& o) const noexcept {
        return batch(vaddq_f32(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-(batch const& o) const noexcept {
        return batch(vsubq_f32(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator*(batch const& o) const noexcept {
        return batch(vmulq_f32(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator/(batch const& o) const noexcept {
        return batch(vdivq_f32(value, o.value));
    }

    SIMDETTE_ALWAYS_INLINE batch& operator+=(batch const& o) noexcept {
        value = vaddq_f32(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator-=(batch const& o) noexcept {
        value = vsubq_f32(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator*=(batch const& o) noexcept {
        value = vmulq_f32(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator/=(batch const& o) noexcept {
        value = vdivq_f32(value, o.value);
        return *this;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<(batch const& o) const noexcept {
        return batch(vcltq_f32(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator<=(batch const& o) const noexcept {
        return batch(vcletq_f32(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>(batch const& o) const noexcept {
        return batch(vcgtq_f32(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator>=(batch const& o) const noexcept {
        return batch(vcgeq_f32(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator==(batch const& o) const noexcept {
        return batch(vceqq_f32(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!=(batch const& o) const noexcept {
        return batch(vmvnq_f32(vceqq_f32(value, o.value)));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&(batch const& o) const noexcept {
        return batch(vandq_f32(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator|(batch const& o) const noexcept {
        return batch(vorrq_f32(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator^(batch const& o) const noexcept {
        return batch(veorq_f32(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator~() const noexcept {
        return batch(veorq_f32(value, vreinterpretq_f32_u32(vmvnq_u32(vreinterpretq_u32_f32(value)))));
    }

    SIMDETTE_ALWAYS_INLINE batch& operator&=(batch const& o) noexcept {
        value = vandq_f32(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator|=(batch const& o) noexcept {
        value = vorrq_f32(value, o.value);
        return *this;
    }
    SIMDETTE_ALWAYS_INLINE batch& operator^=(batch const& o) noexcept {
        value = veorq_f32(value, o.value);
        return *this;
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator&&(batch const& o) const noexcept {
        return batch(vandq_f32(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator||(batch const& o) const noexcept {
        return batch(vorrq_f32(value, o.value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator!() const noexcept {
        return batch(vceqq_f32(value, vdupq_n_f32(0.0f)));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator-() const noexcept {
        return batch(vnegq_f32(value));
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE batch operator+() const noexcept { return *this; }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_add() const noexcept {
        alignas(16) std::array<float, width> lanes{};
        vst1q_f32(lanes.data(), value);
        return lanes[0] + lanes[1] + lanes[2] + lanes[3];
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_min() const noexcept {
        alignas(16) std::array<float, width> lanes{};
        vst1q_f32(lanes.data(), value);
        return std::min(std::min(lanes[0], lanes[1]), std::min(lanes[2], lanes[3]));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE float reduce_max() const noexcept {
        alignas(16) std::array<float, width> lanes{};
        vst1q_f32(lanes.data(), value);
        return std::max(std::max(lanes[0], lanes[1]), std::max(lanes[2], lanes[3]));
    }

    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool all() const noexcept {
        uint32x4_t mask = vcgeq_f32(value, vdupq_n_f32(0.0f));
        uint32x4_t ones = vdupq_n_u32(0xFFFFFFFF);
        return vgetq_lane_u32(vandq_u32(mask, ones), 0) &&
               vgetq_lane_u32(vandq_u32(mask, ones), 1) &&
               vgetq_lane_u32(vandq_u32(mask, ones), 2) &&
               vgetq_lane_u32(vandq_u32(mask, ones), 3);
    }
    [[nodiscard]]
    SIMDETTE_ALWAYS_INLINE bool any() const noexcept {
        uint32x4_t mask = vcgtq_f32(vdupq_n_f32(0.0f), value);
        return vmaxvq_u32(vorrq_u32(vmovl_u16(vget_low_u16(vreinterpretq_u16_u32(mask))),
                                    vget_high_u16(vreinterpretq_u16_u32(mask)))) != 0;
    }
};

} // namespace simdette
