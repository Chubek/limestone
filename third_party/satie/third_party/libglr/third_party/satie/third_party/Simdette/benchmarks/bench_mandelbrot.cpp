#include "simdette/simdette.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <complex>
#include <cmath>

int mandelbrot_scalar(float re, float im, int max_iter) {
    std::complex<float> z(0, 0), c(re, im);
    int iter = 0;
    while (iter < max_iter && std::abs(z) < 2.0f) {
        z = z * z + c;
        ++iter;
    }
    return iter;
}

int mandelbrot_simd(float re, float im, int max_iter) {
    // Scalar fallback for single-pixel (SIMD over pixels not implemented here)
    return mandelbrot_scalar(re, im, max_iter);
}

int main() {
    const int W = 1920, H = 1080, MAX_ITER = 1000;
    std::vector<int> grid(W * H);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float re = -2.5f + 2.0f * static_cast<float>(x) / W;
            float im = -1.0f + 1.5f * static_cast<float>(y) / H;
            grid[y * W + x] = mandelbrot_simd(re, im, MAX_ITER);
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    double ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    std::cout << "mandelbrot " << W << "x" << H << " at " << MAX_ITER << " iter: " << ns << " ns\n";

    int sum = 0;
    for (int v : grid) sum += v;
    std::cout << "grid sum (checksum): " << sum << "\n";

    return 0;
}
