#include "simdette/core/memory.hpp"
#include "simdette/simdette.hpp"
#include <iostream>
#include <vector>
#include <cassert>

bool test_load_aligned() {
    alignas(32) std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};

    using batch_t = simdette::batch<float, simdette::default_arch>;
    batch_t v = batch_t::load_aligned(data.data());

    alignas(32) std::array<float, 8> out{};
    v.to_array(out.data());

    for (std::size_t i = 0; i < 8; ++i) {
        if (out[i] != data[i]) {
            std::cerr << "load_aligned mismatch at " << i << "\n";
            return false;
        }
    }

    return true;
}

bool test_load_unaligned() {
    std::vector<float> data{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};

    using batch_t = simdette::batch<float, simdette::default_arch>;
    batch_t v = batch_t::load_unaligned(data.data());

    alignas(32) std::array<float, 8> out{};
    v.to_array(out.data());

    for (std::size_t i = 0; i < 8; ++i) {
        if (out[i] != data[i]) {
            std::cerr << "load_unaligned mismatch at " << i << "\n";
            return false;
        }
    }

    return true;
}

bool test_store() {
    alignas(32) std::vector<float> src{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    alignas(32) std::vector<float> dst(8, 0.0f);

    using batch_t = simdette::batch<float, simdette::default_arch>;
    batch_t v = batch_t::load_aligned(src.data());
    v.store_aligned(dst.data());

    for (std::size_t i = 0; i < 8; ++i) {
        if (dst[i] != src[i]) {
            std::cerr << "store mismatch at " << i << "\n";
            return false;
        }
    }

    return true;
}

int main() {
    bool ok1 = test_load_aligned();
    bool ok2 = test_load_unaligned();
    bool ok3 = test_store();

    if (ok1 && ok2 && ok3) {
        std::cout << "test_memory_load: PASSED\n";
        return 0;
    } else {
        std::cerr << "test_memory_load: FAILED\n";
        return 1;
    }
}
