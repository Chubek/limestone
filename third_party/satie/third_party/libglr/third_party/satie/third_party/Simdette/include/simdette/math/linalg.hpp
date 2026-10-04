#pragma once

#include "simdette/math/basic_math.hpp"
#include "simdette/math/transcendental.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

namespace simdette {

namespace detail {

template <typename T>
concept linalg_scalar = std::is_arithmetic_v<T>;

template <std::size_t N, std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE bool almost_zero(T value,
                                               T epsilon = std::numeric_limits<T>::epsilon()) noexcept {
    return simdette::abs(value) <= epsilon;
}

template <std::size_t N, std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE bool invert_gauss_jordan(const std::array<std::array<T, N>, N>& in,
                                                       std::array<std::array<T, N>, N>& out,
                                                       T epsilon = std::numeric_limits<T>::epsilon()) noexcept {
    std::array<std::array<T, N>, N> a = in;
    out = {};

    for (std::size_t i = 0; i < N; ++i) {
        out[i][i] = static_cast<T>(1);
    }

    for (std::size_t col = 0; col < N; ++col) {
        std::size_t pivot = col;
        T pivot_abs = simdette::abs(a[col][col]);

        for (std::size_t row = col + 1; row < N; ++row) {
            const T candidate_abs = simdette::abs(a[row][col]);
            if (candidate_abs > pivot_abs) {
                pivot = row;
                pivot_abs = candidate_abs;
            }
        }

        if (pivot_abs <= epsilon) {
            return false;
        }

        if (pivot != col) {
            std::swap(a[pivot], a[col]);
            std::swap(out[pivot], out[col]);
        }

        const T pivot_value = a[col][col];
        for (std::size_t j = 0; j < N; ++j) {
            a[col][j] /= pivot_value;
            out[col][j] /= pivot_value;
        }

        for (std::size_t row = 0; row < N; ++row) {
            if (row == col) {
                continue;
            }

            const T factor = a[row][col];
            if (factor == static_cast<T>(0)) {
                continue;
            }

            for (std::size_t j = 0; j < N; ++j) {
                a[row][j] -= factor * a[col][j];
                out[row][j] -= factor * out[col][j];
            }
        }
    }

    return true;
}

template <std::size_t N, std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T determinant_gaussian(const std::array<std::array<T, N>, N>& in,
                                                     T epsilon = std::numeric_limits<T>::epsilon()) noexcept {
    std::array<std::array<T, N>, N> a = in;
    T det = static_cast<T>(1);
    int sign = 1;

    for (std::size_t col = 0; col < N; ++col) {
        std::size_t pivot = col;
        T pivot_abs = simdette::abs(a[col][col]);

        for (std::size_t row = col + 1; row < N; ++row) {
            const T candidate_abs = simdette::abs(a[row][col]);
            if (candidate_abs > pivot_abs) {
                pivot = row;
                pivot_abs = candidate_abs;
            }
        }

        if (pivot_abs <= epsilon) {
            return static_cast<T>(0);
        }

        if (pivot != col) {
            std::swap(a[pivot], a[col]);
            sign = -sign;
        }

        const T pivot_value = a[col][col];
        det *= pivot_value;

        for (std::size_t row = col + 1; row < N; ++row) {
            const T factor = a[row][col] / pivot_value;
            for (std::size_t j = col + 1; j < N; ++j) {
                a[row][j] -= factor * a[col][j];
            }
        }
    }

    return (sign < 0) ? -det : det;
}

template <std::size_t N, typename VecT>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE std::array<std::array<typename VecT::value_type, N>, N>
extract_rows(const std::array<VecT, N>& rows) noexcept {
    using T = typename VecT::value_type;
    std::array<std::array<T, N>, N> out{};

    for (std::size_t r = 0; r < N; ++r) {
        for (std::size_t c = 0; c < N; ++c) {
            out[r][c] = rows[r][c];
        }
    }

    return out;
}

} // namespace detail

template <detail::linalg_scalar T>
struct vec2 {
    using value_type = T;

    T x{};
    T y{};

    constexpr vec2() noexcept = default;
    constexpr explicit vec2(T value) noexcept : x(value), y(value) {}
    constexpr vec2(T x_value, T y_value) noexcept : x(x_value), y(y_value) {}

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE T& operator[](std::size_t index) noexcept {
        return (&x)[index];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE T const& operator[](std::size_t index) const noexcept {
        return (&x)[index];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec2 operator+() const noexcept {
        return *this;
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec2 operator-() const noexcept {
        return vec2{-x, -y};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec2 operator+(vec2 const& other) const noexcept {
        return vec2{x + other.x, y + other.y};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec2 operator-(vec2 const& other) const noexcept {
        return vec2{x - other.x, y - other.y};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec2 operator*(T scalar) const noexcept {
        return vec2{x * scalar, y * scalar};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec2 operator/(T scalar) const noexcept {
        return vec2{x / scalar, y / scalar};
    }

    constexpr SIMDETTE_ALWAYS_INLINE vec2& operator+=(vec2 const& other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE vec2& operator-=(vec2 const& other) noexcept {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE vec2& operator*=(T scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE vec2& operator/=(T scalar) noexcept {
        x /= scalar;
        y /= scalar;
        return *this;
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE bool operator==(vec2 const& other) const noexcept {
        return x == other.x && y == other.y;
    }
};

template <detail::linalg_scalar T>
struct vec3 {
    using value_type = T;

    T x{};
    T y{};
    T z{};

    constexpr vec3() noexcept = default;
    constexpr explicit vec3(T value) noexcept : x(value), y(value), z(value) {}
    constexpr vec3(T x_value, T y_value, T z_value) noexcept : x(x_value), y(y_value), z(z_value) {}

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE T& operator[](std::size_t index) noexcept {
        return (&x)[index];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE T const& operator[](std::size_t index) const noexcept {
        return (&x)[index];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec3 operator+() const noexcept {
        return *this;
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec3 operator-() const noexcept {
        return vec3{-x, -y, -z};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec3 operator+(vec3 const& other) const noexcept {
        return vec3{x + other.x, y + other.y, z + other.z};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec3 operator-(vec3 const& other) const noexcept {
        return vec3{x - other.x, y - other.y, z - other.z};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec3 operator*(T scalar) const noexcept {
        return vec3{x * scalar, y * scalar, z * scalar};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec3 operator/(T scalar) const noexcept {
        return vec3{x / scalar, y / scalar, z / scalar};
    }

    constexpr SIMDETTE_ALWAYS_INLINE vec3& operator+=(vec3 const& other) noexcept {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE vec3& operator-=(vec3 const& other) noexcept {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE vec3& operator*=(T scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE vec3& operator/=(T scalar) noexcept {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE bool operator==(vec3 const& other) const noexcept {
        return x == other.x && y == other.y && z == other.z;
    }
};

template <detail::linalg_scalar T>
struct vec4 {
    using value_type = T;

    T x{};
    T y{};
    T z{};
    T w{};

    constexpr vec4() noexcept = default;
    constexpr explicit vec4(T value) noexcept : x(value), y(value), z(value), w(value) {}
    constexpr vec4(T x_value, T y_value, T z_value, T w_value) noexcept
        : x(x_value), y(y_value), z(z_value), w(w_value) {}

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE T& operator[](std::size_t index) noexcept {
        return (&x)[index];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE T const& operator[](std::size_t index) const noexcept {
        return (&x)[index];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec4 operator+() const noexcept {
        return *this;
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec4 operator-() const noexcept {
        return vec4{-x, -y, -z, -w};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec4 operator+(vec4 const& other) const noexcept {
        return vec4{x + other.x, y + other.y, z + other.z, w + other.w};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec4 operator-(vec4 const& other) const noexcept {
        return vec4{x - other.x, y - other.y, z - other.z, w - other.w};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec4 operator*(T scalar) const noexcept {
        return vec4{x * scalar, y * scalar, z * scalar, w * scalar};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE vec4 operator/(T scalar) const noexcept {
        return vec4{x / scalar, y / scalar, z / scalar, w / scalar};
    }

    constexpr SIMDETTE_ALWAYS_INLINE vec4& operator+=(vec4 const& other) noexcept {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE vec4& operator-=(vec4 const& other) noexcept {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE vec4& operator*=(T scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        w *= scalar;
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE vec4& operator/=(T scalar) noexcept {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        w /= scalar;
        return *this;
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE bool operator==(vec4 const& other) const noexcept {
        return x == other.x && y == other.y && z == other.z && w == other.w;
    }
};

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec2<T> operator*(T scalar, vec2<T> const& vector) noexcept {
    return vector * scalar;
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec3<T> operator*(T scalar, vec3<T> const& vector) noexcept {
    return vector * scalar;
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec4<T> operator*(T scalar, vec4<T> const& vector) noexcept {
    return vector * scalar;
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE T dot(vec2<T> const& a, vec2<T> const& b) noexcept {
    return a.x * b.x + a.y * b.y;
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE T dot(vec3<T> const& a, vec3<T> const& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE T dot(vec4<T> const& a, vec4<T> const& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

template <std::floating_point T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE T length_squared(vec2<T> const& vector) noexcept {
    return dot(vector, vector);
}

template <std::floating_point T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE T length_squared(vec3<T> const& vector) noexcept {
    return dot(vector, vector);
}

template <std::floating_point T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE T length_squared(vec4<T> const& vector) noexcept {
    return dot(vector, vector);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T length(vec2<T> const& vector) noexcept {
    return simdette::sqrt(length_squared(vector));
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T length(vec3<T> const& vector) noexcept {
    return simdette::sqrt(length_squared(vector));
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T length(vec4<T> const& vector) noexcept {
    return simdette::sqrt(length_squared(vector));
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE vec2<T> normalize(vec2<T> const& vector,
                                                T epsilon = std::numeric_limits<T>::epsilon()) noexcept {
    const T len = length(vector);
    if (simdette::abs(len) <= epsilon) {
        return vec2<T>{static_cast<T>(0)};
    }
    return vector / len;
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE vec3<T> normalize(vec3<T> const& vector,
                                                T epsilon = std::numeric_limits<T>::epsilon()) noexcept {
    const T len = length(vector);
    if (simdette::abs(len) <= epsilon) {
        return vec3<T>{static_cast<T>(0)};
    }
    return vector / len;
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE vec4<T> normalize(vec4<T> const& vector,
                                                T epsilon = std::numeric_limits<T>::epsilon()) noexcept {
    const T len = length(vector);
    if (simdette::abs(len) <= epsilon) {
        return vec4<T>{static_cast<T>(0)};
    }
    return vector / len;
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE vec2<T> normalize_or(vec2<T> const& vector,
                                                   vec2<T> const& fallback,
                                                   T epsilon = std::numeric_limits<T>::epsilon()) noexcept {
    const T len = length(vector);
    if (simdette::abs(len) <= epsilon) {
        return fallback;
    }
    return vector / len;
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE vec3<T> normalize_or(vec3<T> const& vector,
                                                   vec3<T> const& fallback,
                                                   T epsilon = std::numeric_limits<T>::epsilon()) noexcept {
    const T len = length(vector);
    if (simdette::abs(len) <= epsilon) {
        return fallback;
    }
    return vector / len;
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE vec4<T> normalize_or(vec4<T> const& vector,
                                                   vec4<T> const& fallback,
                                                   T epsilon = std::numeric_limits<T>::epsilon()) noexcept {
    const T len = length(vector);
    if (simdette::abs(len) <= epsilon) {
        return fallback;
    }
    return vector / len;
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec3<T> cross(vec3<T> const& a, vec3<T> const& b) noexcept {
    return vec3<T>{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec2<T> min(vec2<T> const& a, vec2<T> const& b) noexcept {
    return vec2<T>{simdette::min(a.x, b.x), simdette::min(a.y, b.y)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec3<T> min(vec3<T> const& a, vec3<T> const& b) noexcept {
    return vec3<T>{simdette::min(a.x, b.x), simdette::min(a.y, b.y), simdette::min(a.z, b.z)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec4<T> min(vec4<T> const& a, vec4<T> const& b) noexcept {
    return vec4<T>{simdette::min(a.x, b.x), simdette::min(a.y, b.y), simdette::min(a.z, b.z), simdette::min(a.w, b.w)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec2<T> max(vec2<T> const& a, vec2<T> const& b) noexcept {
    return vec2<T>{simdette::max(a.x, b.x), simdette::max(a.y, b.y)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec3<T> max(vec3<T> const& a, vec3<T> const& b) noexcept {
    return vec3<T>{simdette::max(a.x, b.x), simdette::max(a.y, b.y), simdette::max(a.z, b.z)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec4<T> max(vec4<T> const& a, vec4<T> const& b) noexcept {
    return vec4<T>{simdette::max(a.x, b.x), simdette::max(a.y, b.y), simdette::max(a.z, b.z), simdette::max(a.w, b.w)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec2<T> clamp(vec2<T> const& x, vec2<T> const& lo, vec2<T> const& hi) noexcept {
    return min(max(x, lo), hi);
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec3<T> clamp(vec3<T> const& x, vec3<T> const& lo, vec3<T> const& hi) noexcept {
    return min(max(x, lo), hi);
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec4<T> clamp(vec4<T> const& x, vec4<T> const& lo, vec4<T> const& hi) noexcept {
    return min(max(x, lo), hi);
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec2<T> abs(vec2<T> const& x) noexcept {
    return vec2<T>{simdette::abs(x.x), simdette::abs(x.y)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec3<T> abs(vec3<T> const& x) noexcept {
    return vec3<T>{simdette::abs(x.x), simdette::abs(x.y), simdette::abs(x.z)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec4<T> abs(vec4<T> const& x) noexcept {
    return vec4<T>{simdette::abs(x.x), simdette::abs(x.y), simdette::abs(x.z), simdette::abs(x.w)};
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE vec2<T> sqrt(vec2<T> const& x) noexcept {
    return vec2<T>{simdette::sqrt(x.x), simdette::sqrt(x.y)};
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE vec3<T> sqrt(vec3<T> const& x) noexcept {
    return vec3<T>{simdette::sqrt(x.x), simdette::sqrt(x.y), simdette::sqrt(x.z)};
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE vec4<T> sqrt(vec4<T> const& x) noexcept {
    return vec4<T>{simdette::sqrt(x.x), simdette::sqrt(x.y), simdette::sqrt(x.z), simdette::sqrt(x.w)};
}

template <detail::linalg_scalar T>
struct mat2 {
    using value_type = T;
    using row_type = vec2<T>;

    std::array<row_type, 2> rows{};

    constexpr mat2() noexcept = default;
    constexpr explicit mat2(T diagonal) noexcept
        : rows{row_type{diagonal, static_cast<T>(0)}, row_type{static_cast<T>(0), diagonal}} {}
    constexpr mat2(row_type const& r0, row_type const& r1) noexcept : rows{r0, r1} {}
    constexpr mat2(T m00, T m01, T m10, T m11) noexcept : rows{row_type{m00, m01}, row_type{m10, m11}} {}

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE row_type& operator[](std::size_t index) noexcept {
        return rows[index];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE row_type const& operator[](std::size_t index) const noexcept {
        return rows[index];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE T& operator()(std::size_t row, std::size_t col) noexcept {
        return rows[row][col];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE T const& operator()(std::size_t row, std::size_t col) const noexcept {
        return rows[row][col];
    }

    [[nodiscard]]
    static constexpr SIMDETTE_ALWAYS_INLINE mat2 identity() noexcept {
        return mat2{static_cast<T>(1)};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE mat2 operator+(mat2 const& other) const noexcept {
        return mat2{rows[0] + other.rows[0], rows[1] + other.rows[1]};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE mat2 operator-(mat2 const& other) const noexcept {
        return mat2{rows[0] - other.rows[0], rows[1] - other.rows[1]};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE mat2 operator*(T scalar) const noexcept {
        return mat2{rows[0] * scalar, rows[1] * scalar};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE mat2 operator/(T scalar) const noexcept {
        return mat2{rows[0] / scalar, rows[1] / scalar};
    }

    constexpr SIMDETTE_ALWAYS_INLINE mat2& operator+=(mat2 const& other) noexcept {
        rows[0] += other.rows[0];
        rows[1] += other.rows[1];
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE mat2& operator-=(mat2 const& other) noexcept {
        rows[0] -= other.rows[0];
        rows[1] -= other.rows[1];
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE mat2& operator*=(T scalar) noexcept {
        rows[0] *= scalar;
        rows[1] *= scalar;
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE mat2& operator/=(T scalar) noexcept {
        rows[0] /= scalar;
        rows[1] /= scalar;
        return *this;
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE bool operator==(mat2 const& other) const noexcept {
        return rows[0] == other.rows[0] && rows[1] == other.rows[1];
    }
};

template <detail::linalg_scalar T>
struct mat3 {
    using value_type = T;
    using row_type = vec3<T>;

    std::array<row_type, 3> rows{};

    constexpr mat3() noexcept = default;
    constexpr explicit mat3(T diagonal) noexcept
        : rows{row_type{diagonal, static_cast<T>(0), static_cast<T>(0)},
               row_type{static_cast<T>(0), diagonal, static_cast<T>(0)},
               row_type{static_cast<T>(0), static_cast<T>(0), diagonal}} {}
    constexpr mat3(row_type const& r0, row_type const& r1, row_type const& r2) noexcept : rows{r0, r1, r2} {}
    constexpr mat3(T m00,
                   T m01,
                   T m02,
                   T m10,
                   T m11,
                   T m12,
                   T m20,
                   T m21,
                   T m22) noexcept
        : rows{row_type{m00, m01, m02}, row_type{m10, m11, m12}, row_type{m20, m21, m22}} {}

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE row_type& operator[](std::size_t index) noexcept {
        return rows[index];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE row_type const& operator[](std::size_t index) const noexcept {
        return rows[index];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE T& operator()(std::size_t row, std::size_t col) noexcept {
        return rows[row][col];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE T const& operator()(std::size_t row, std::size_t col) const noexcept {
        return rows[row][col];
    }

    [[nodiscard]]
    static constexpr SIMDETTE_ALWAYS_INLINE mat3 identity() noexcept {
        return mat3{static_cast<T>(1)};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE mat3 operator+(mat3 const& other) const noexcept {
        return mat3{rows[0] + other.rows[0], rows[1] + other.rows[1], rows[2] + other.rows[2]};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE mat3 operator-(mat3 const& other) const noexcept {
        return mat3{rows[0] - other.rows[0], rows[1] - other.rows[1], rows[2] - other.rows[2]};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE mat3 operator*(T scalar) const noexcept {
        return mat3{rows[0] * scalar, rows[1] * scalar, rows[2] * scalar};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE mat3 operator/(T scalar) const noexcept {
        return mat3{rows[0] / scalar, rows[1] / scalar, rows[2] / scalar};
    }

    constexpr SIMDETTE_ALWAYS_INLINE mat3& operator+=(mat3 const& other) noexcept {
        rows[0] += other.rows[0];
        rows[1] += other.rows[1];
        rows[2] += other.rows[2];
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE mat3& operator-=(mat3 const& other) noexcept {
        rows[0] -= other.rows[0];
        rows[1] -= other.rows[1];
        rows[2] -= other.rows[2];
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE mat3& operator*=(T scalar) noexcept {
        rows[0] *= scalar;
        rows[1] *= scalar;
        rows[2] *= scalar;
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE mat3& operator/=(T scalar) noexcept {
        rows[0] /= scalar;
        rows[1] /= scalar;
        rows[2] /= scalar;
        return *this;
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE bool operator==(mat3 const& other) const noexcept {
        return rows[0] == other.rows[0] && rows[1] == other.rows[1] && rows[2] == other.rows[2];
    }
};

template <detail::linalg_scalar T>
struct mat4 {
    using value_type = T;
    using row_type = vec4<T>;

    std::array<row_type, 4> rows{};

    constexpr mat4() noexcept = default;
    constexpr explicit mat4(T diagonal) noexcept
        : rows{row_type{diagonal, static_cast<T>(0), static_cast<T>(0), static_cast<T>(0)},
               row_type{static_cast<T>(0), diagonal, static_cast<T>(0), static_cast<T>(0)},
               row_type{static_cast<T>(0), static_cast<T>(0), diagonal, static_cast<T>(0)},
               row_type{static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), diagonal}} {}
    constexpr mat4(row_type const& r0, row_type const& r1, row_type const& r2, row_type const& r3) noexcept
        : rows{r0, r1, r2, r3} {}
    constexpr mat4(T m00,
                   T m01,
                   T m02,
                   T m03,
                   T m10,
                   T m11,
                   T m12,
                   T m13,
                   T m20,
                   T m21,
                   T m22,
                   T m23,
                   T m30,
                   T m31,
                   T m32,
                   T m33) noexcept
        : rows{row_type{m00, m01, m02, m03},
               row_type{m10, m11, m12, m13},
               row_type{m20, m21, m22, m23},
               row_type{m30, m31, m32, m33}} {}

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE row_type& operator[](std::size_t index) noexcept {
        return rows[index];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE row_type const& operator[](std::size_t index) const noexcept {
        return rows[index];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE T& operator()(std::size_t row, std::size_t col) noexcept {
        return rows[row][col];
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE T const& operator()(std::size_t row, std::size_t col) const noexcept {
        return rows[row][col];
    }

    [[nodiscard]]
    static constexpr SIMDETTE_ALWAYS_INLINE mat4 identity() noexcept {
        return mat4{static_cast<T>(1)};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE mat4 operator+(mat4 const& other) const noexcept {
        return mat4{rows[0] + other.rows[0], rows[1] + other.rows[1], rows[2] + other.rows[2], rows[3] + other.rows[3]};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE mat4 operator-(mat4 const& other) const noexcept {
        return mat4{rows[0] - other.rows[0], rows[1] - other.rows[1], rows[2] - other.rows[2], rows[3] - other.rows[3]};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE mat4 operator*(T scalar) const noexcept {
        return mat4{rows[0] * scalar, rows[1] * scalar, rows[2] * scalar, rows[3] * scalar};
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE mat4 operator/(T scalar) const noexcept {
        return mat4{rows[0] / scalar, rows[1] / scalar, rows[2] / scalar, rows[3] / scalar};
    }

    constexpr SIMDETTE_ALWAYS_INLINE mat4& operator+=(mat4 const& other) noexcept {
        rows[0] += other.rows[0];
        rows[1] += other.rows[1];
        rows[2] += other.rows[2];
        rows[3] += other.rows[3];
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE mat4& operator-=(mat4 const& other) noexcept {
        rows[0] -= other.rows[0];
        rows[1] -= other.rows[1];
        rows[2] -= other.rows[2];
        rows[3] -= other.rows[3];
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE mat4& operator*=(T scalar) noexcept {
        rows[0] *= scalar;
        rows[1] *= scalar;
        rows[2] *= scalar;
        rows[3] *= scalar;
        return *this;
    }

    constexpr SIMDETTE_ALWAYS_INLINE mat4& operator/=(T scalar) noexcept {
        rows[0] /= scalar;
        rows[1] /= scalar;
        rows[2] /= scalar;
        rows[3] /= scalar;
        return *this;
    }

    [[nodiscard]]
    constexpr SIMDETTE_ALWAYS_INLINE bool operator==(mat4 const& other) const noexcept {
        return rows[0] == other.rows[0] && rows[1] == other.rows[1] && rows[2] == other.rows[2] && rows[3] == other.rows[3];
    }
};

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat2<T> operator*(T scalar, mat2<T> const& matrix) noexcept {
    return matrix * scalar;
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat3<T> operator*(T scalar, mat3<T> const& matrix) noexcept {
    return matrix * scalar;
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat4<T> operator*(T scalar, mat4<T> const& matrix) noexcept {
    return matrix * scalar;
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec2<T> operator*(mat2<T> const& matrix, vec2<T> const& vector) noexcept {
    return vec2<T>{dot(matrix[0], vector), dot(matrix[1], vector)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec3<T> operator*(mat3<T> const& matrix, vec3<T> const& vector) noexcept {
    return vec3<T>{dot(matrix[0], vector), dot(matrix[1], vector), dot(matrix[2], vector)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE vec4<T> operator*(mat4<T> const& matrix, vec4<T> const& vector) noexcept {
    return vec4<T>{dot(matrix[0], vector), dot(matrix[1], vector), dot(matrix[2], vector), dot(matrix[3], vector)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat2<T> operator*(mat2<T> const& a, mat2<T> const& b) noexcept {
    mat2<T> out{};
    const mat2<T> bt = mat2<T>{vec2<T>{b(0, 0), b(1, 0)}, vec2<T>{b(0, 1), b(1, 1)}};

    for (std::size_t r = 0; r < 2; ++r) {
        for (std::size_t c = 0; c < 2; ++c) {
            out(r, c) = dot(a[r], bt[c]);
        }
    }

    return out;
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat3<T> operator*(mat3<T> const& a, mat3<T> const& b) noexcept {
    mat3<T> out{};
    const mat3<T> bt = mat3<T>{
        vec3<T>{b(0, 0), b(1, 0), b(2, 0)},
        vec3<T>{b(0, 1), b(1, 1), b(2, 1)},
        vec3<T>{b(0, 2), b(1, 2), b(2, 2)},
    };

    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t c = 0; c < 3; ++c) {
            out(r, c) = dot(a[r], bt[c]);
        }
    }

    return out;
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat4<T> operator*(mat4<T> const& a, mat4<T> const& b) noexcept {
    mat4<T> out{};
    const mat4<T> bt = mat4<T>{
        vec4<T>{b(0, 0), b(1, 0), b(2, 0), b(3, 0)},
        vec4<T>{b(0, 1), b(1, 1), b(2, 1), b(3, 1)},
        vec4<T>{b(0, 2), b(1, 2), b(2, 2), b(3, 2)},
        vec4<T>{b(0, 3), b(1, 3), b(2, 3), b(3, 3)},
    };

    for (std::size_t r = 0; r < 4; ++r) {
        for (std::size_t c = 0; c < 4; ++c) {
            out(r, c) = dot(a[r], bt[c]);
        }
    }

    return out;
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat2<T> transpose(mat2<T> const& matrix) noexcept {
    return mat2<T>{matrix(0, 0), matrix(1, 0), matrix(0, 1), matrix(1, 1)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat3<T> transpose(mat3<T> const& matrix) noexcept {
    return mat3<T>{matrix(0, 0),
                   matrix(1, 0),
                   matrix(2, 0),
                   matrix(0, 1),
                   matrix(1, 1),
                   matrix(2, 1),
                   matrix(0, 2),
                   matrix(1, 2),
                   matrix(2, 2)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat4<T> transpose(mat4<T> const& matrix) noexcept {
    return mat4<T>{matrix(0, 0),
                   matrix(1, 0),
                   matrix(2, 0),
                   matrix(3, 0),
                   matrix(0, 1),
                   matrix(1, 1),
                   matrix(2, 1),
                   matrix(3, 1),
                   matrix(0, 2),
                   matrix(1, 2),
                   matrix(2, 2),
                   matrix(3, 2),
                   matrix(0, 3),
                   matrix(1, 3),
                   matrix(2, 3),
                   matrix(3, 3)};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE T determinant(mat2<T> const& matrix) noexcept {
    return matrix(0, 0) * matrix(1, 1) - matrix(0, 1) * matrix(1, 0);
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE T determinant(mat3<T> const& matrix) noexcept {
    const T c00 = matrix(1, 1) * matrix(2, 2) - matrix(1, 2) * matrix(2, 1);
    const T c01 = matrix(1, 0) * matrix(2, 2) - matrix(1, 2) * matrix(2, 0);
    const T c02 = matrix(1, 0) * matrix(2, 1) - matrix(1, 1) * matrix(2, 0);
    return matrix(0, 0) * c00 - matrix(0, 1) * c01 + matrix(0, 2) * c02;
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE T determinant(mat4<T> const& matrix) noexcept {
    const auto rows = detail::extract_rows<4>(matrix.rows);
    return detail::determinant_gaussian<4, T>(rows);
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE mat2<T> inverse(mat2<T> const& matrix,
                                              T epsilon = std::numeric_limits<T>::epsilon()) noexcept {
    const T det = determinant(matrix);
    if (simdette::abs(det) <= epsilon) {
        return mat2<T>{};
    }

    return mat2<T>{
        matrix(1, 1) / det,
        -matrix(0, 1) / det,
        -matrix(1, 0) / det,
        matrix(0, 0) / det,
    };
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE mat3<T> inverse(mat3<T> const& matrix,
                                              T epsilon = std::numeric_limits<T>::epsilon()) noexcept {
    const T det = determinant(matrix);
    if (simdette::abs(det) <= epsilon) {
        return mat3<T>{};
    }

    mat3<T> out{};
    out(0, 0) = (matrix(1, 1) * matrix(2, 2) - matrix(1, 2) * matrix(2, 1)) / det;
    out(0, 1) = (matrix(0, 2) * matrix(2, 1) - matrix(0, 1) * matrix(2, 2)) / det;
    out(0, 2) = (matrix(0, 1) * matrix(1, 2) - matrix(0, 2) * matrix(1, 1)) / det;

    out(1, 0) = (matrix(1, 2) * matrix(2, 0) - matrix(1, 0) * matrix(2, 2)) / det;
    out(1, 1) = (matrix(0, 0) * matrix(2, 2) - matrix(0, 2) * matrix(2, 0)) / det;
    out(1, 2) = (matrix(0, 2) * matrix(1, 0) - matrix(0, 0) * matrix(1, 2)) / det;

    out(2, 0) = (matrix(1, 0) * matrix(2, 1) - matrix(1, 1) * matrix(2, 0)) / det;
    out(2, 1) = (matrix(0, 1) * matrix(2, 0) - matrix(0, 0) * matrix(2, 1)) / det;
    out(2, 2) = (matrix(0, 0) * matrix(1, 1) - matrix(0, 1) * matrix(1, 0)) / det;

    return out;
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE mat4<T> inverse(mat4<T> const& matrix,
                                              T epsilon = std::numeric_limits<T>::epsilon()) noexcept {
    const auto rows = detail::extract_rows<4>(matrix.rows);
    std::array<std::array<T, 4>, 4> inv_rows{};

    if (!detail::invert_gauss_jordan<4, T>(rows, inv_rows, epsilon)) {
        return mat4<T>{};
    }

    return mat4<T>{
        inv_rows[0][0], inv_rows[0][1], inv_rows[0][2], inv_rows[0][3],
        inv_rows[1][0], inv_rows[1][1], inv_rows[1][2], inv_rows[1][3],
        inv_rows[2][0], inv_rows[2][1], inv_rows[2][2], inv_rows[2][3],
        inv_rows[3][0], inv_rows[3][1], inv_rows[3][2], inv_rows[3][3],
    };
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE mat2<T> rotation_matrix(T radians) noexcept {
    const T c = simdette::cos(radians);
    const T s = simdette::sin(radians);
    return mat2<T>{c, -s, s, c};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat2<T> diagonal_matrix(vec2<T> const& diagonal) noexcept {
    return mat2<T>{diagonal.x, static_cast<T>(0), static_cast<T>(0), diagonal.y};
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat3<T> diagonal_matrix(vec3<T> const& diagonal) noexcept {
    return mat3<T>{
        diagonal.x, static_cast<T>(0), static_cast<T>(0),
        static_cast<T>(0), diagonal.y, static_cast<T>(0),
        static_cast<T>(0), static_cast<T>(0), diagonal.z,
    };
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat4<T> diagonal_matrix(vec4<T> const& diagonal) noexcept {
    return mat4<T>{
        diagonal.x, static_cast<T>(0), static_cast<T>(0), static_cast<T>(0),
        static_cast<T>(0), diagonal.y, static_cast<T>(0), static_cast<T>(0),
        static_cast<T>(0), static_cast<T>(0), diagonal.z, static_cast<T>(0),
        static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), diagonal.w,
    };
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat3<T> homogeneous_translation(vec2<T> const& offset) noexcept {
    return mat3<T>{
        static_cast<T>(1), static_cast<T>(0), offset.x,
        static_cast<T>(0), static_cast<T>(1), offset.y,
        static_cast<T>(0), static_cast<T>(0), static_cast<T>(1),
    };
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat4<T> homogeneous_translation(vec3<T> const& offset) noexcept {
    return mat4<T>{
        static_cast<T>(1), static_cast<T>(0), static_cast<T>(0), offset.x,
        static_cast<T>(0), static_cast<T>(1), static_cast<T>(0), offset.y,
        static_cast<T>(0), static_cast<T>(0), static_cast<T>(1), offset.z,
        static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(1),
    };
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat3<T> homogeneous_scale(vec2<T> const& factors) noexcept {
    return mat3<T>{
        factors.x, static_cast<T>(0), static_cast<T>(0),
        static_cast<T>(0), factors.y, static_cast<T>(0),
        static_cast<T>(0), static_cast<T>(0), static_cast<T>(1),
    };
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat4<T> homogeneous_scale(vec3<T> const& factors) noexcept {
    return mat4<T>{
        factors.x, static_cast<T>(0), static_cast<T>(0), static_cast<T>(0),
        static_cast<T>(0), factors.y, static_cast<T>(0), static_cast<T>(0),
        static_cast<T>(0), static_cast<T>(0), factors.z, static_cast<T>(0),
        static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(1),
    };
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE mat3<T> homogeneous_rotation(T radians) noexcept {
    const T c = simdette::cos(radians);
    const T s = simdette::sin(radians);

    return mat3<T>{
        c, -s, static_cast<T>(0),
        s, c, static_cast<T>(0),
        static_cast<T>(0), static_cast<T>(0), static_cast<T>(1),
    };
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE mat3<T> rotation_x(T radians) noexcept {
    const T c = simdette::cos(radians);
    const T s = simdette::sin(radians);
    return mat3<T>{
        static_cast<T>(1), static_cast<T>(0), static_cast<T>(0),
        static_cast<T>(0), c, -s,
        static_cast<T>(0), s, c,
    };
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE mat3<T> rotation_y(T radians) noexcept {
    const T c = simdette::cos(radians);
    const T s = simdette::sin(radians);
    return mat3<T>{
        c, static_cast<T>(0), s,
        static_cast<T>(0), static_cast<T>(1), static_cast<T>(0),
        -s, static_cast<T>(0), c,
    };
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE mat3<T> rotation_z(T radians) noexcept {
    const T c = simdette::cos(radians);
    const T s = simdette::sin(radians);
    return mat3<T>{
        c, -s, static_cast<T>(0),
        s, c, static_cast<T>(0),
        static_cast<T>(0), static_cast<T>(0), static_cast<T>(1),
    };
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE mat4<T> homogeneous_rotation_x(T radians) noexcept {
    const mat3<T> r = rotation_x(radians);
    return mat4<T>{
        r(0, 0), r(0, 1), r(0, 2), static_cast<T>(0),
        r(1, 0), r(1, 1), r(1, 2), static_cast<T>(0),
        r(2, 0), r(2, 1), r(2, 2), static_cast<T>(0),
        static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(1),
    };
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE mat4<T> homogeneous_rotation_y(T radians) noexcept {
    const mat3<T> r = rotation_y(radians);
    return mat4<T>{
        r(0, 0), r(0, 1), r(0, 2), static_cast<T>(0),
        r(1, 0), r(1, 1), r(1, 2), static_cast<T>(0),
        r(2, 0), r(2, 1), r(2, 2), static_cast<T>(0),
        static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(1),
    };
}

template <std::floating_point T>
[[nodiscard]]
inline SIMDETTE_ALWAYS_INLINE mat4<T> homogeneous_rotation_z(T radians) noexcept {
    const mat3<T> r = rotation_z(radians);
    return mat4<T>{
        r(0, 0), r(0, 1), r(0, 2), static_cast<T>(0),
        r(1, 0), r(1, 1), r(1, 2), static_cast<T>(0),
        r(2, 0), r(2, 1), r(2, 2), static_cast<T>(0),
        static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(1),
    };
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat3<T> basis_from_columns(vec3<T> const& x,
                                                            vec3<T> const& y,
                                                            vec3<T> const& z) noexcept {
    return mat3<T>{
        x.x, y.x, z.x,
        x.y, y.y, z.y,
        x.z, y.z, z.z,
    };
}

template <detail::linalg_scalar T>
[[nodiscard]]
constexpr SIMDETTE_ALWAYS_INLINE mat3<T> basis_from_rows(vec3<T> const& x,
                                                         vec3<T> const& y,
                                                         vec3<T> const& z) noexcept {
    return mat3<T>{x, y, z};
}

using fvec2 = vec2<float>;
using fvec3 = vec3<float>;
using fvec4 = vec4<float>;

using dvec2 = vec2<double>;
using dvec3 = vec3<double>;
using dvec4 = vec4<double>;

using ivec2 = vec2<int>;
using ivec3 = vec3<int>;
using ivec4 = vec4<int>;

using fmat2 = mat2<float>;
using fmat3 = mat3<float>;
using fmat4 = mat4<float>;

using dmat2 = mat2<double>;
using dmat3 = mat3<double>;
using dmat4 = mat4<double>;

} // namespace simdette
