# Ekipp - Modern C++17 Text Preprocessor Library

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Header-Only](https://img.shields.io/badge/header--only-yes-green.svg)](https://github.com/)

Ekipp is a powerful, extensible text preprocessing library for C++17 that provides sophisticated macro expansion and directive-based text transformation capabilities. Perfect for code generation, template processing, and custom text preprocessing needs.

## ✨ Features

- **Header-only library** - Just include and use, no linking required
- **Modern C++17** - Clean, efficient code using latest language features
- **Extensible directives** - Define custom directives with simple lambdas
- **Powerful macros** - Object-like, function-like, and variadic macros
- **Configurable syntax** - Customize prefix, brackets, and separators
- **Source tracking** - Detailed error reporting with file/line/column info
- **Zero dependencies** - Only requires C++17 standard library
- **Well documented** - Comprehensive manual and API reference

## 🚀 Quick Start

```cpp
#include <ekipp/ekipp.hpp>
#include <ekipp/directive_bank.hpp>
#include <iostream>

int main() {
    ekipp::Preprocessor pp;
    ekipp::directive_bank::register_all(pp.registry());
    
    std::string input = R"(
        @define(VERSION, 2.0.0)@
        Version: @VERSION@
        Date: @date(%Y-%m-%d)@
    )";
    
    std::cout << pp.process(input) << std::endl;
}
```

## 📦 Installation

Ekipp is header-only. Just copy the headers to your project:

```bash
# Copy headers
cp include/ekipp.hpp your_project/include/
cp include/directive_bank.hpp your_project/include/

# Or install system-wide
make install
```

### Requirements

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Standard library with filesystem support

## 🔧 Building Examples

```bash
make examples      # Build all examples
make test          # Run tests
make docs          # Generate documentation
make clean         # Clean build artifacts
```

## 📚 Documentation

- **[User Guide](GUIDE.md)** - Complete usage guide
- **[API Reference](docs/html/index.html)** - Generated with Doxygen
- **[Manual](docs/manual/)** - Five-chapter comprehensive manual
  - Chapter 1: Introduction
  - Chapter 2: Basic Usage
  - Chapter 3: Directives
  - Chapter 4: Advanced Features
  - Chapter 5: Extending Ekipp

Generate documentation:
```bash
make docs
open docs/html/index.html
```

## 🎯 Built-in Directives

| Directive | Description | Example |
|-----------|-------------|---------|
| `include` | Include external file | `@include("header.txt")@` |
| `define` | Define macro | `@define(PI, 3.14159)@` |
| `date` | Insert date/time | `@date(%Y-%m-%d)@` |
| `exec` | Execute shell command | `@exec(git rev-parse HEAD)@` |
| `eval` | Evaluate expression | `@eval(2 + 2)@` |
| `env` | Get environment variable | `@env(HOME)@` |

## 💡 Examples

### Code Generation

```cpp
ekipp::Preprocessor pp;
ekipp::directive_bank::register_all(pp.registry());

std::string template_code = R"(
@define(FIELD(name,type), type name;)@

struct User {
    @FIELD(id, int)@
    @FIELD(name, std::string)@
    @FIELD(age, int)@
};
)";

std::cout << pp.process(template_code);
```

### Custom Directive

```cpp
auto upper = ekipp::Directive::fluent()
    << ekipp::Directive::Name("upper")
    << ekipp::Directive::NumParams(1)
    << ekipp::Directive::Semantics([](ekipp::Arguments& args, ekipp::Context&) {
        std::string s = args.raw(0);
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return s;
    });

pp.registry().registerDirective(upper);

std::string result = pp.process("@upper(hello world)@");
// Result: "HELLO WORLD"
```

### Configuration

```cpp
ekipp::Configuration config;
config.parse.directive_prefix = '#';  // Use # instead of @
config.limits.max_expansion_depth = 64;
config.params.include_dirs.push_back("./templates");

ekipp::Preprocessor pp(config);
```

## 🏗️ Architecture

```
┌─────────────────┐
│  Preprocessor   │  Main entry point
└────────┬────────┘
         │
    ┌────┴────┬──────────┬──────────┐
    │         │          │          │
┌───▼───┐ ┌──▼──────┐ ┌─▼────────┐ ┌▼──────────┐
│Config │ │Registry │ │SymbolTbl │ │  Context  │
└───────┘ └─────────┘ └──────────┘ └───────────┘
```

## 🎨 Use Cases

- **Code generation** - Generate repetitive code from templates
- **Documentation** - Preprocess markdown or other documentation
- **Configuration** - Add macro expansion to config files
- **Build systems** - Custom preprocessing in build pipelines
- **Templates** - Create domain-specific template languages

## 🧪 Testing

```bash
# Run all tests
make test

# Check syntax
make check-syntax

# Build with debug symbols
make debug

# Build with sanitizers
make sanitize
```

## 📖 API Overview

### Core Classes

- **`Preprocessor`** - Main processing engine
- **`Configuration`** - Settings and limits
- **`DirectiveRegistry`** - Manages directives
- **`SymbolTable`** - Macro storage and expansion
- **`Directive`** - Directive definition
- **`Arguments`** - Directive arguments
- **`Context`** - Runtime state

### Exception Types

- **`ParseError`** - Parsing errors
- **`DirectiveError`** - Directive execution errors
- **`MacroError`** - Macro expansion errors

## 🤝 Contributing

Contributions are welcome! Please ensure:

- Code follows C++17 standards
- All tests pass
- Documentation is updated
- Examples demonstrate new features

## 📄 License

Ekipp is released under the MIT License. See [LICENSE](LICENSE) for details.

## 🔗 Links

- **Documentation**: [docs/html/index.html](docs/html/index.html)
- **User Guide**: [GUIDE.md](GUIDE.md)
- **Examples**: [examples/](examples/)
- **Refactoring Summary**: [REFACTORING_SUMMARY.md](REFACTORING_SUMMARY.md)

## 📊 Project Status

- ✅ Compiles without errors
- ✅ All examples work
- ✅ Comprehensive documentation
- ✅ Complete build system
- ✅ Ready for production use

**Version**: 2.0.0  
**Last Updated**: 2025-05-07  
**Status**: Stable and Functional

---

Made with ❤️ by the Ekipp Contributors
