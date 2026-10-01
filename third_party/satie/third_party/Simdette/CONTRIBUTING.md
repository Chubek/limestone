# Contributing to Simdette

Thank you for your interest in contributing to Simdette! This document provides guidelines for contributing.

## Code of Conduct

- Be respectful and inclusive
- Focus on constructive feedback
- Welcome newcomers
- Maintain professional communication

## Areas for Contribution

### Documentation

- **User Manual**: Improve existing content, add examples
- **API Reference**: Update Doxygen comments
- **Examples**: Create new usage examples
- **Translations**: Localize documentation

### Code

- **Bug Fixes**: Address issues in the codebase
- **New Features**: Propose and implement enhancements
- **Performance**: Optimize existing code
- **Tests**: Improve test coverage

### Tooling

- **Build Systems**: Add/improve CMake, Meson, etc.
- **CI/CD**: Improve automation
- **Quality Tools**: Add linters, formatters

## Getting Started

### 1. Fork and Clone

```bash
git clone https://github.com/your-username/simdette.git
cd simdette
```

### 2. Configure

```bash
cmake -S . -B build -DSIMDETTE_BUILD_TESTS=ON -DSIMDETTE_BUILD_EXAMPLES=ON
cmake --build build
```

### 3. Run Tests

```bash
ctest --test-dir build --output-on-failure
```

### 4. Make Changes

- Follow coding style (see below)
- Add tests for new features
- Update documentation
- Keep changes minimal and focused

## Coding Guidelines

### Style

- Use 4-space indentation
- Max line length: 100 characters
- Meaningful variable names
- Consistent naming conventions

### C++20 Features

- Use concepts for type constraints
- Use `std::format` for formatting
- Prefer `constexpr` where possible
- Use `if constexpr` for compile-time branching

### Examples

```cpp
// Good: Clear naming and structure
template <simd_compatible T, typename Arch = default_arch>
void process_vector(const T* input, T* output, std::size_t n) {
    using batch_t = simdette::batch<T, Arch>;
    constexpr std::size_t width = detail::batch_width_v<T, Arch>;
    
    // Process in blocks
    for (std::size_t i = 0; i < n; i += width) {
        // ...
    }
}
```

## Commit Messages

Follow conventional commits:

```
type(scope): description

[optional body]

[optional footer]
```

**Types**:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `style`: Formatting
- `refactor`: Code restructuring
- `test`: Tests
- `chore`: Maintenance

**Examples**:

```
feat(batch): add reduce_min and reduce_max operations

Add horizontal reduction operations for min and max.
These operations collapse a batch into a single scalar.

Closes #123
```

```
docs(manual): improve getting started section

Add more detailed installation instructions and
clearer examples for different build systems.
```

## Pull Request Process

### 1. Before Submitting

- [ ] Tests pass (`ctest`)
- [ ] Code follows style guidelines
- [ ] Documentation updated
- [ ] Changelog updated
- [ ] No new compiler warnings
- [ ] Performance tested (if applicable)

### 2. PR Checklist

- [ ] Describe changes clearly
- [ ] Link related issues
- [ ] Include test cases
- [ ] Update documentation
- [ ] Verify CI passes

### 3. Review Process

1. Automated checks run
2. Maintainer review
3. Address feedback
4. Merge or request changes

## Testing

### Unit Tests

Write comprehensive tests for new features:

```cpp
bool test_feature() {
    // Setup
    batch<float, arch> a{1, 2, 3, 4};
    
    // Execute
    auto result = process(a);
    
    // Verify
    alignas(32) std::array<float, 4> out;
    result.to_array(out.data());
    
    return out[0] == 2.0f && out[1] == 4.0f;
}
```

### Test Coverage

Aim for:
- All new code covered
- Edge cases tested
- Performance regression tests

### Integration Tests

Test with real-world scenarios:
- Different architectures
- Various data sizes
- Memory alignment edge cases

## Documentation Standards

### Doxygen Comments

```cpp
/**
 * @brief Compute element-wise square root
 * 
 * This function applies sqrt to each element of the batch.
 * 
 * @tparam T Element type (must be floating-point)
 * @tparam Arch SIMD architecture
 * @param x Input batch
 * @return Batch with square roots
 * 
 * @example
 * batch<float, avx2> v{1, 4, 9, 16};
 * auto r = sqrt(v);  // [1, 2, 3, 4]
 */
template <std::floating_point T, typename Arch>
batch<T, Arch> sqrt(batch<T, Arch> const& x) noexcept;
```

### User Guide

- Clear examples
- Step-by-step instructions
- Common pitfalls and solutions
- Performance tips

## Release Process

### Versioning

Follow [Semantic Versioning](https://semver.org/):
- MAJOR: Breaking changes
- MINOR: New features (backward compatible)
- PATCH: Bug fixes

### Releasing

1. Update version in `CMakeLists.txt`
2. Update `CHANGELOG.md`
3. Tag release: `git tag -a v0.1.1 -m "Version 0.1.1"`
4. Push: `git push origin v0.1.1`
5. Create GitHub release

## Questions?

- **General**: Use GitHub Discussions
- **Bugs**: Create issue with template
- **Security**: See [SECURITY.md](SECURITY.md)

## Thank You!

Your contributions make Simdette better. We appreciate:
- Bug reports
- Feature suggestions
- Code improvements
- Documentation enhancements
- Community support

## Resources

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [Conventional Commits](https://www.conventionalcommits.org/)
- [Semantic Versioning](https://semver.org/)
