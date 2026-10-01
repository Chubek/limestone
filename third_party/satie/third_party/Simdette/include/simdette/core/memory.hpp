#pragma once

#include "simdette/setup/config.hpp"
#include "simdette/core/traits.hpp"

#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>

namespace simdette {

// ============================================================================
// Alignment constants
// ============================================================================

/**
 * @brief Minimum alignment required for SIMD operations (32 bytes for AVX2).
 */
inline constexpr std::size_t Alignment = 32;

/**
 * @brief Minimum alignment required for AVX512 operations (64 bytes).
 */
inline constexpr std::size_t AVX512_Alignment = 64;

// ============================================================================
// Aligned memory allocation utilities
// ============================================================================

/**
 * @brief Allocate aligned memory for a given type.
 * 
 * @tparam T The type to allocate memory for.
 * @tparam N Number of elements.
 * @return Pointer to aligned memory (must be freed with aligned_free).
 */
template <typename T, std::size_t N = 1>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE T* aligned_alloc() noexcept {
    using AlignType = typename std::aligned_storage<
        sizeof(T) * N,
        Alignment
    >::type;
    
    AlignType* storage = reinterpret_cast<AlignType*>(
        std::malloc(sizeof(AlignType))
    );
    
    if (storage == nullptr) {
        std::abort();
    }
    
    return reinterpret_cast<T*>(storage);
}

/**
 * @brief Free aligned memory allocated with aligned_alloc.
 */
template <typename T>
SIMDETTE_ALWAYS_INLINE void aligned_free(T* ptr) noexcept {
    std::free(ptr);
}

// ============================================================================
// Helper functions for aligned allocation
// ============================================================================

/**
 * @brief Allocate an array of aligned elements.
 * 
 * @tparam T The element type.
 * @param count Number of elements to allocate.
 * @return Pointer to aligned memory.
 */
template <typename T>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE T* make_aligned_array(std::size_t count) noexcept {
    std::size_t bytes = sizeof(T) * count;
    std::size_t aligned_bytes = (bytes + Alignment - 1) & ~(Alignment - 1);
    
    T* ptr = reinterpret_cast<T*>(
        std::aligned_alloc(Alignment, aligned_bytes)
    );
    
    if (ptr == nullptr) {
        std::abort();
    }
    
    return ptr;
}

/**
 * @brief Deallocate an array allocated with make_aligned_array.
 */
template <typename T>
SIMDETTE_ALWAYS_INLINE void free_aligned_array(T* ptr) noexcept {
    std::free(ptr);
}

// ============================================================================
// Helper functions for vectorized loads/stores
// ============================================================================

/**
 * @brief Load a single aligned value.
 */
template <typename T>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE T load_aligned(T const* ptr) noexcept {
    return *ptr;
}

/**
 * @brief Load a single unaligned value.
 */
template <typename T>
[[nodiscard]]
SIMDETTE_ALWAYS_INLINE T load_unaligned(T const* ptr) noexcept {
    return *ptr;
}

/**
 * @brief Store a single value with alignment.
 */
template <typename T>
SIMDETTE_ALWAYS_INLINE void store_aligned(T* ptr, T value) noexcept {
    *ptr = value;
}

/**
 * @brief Store a single value without alignment requirement.
 */
template <typename T>
SIMDETTE_ALWAYS_INLINE void store_unaligned(T* ptr, T value) noexcept {
    *ptr = value;
}

/**
 * @brief Store a single value (convenience function).
 */
template <typename T>
SIMDETTE_ALWAYS_INLINE void store(T* ptr, T value) noexcept {
    store_aligned(ptr, value);
}

// ============================================================================
// Memory utilities
// ============================================================================

/**
 * @brief Copy memory with alignment hint.
 */
template <typename T>
SIMDETTE_ALWAYS_INLINE void memcpy_aligned(T* dest, T const* src, std::size_t count) noexcept {
    std::memcpy(dest, src, sizeof(T) * count);
}

/**
 * @brief Set memory to zero with alignment hint.
 */
template <typename T>
SIMDETTE_ALWAYS_INLINE void memset_aligned(T* ptr, std::size_t count) noexcept {
    std::memset(ptr, 0, sizeof(T) * count);
}

/**
 * @brief Broadcast a scalar value to a memory range.
 */
template <typename T>
SIMDETTE_ALWAYS_INLINE void fill_aligned(T* ptr, T value, std::size_t count) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        ptr[i] = value;
    }
}

// ============================================================================
// Safe array wrapper with automatic cleanup
// ============================================================================

/**
 * @brief RAII wrapper for aligned arrays.
 * 
 * @tparam T The element type.
 */
template <typename T>
class aligned_array {
public:
    /**
     * @brief Construct empty array.
     */
    aligned_array() noexcept : data_(nullptr), size_(0) {}

    /**
     * @brief Construct array with given size.
     * @param size Number of elements.
     */
    explicit aligned_array(std::size_t size) noexcept : data_(nullptr), size_(0) {
        allocate(size);
    }

    /**
     * @brief Construct array with given size and initial value.
     * @param size Number of elements.
     * @param value Initial value for all elements.
     */
    aligned_array(std::size_t size, T value) noexcept : data_(nullptr), size_(0) {
        allocate(size);
        if (data_) {
            fill_aligned(data_, value, size);
        }
    }

    /**
     * @brief Move constructor.
     */
    aligned_array(aligned_array&& other) noexcept 
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    /**
     * @brief Move assignment.
     */
    aligned_array& operator=(aligned_array&& other) noexcept {
        if (this != &other) {
            deallocate();
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    /**
     * @brief Destructor - frees allocated memory.
     */
    ~aligned_array() noexcept {
        deallocate();
    }

    // Disable copy operations
    aligned_array(aligned_array const&) = delete;
    aligned_array& operator=(aligned_array const&) = delete;

    /**
     * @brief Get pointer to data.
     */
    [[nodiscard]]
    T* data() noexcept {
        return data_;
    }

    /**
     * @brief Get pointer to const data.
     */
    [[nodiscard]]
    T const* data() const noexcept {
        return data_;
    }

    /**
     * @brief Get size of array.
     */
    [[nodiscard]]
    std::size_t size() const noexcept {
        return size_;
    }

    /**
     * @brief Check if array is empty.
     */
    [[nodiscard]]
    bool empty() const noexcept {
        return size_ == 0;
    }

    /**
     * @brief Get reference to element at index.
     */
    [[nodiscard]]
    T& operator[](std::size_t index) noexcept {
        return data_[index];
    }

    /**
     * @brief Get const reference to element at index.
     */
    [[nodiscard]]
    T const& operator[](std::size_t index) const noexcept {
        return data_[index];
    }

    /**
     * @brief Reset array to zero.
     */
    void reset() noexcept {
        deallocate();
        allocate(size_);
        memset_aligned(data_, size_);
    }

    /**
     * @brief Set all elements to a value.
     */
    void fill(T value) noexcept {
        fill_aligned(data_, value, size_);
    }

private:
    void allocate(std::size_t size) noexcept {
        size_ = size;
        if (size > 0) {
            data_ = make_aligned_array<T>(size);
        }
    }

    void deallocate() noexcept {
        if (data_) {
            free_aligned_array(data_);
            data_ = nullptr;
        }
        size_ = 0;
    }

    T* data_;
    std::size_t size_;
};

// ============================================================================
// Type trait for aligned storage
// ============================================================================

namespace detail {

/**
 * * @brief Get the aligned storage size for a type.
 */
template <typename T>
struct aligned_storage_size : std::integral_constant<std::size_t,
    (sizeof(T) + Alignment - 1) & ~(Alignment - 1)> {};

} // namespace detail

} // namespace simdette
