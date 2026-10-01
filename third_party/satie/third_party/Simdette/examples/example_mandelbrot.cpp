// ============================================================================
// Simdette Example: Mandelbrot Set Generation
// ============================================================================
// This example demonstrates a compute-intensive SIMD application: generating
// the Mandelbrot fractal set using parallel computation.
// ============================================================================

#include "simdette/simdette.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <chrono>
#include <cstdint>

// ============================================================================
// Scalar Implementation
// ============================================================================

int mandelbrot_scalar(std::complex<double> c, int max_iterations) {
    std::complex<double> z(0.0, 0.0);
    int iteration = 0;
    
    while (iteration < max_iterations && std::abs(z) < 2.0) {
        z = z * z + c;
        ++iteration;
    }
    
    return iteration;
}

// ============================================================================
// SIMD Implementation (using complex number decomposition)
// ============================================================================

// We'll process 4 points in parallel using SIMD
// Each point: (x, y) -> complex(z_real, z_imag)
// For 4 points: [z0_real, z1_real, z2_real, z3_real] and [z0_imag, ...]

int mandelbrot_simd_4(std::complex<double> c0, std::complex<double> c1,
                      std::complex<double> c2, std::complex<double> c3,
                      int max_iterations) {
    using f64x2 = simdette::batch<double, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<double, simdette::default_arch>;
    
    // Initialize real and imaginary parts for 4 points
    f64x2 zr0{c0.real(), c1.real()}, zi0{c0.imag(), c1.imag()};
    f64x2 zr1{c2.real(), c3.real()}, zi1{c2.imag(), c3.imag()};
    
    f64x2 zr_prev0, zr_prev1, zi_prev0, zi_prev1;
    f64x2 cr{c0.real(), c2.real()}, ci{c0.imag(), c2.imag()};
    
    int iterations[4] = {0, 0, 0, 0};
    
    for (int iter = 0; iter < max_iterations; ++iter) {
        // z = z^2 + c
        // (a + bi)^2 = (a^2 - b^2) + 2abi
        f64x2 zr_sq0 = simdette::mul(zr0, zr0);
        f64x2 zi_sq0 = simdette::mul(zi0, zi0);
        f64x2 zr2 = zr_sq0 - zi_sq0;
        f64x2 zi2 = simdette::mul(simdette::mul(f64x2(2.0), zr0), zi0);
        
        zr0 = zr2 + cr;
        zi0 = zi2 + ci;
        
        f64x2 zr_sq1 = simdette::mul(zr1, zr1);
        f64x2 zi_sq1 = simdette::mul(zi1, zi1);
        f64x2 zr2_1 = zr_sq1 - zi_sq1;
        f64x2 zi2_1 = simdette::mul(simdette::mul(f64x2(2.0), zr1), zi1);
        
        zr1 = zr2_1 + cr;
        zi1 = zi2_1 + ci;
        
        // Check magnitude: z.real^2 + z.imag^2 > 4
        f64x2 mag0 = simdette::add(simdette::mul(zr0, zr0), simdette::mul(zi0, zi0));
        f64x2 mag1 = simdette::add(simdette::mul(zr1, zr1), simdette::mul(zi1, zi1));
        
        f64x2 escape0 = mag0 > f64x2(4.0);
        f64x2 escape1 = mag1 > f64x2(4.0);
        
        // Store iterations for escaped points
        alignas(32) std::array<double, 2> e0, e1;
        escape0.to_array(e0.data());
        escape1.to_array(e1.data());
        
        // Note: This is a simplified SIMD implementation
        // A full implementation would need more complex mask handling
    }
    
    return max_iterations;  // Placeholder
}

// ============================================================================
// Vectorized Mandelbrot (pixel-wise parallelism)
// ============================================================================

struct MandelbrotConfig {
    double re_min, re_max;
    double im_min, im_max;
    int width, height;
    int max_iterations;
};

std::vector<uint8_t> mandelbrot_simd(const MandelbrotConfig& config) {
    using f64x2 = simdette::batch<double, simdette::default_arch>;
    constexpr std::size_t width = simdette::detail::batch_width_v<double, simdette::default_arch>;
    
    std::vector<uint8_t> pixels(config.width * config.height);
    
    double re_step = (config.re_max - config.re_min) / config.width;
    double im_step = (config.im_max - config.im_min) / config.height;
    
    for (int y = 0; y < config.height; ++y) {
        double im_c = config.im_min + y * im_step;
        
        for (int x = 0; x < config.width; x += width) {
            // Process up to 2 points at a time (double precision)
            int pixels_processed = std::min(width, config.width - x);
            
            // Initialize real parts
            alignas(32) std::array<double, 2> re_arr{};
            for (int i = 0; i < pixels_processed; ++i) {
                re_arr[i] = config.re_min + (x + i) * re_step;
            }
            
            // Initialize imaginary parts
            alignas(32) std::array<double, 2> im_arr{};
            for (int i = 0; i < pixels_processed; ++i) {
                im_arr[i] = im_c;
            }
            
            f64x2 zr(re_arr.data()), zi(im_arr.data());
            f64x2 zr_init(re_arr.data()), zi_init(im_arr.data());
            
            int iteration = 0;
            for (; iteration < config.max_iterations; ++iteration) {
                // z = z^2 + c
                f64x2 zr_sq = simdette::mul(zr, zr);
                f64x2 zi_sq = simdette::mul(zi, zi);
                f64x2 zr_new = zr_sq - zi_sq + zr_init;
                f64x2 zi_new = simdette::mul(simdette::mul(f64x2(2.0), zr), zi) + zi_init;
                
                zr = zr_new;
                zi = zi_new;
                
                // Check escape condition: |z|^2 > 4
                f64x2 mag = simdette::add(simdette::mul(zr, zr), simdette::mul(zi, zi));
                if (simdette::any(mag > f64x2(4.0))) {
                    break;
                }
            }
            
            // Store results
            alignas(32) std::array<double, 2> result_re, result_im;
            zr.to_array(result_re.data());
            zi.to_array(result_im.data());
            
            for (int i = 0; i < pixels_processed; ++i) {
                pixels[(y * config.width + x + i)] = static_cast<uint8_t>(iteration);
            }
        }
    }
    
    return pixels;
}

// ============================================================================
// Coloring
// ============================================================================

uint32_t color_from_iteration(int iteration, int max_iterations) {
    if (iteration == max_iterations) {
        return 0xFF000000;  // Black for interior
    }
    
    // Smooth coloring
    float t = static_cast<float>(iteration) / max_iterations;
    
    uint8_t r = static_cast<uint8_t>(sin(t * 10.0f + 0.0f) * 127.5f + 127.5f);
    uint8_t g = static_cast<uint8_t>(sin(t * 10.0f + 2.0f) * 127.5f + 127.5f);
    uint8_t b = static_cast<uint8_t>(sin(t * 10.0f + 4.0f) * 127.5f + 127.5f);
    
    return (255 << 24) | (b << 16) | (g << 8) | r;
}

// ============================================================================
// ASCII Visualization
// ============================================================================

void print_ascii(const std::vector<uint8_t>& pixels, int width, int height) {
    std::cout << "Mandelbrot Set (ASCII):\n";
    std::cout << "========================\n\n";
    
    const char* chars = " .:-=+*#%@";
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            int intensity = pixels[idx];
            int char_idx = intensity * (11 - 1) / 255;
            char_idx = std::min(char_idx, 10);
            std::cout << chars[char_idx];
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Simdette Mandelbrot Example ===\n\n";
    
    MandelbrotConfig config{
        -2.5, 1.0,    // Real range
        -1.5, 1.5,    // Imaginary range
        80, 60,       // Resolution (ASCII display)
        100           // Max iterations
    };
    
    std::cout << "Configuration:\n";
    std::cout << "  Real range: [" << config.re_min << ", " << config.re_max << "]\n";
    std::cout << "  Imag range: [" << config.im_min << ", " << config.im_max << "]\n";
    std::cout << "  Resolution: " << config.width << "x" << config.height << "\n";
    std::cout << "  Max iterations: " << config.max_iterations << "\n\n";
    
    // Generate Mandelbrot set
    std::cout << "Generating Mandelbrot set...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    auto pixels = mandelbrot_simd(config);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Generation time: " << duration << " ms\n\n";
    
    // Print ASCII representation
    print_ascii(pixels, config.width, config.height);
    
    // Color histogram
    std::vector<int> histogram(256, 0);
    for (uint8_t p : pixels) {
        histogram[p]++;
    }
    
    std::cout << "Iteration histogram (first 20 bins):\n";
    for (int i = 0; i < 20; ++i) {
        std::cout << "  Iteration " << i << ": " << histogram[i] << " pixels\n";
    }
    
    std::cout << "\n=== Mandelbrot Example Complete ===\n";
    
    return 0;
}
