// ============================================================================
// Simdette Type Traits Tests
// ============================================================================

#include "simdette/core/traits.hpp"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "=== Simdette Type Traits Tests ===\n\n";
    
    // ========================================================================
    // Test is_simd_compatible
    // ========================================================================
    std::cout << "Testing is_simd_compatible:\n";
    
    static_assert(simdette::is_simd_compatible<float>::value, "float should be SIMD compatible");
    static_assert(simdette::is_simd_compatible<double>::value, "double should be SIMD compatible");
    static_assert(simdette::is_simd_compatible<int32_t>::value, "int32_t should be SIMD compatible");
    static_assert(simdette::is_simd_compatible<uint32_t>::value, "uint32_t should be SIMD compatible");
    
    static_assert(!simdette::is_simd_compatible<double*>::value, "pointer should not be SIMD compatible");
    static_assert(!simdette::is_simd_compatible<void>::value, "void should not be SIMD compatible");
    
    std::cout << "  is_simd_compatible: PASS\n";
    
    // ========================================================================
    // Test is_floating_point
    // ========================================================================
    std::cout << "Testing is_floating_point:\n";
    
    static_assert(simdette::is_floating_point<float>::value, "float should be floating point");
    static_assert(simdette::is_floating_point<double>::value, "double should be floating point");
    static_assert(!simdette::is_floating_point<int32_t>::value, "int32_t should not be floating point");
    
    std::cout << "  is_floating_point: PASS\n";
    
    // ========================================================================
    // Test is_integral
    // ========================================================================
    std::cout << "Testing is_integral:\n";
    
    static_assert(simdette::is_integral<int32_t>::value, "int32_t should be integral");
    static_assert(simdette::is_integral<uint32_t>::value, "uint32_t should be integral");
    static_assert(!simdette::is_integral<float>::value, "float should not be integral");
    
    std::cout << "  is_integral: PASS\n";
    
    // ========================================================================
    // Test is_signed_integral / is_unsigned_integral
    // ========================================================================
    std::cout << "Testing is_signed_integral / is_unsigned_integral:\n";
    
    static_assert(simdette::is_signed_integral<int32_t>::value, "int32_t should be signed");
    static_assert(!simdette::is_unsigned_integral<int32_t>::value, "int32_t should not be unsigned");
    static_assert(!simdette::is_signed_integral<uint32_t>::value, "uint32_t should not be signed");
    static_assert(simdette::is_unsigned_integral<uint32_t>::value, "uint32_t should be unsigned");
    
    std::cout << "  is_signed_integral / is_unsigned_integral: PASS\n";
    
    // ========================================================================
    // Test bit_width
    // ========================================================================
    std::cout << "Testing bit_width:\n";
    
    static_assert(simdette::bit_width_v<float> == 32, "float should be 32 bits");
    static_assert(simdette::bit_width_v<double> == 64, "double should be 64 bits");
    static_assert(simdette::bit_width_v<int8_t> == 8, "int8_t should be 8 bits");
    static_assert(simdette::bit_width_v<int32_t> == 32, "int32_t should be 32 bits");
    static_assert(simdette::bit_width_v<int64_t> == 64, "int64_t should be 64 bits");
    
    std::cout << "  bit_width: PASS\n";
    
    // ========================================================================
    // Test signed_type / unsigned_type
    // ========================================================================
    std::cout << "Testing signed_type / unsigned_type:\n";
    
    static_assert(std::is_same<simdette::signed_type_t<uint8_t>, int8_t>::value, "uint8_t -> int8_t");
    static_assert(std::is_same<simdette::signed_type_t<uint16_t>, int16_t>::value, "uint16_t -> int16_t");
    static_assert(std::is_same<simdette::signed_type_t<uint32_t>, int32_t>::value, "uint32_t -> int32_t");
    static_assert(std::is_same<simdette::signed_type_t<uint64_t>, int64_t>::value, "uint64_t -> int64_t");
    
    static_assert(std::is_same<simdette::unsigned_type_t<int8_t>, uint8_t>::value, "int8_t -> uint8_t");
    static_assert(std::is_same<simdette::unsigned_type_t<int16_t>, uint16_t>::value, "int16_t -> uint16_t");
    static_assert(std::is_same<simdette::unsigned_type_t<int32_t>, uint32_t>::value, "int32_t -> uint32_t");
    static_assert(std::is_same<simdette::unsigned_type_t<int64_t>, uint64_t>::value, "int64_t -> uint64_t");
    
    std::cout << "  signed_type / unsigned_type: PASS\n";
    
    // ========================================================================
    // Test float_type
    // ========================================================================
    std::cout << "Testing float_type:\n";
    
    static_assert(std::is_same<simdette::float_type_t<float>, float>::value, "float_type<float> = float");
    static_assert(std::is_same<simdette::float_type_t<double>, double>::value, "float_type<double> = double");
    static_assert(std::is_same<simdette::float_type_t<int32_t>, float>::value, "float_type<int32_t> = float");
    
    std::cout << "  float_type: PASS\n";
    
    // ========================================================================
    // Test simd_alignment
    // ========================================================================
    std::cout << "Testing simd_alignment:\n";
    
    // Alignment should be at least 32 for SIMD types
    static_assert(simdette::simd_alignment_v<float> >= 32, "float alignment >= 32");
    static_assert(simdette::simd_alignment_v<double> >= 32, "double alignment >= 32");
    static_assert(simdette::simd_alignment_v<int32_t> >= 32, "int32_t alignment >= 32");
    
    std::cout << "  simd_alignment: PASS\n";
    
    // ========================================================================
    // Test is_simdable
    // ========================================================================
    std::cout << "Testing is_simdable():\n";
    
    static_assert(simdette::is_simdable<float>(), "float should be simdable");
    static_assert(simdette::is_simdable<int32_t>(), "int32_t should be simdable");
    static_assert(!simdette::is_simdable<double*>(), "pointer should not be simdable");
    
    std::cout << "  is_simdable(): PASS\n";
    
    // ========================================================================
    // Test vector_256_count / vector_512_count
    // ========================================================================
    std::cout << "Testing vector_256_count / vector_512_count:\n";
    
    static_assert(simdette::vector_256_count<float>() == 8, "8 floats in 256 bits");
    static_assert(simdette::vector_256_count<double>() == 4, "4 doubles in 256 bits");
    static_assert(simdette::vector_512_count<float>() == 16, "16 floats in 512 bits");
    static_assert(simdette::vector_512_count<double>() == 8, "8 doubles in 512 bits");
    
    std::cout << "  vector_256_count / vector_512_count: PASS\n";
    
    // ========================================================================
    // Test has_simd_add / has_simd_mul / has_simd_compare
    // ========================================================================
    std::cout << "Testing has_simd_*:\n";
    
    static_assert(simdette::has_simd_add<float>::value, "float should support SIMD add");
    static_assert(simdette::has_simd_mul<double>::value, "double should support SIMD mul");
    static_assert(simdette::has_simd_compare<int32_t>::value, "int32_t should support SIMD compare");
    
    std::cout << "  has_simd_*: PASS\n";
    
    // ========================================================================
    // Summary
    // ========================================================================
    std::cout << "\n=== All Traits Tests Passed ===\n";
    
    return 0;
}
