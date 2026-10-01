# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.1.x   | ✓         |
| < 0.1   | ✗         |

## Reporting a Vulnerability

If you discover a security vulnerability in Simdette, please report it privately.

### How to Report

1. **Do not** create a public GitHub issue
2. Email security@simdette.org (or the maintainer)
3. Include:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fixes (if any)

### What to Expect

- Acknowledgment within 48 hours
- Regular updates on progress
- Public disclosure after fix is released
- Credit in release notes (if desired)

## Security Features

### Type Safety

Simdette uses C++20 concepts for type safety:

```cpp
template <typename T>
concept simd_compatible = is_simd_compatible_v<T>;

template <simd_compatible T, typename Arch>
struct batch { ... };
```

### Memory Safety

- RAII wrappers for aligned memory
- Automatic cleanup
- Bounds checking in debug mode

### Alignment

Proper alignment detection prevents:
- Segmentation faults
- Performance degradation
- Undefined behavior

## Best Practices

### 1. Initialize Batches

```cpp
// Good
batch<float, arch> v(0.0f);

// Bad - uninitialized
batch<float, arch> v;
```

### 2. Align Memory

```cpp
// Good
alignas(32) float data[8];

// Bad - potential misalignment
float data[8];
```

### 3. Validate Input

```cpp
SIMD_ASSERT(data != nullptr);
SIMD_ASSERT(n > 0);
```

### 4. Handle Special Values

```cpp
// Check for NaN/Inf
if (simdute::isnan(value).any()) {
    // Handle specially
}
```

## Known Issues

### No Known Vulnerabilities

As of this writing, Simdette has no known security vulnerabilities.

### Architecture-Specific Notes

Some architectures (AVX-512) may have CPU-specific mitigations:
- Consult CPU vendor documentation
- Use appropriate compiler flags
- Test on target hardware

## Audit Trail

All security-related changes are:
- Documented in changelog
- Tested with existing test suite
- Reviewed by maintainers

## Compliance

Simdette follows:
- C++ Core Guidelines
- CWE/SANS Top 25
- OWASP guidelines where applicable

## Credits

Security improvements come from:
- Community reports
- Code reviews
- Automated analysis tools
- Security audits (when available)
