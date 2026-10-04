# Ekipp User Guide

## Table of Contents

1. [Introduction](#introduction)
2. [Quick Start](#quick-start)
3. [Installation](#installation)
4. [Basic Usage](#basic-usage)
5. [Directives](#directives)
6. [Macros](#macros)
7. [Configuration](#configuration)
8. [Creating Custom Directives](#creating-custom-directives)
9. [Advanced Features](#advanced-features)
10. [API Reference](#api-reference)

## Introduction

Ekipp is a modern C++17 text preprocessing library that provides powerful macro expansion and directive-based text transformation. It's designed to be embedded in applications that need sophisticated text processing, code generation, or template expansion.

### Key Features

- **Header-only library** - Easy integration
- **Modern C++17** - Clean, efficient code
- **Extensible** - Define custom directives with lambdas
- **Configurable syntax** - Customize prefix, brackets, separators
- **Macro support** - Object-like and function-like macros
- **Source tracking** - Detailed error reporting
- **Zero dependencies** - Only C++17 standard library

## Quick Start

```cpp
#include <ekipp/ekipp.hpp>
#include <ekipp/directive_bank.hpp>
#include <iostream>

int main() {
    // Create preprocessor
    ekipp::Preprocessor pp;
    
    // Register built-in directives
    ekipp::directive_bank::register_all(pp.registry());
    
    // Process text
    std::string input = R"(
        @define(VERSION, 2.0.0)@
        Version: @VERSION@
        Date: @date(%Y-%m-%d)@
    )";
    
    std::string output = pp.process(input);
    std::cout << output << std::endl;
    
    return 0;
}
```

## Installation

Ekipp is header-only. To use it:

1. Copy `include/ekipp.hpp` and `include/directive_bank.hpp` to your project
2. Add the include directory to your compiler's include path
3. Include the headers in your code

### Requirements

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- Standard library with filesystem support

### Building Examples

```bash
make examples      # Build all examples
make test          # Run tests
make docs          # Generate documentation
make clean         # Clean build artifacts
```

## Basic Usage

### Creating a Preprocessor

```cpp
#include <ekipp/ekipp.hpp>

// Default configuration
ekipp::Preprocessor pp;

// Custom configuration
ekipp::Configuration config;
config.limits.max_input_size = 1024 * 1024;  // 1MB
config.parse.directive_prefix = '#';          // Use # instead of @
ekipp::Preprocessor pp_custom(config);
```

### Processing Text

```cpp
std::string input = "@date()@";
std::string output = pp.process(input);
```

### Error Handling

```cpp
try {
    std::string output = pp.process(input);
} catch (const ekipp::ParseError& e) {
    std::cerr << "Parse error: " << e.what() << std::endl;
} catch (const ekipp::DirectiveError& e) {
    std::cerr << "Directive error: " << e.what() << std::endl;
} catch (const ekipp::MacroError& e) {
    std::cerr << "Macro error: " << e.what() << std::endl;
}
```

## Directives

### Built-in Directives

Ekipp provides a standard library of directives in `directive_bank.hpp`:

#### include

Include and process external files:

```
@include("header.txt")@
```

Features:
- Searches relative to current file
- Searches include directories
- Recursively processes content

#### define

Define macros:

```
@define(PI, 3.14159)@
@define(MAX(a,b), ((a) > (b) ? (a) : (b)))@
```

#### date

Insert current date/time:

```
@date()@                    // 2025-01-15 14:30:00
@date(%Y-%m-%d)@           // 2025-01-15
@date(%H:%M:%S)@           // 14:30:00
```

#### exec

Execute shell commands:

```
@exec(git rev-parse --short HEAD)@
@exec(whoami)@
```

#### eval

Evaluate numeric expressions:

```
@eval(2 + 2)@              // 4
@eval(10 * 5)@             // 50
```

#### env

Get environment variables:

```
@env(HOME)@
@env(USER, unknown)@
```

### Registering Directives

```cpp
// Register all built-in directives
ekipp::directive_bank::register_all(pp.registry());

// Register individual directives
pp.registry().registerDirective(ekipp::directive_bank::make_include());
pp.registry().registerDirective(ekipp::directive_bank::make_define());
```

## Macros

### Object-like Macros

Simple text replacement:

```
@define(VERSION, 1.0.0)@
Version: @VERSION@
```

### Function-like Macros

Parameterized expansion:

```
@define(SQUARE(x), (x) * (x))@
Result: @SQUARE(5)@
```

### Variadic Macros

Variable number of arguments:

```
@define(LOG(fmt, ...), printf(fmt, __VA_ARGS__))@
@LOG("Value: %d", 42)@
```

### Programmatic Macro Definition

```cpp
// Object-like macro
pp.symbols().define_object("DEBUG", "1");

// Function-like macro
pp.symbols().define_function("ADD", {"a", "b"}, "(a) + (b)", false);

// Variadic macro
pp.symbols().define_function("PRINT", {"fmt"}, "printf(fmt, __VA_ARGS__)", true);
```

## Configuration

### Limits

```cpp
config.limits.max_input_size = 16 * 1024 * 1024;  // 16MB
config.limits.max_output_size = 64 * 1024 * 1024; // 64MB
config.limits.max_expansion_depth = 128;
config.limits.max_arguments = 256;
```

### Parse Style

```cpp
config.parse.directive_prefix = '@';
config.parse.default_left_bracket = '(';
config.parse.default_right_bracket = ')';
config.parse.argument_separator = ',';
```

### Include Directories

```cpp
config.params.include_dirs.push_back("./templates");
config.params.include_dirs.push_back("/usr/share/myapp/includes");
```

## Creating Custom Directives

### Simple Directive

```cpp
auto hello = ekipp::Directive::fluent()
    << ekipp::Directive::Name("hello")
    << ekipp::Directive::NumParams(1)
    << ekipp::Directive::Semantics([](ekipp::Arguments& args, ekipp::Context&) {
        return "Hello, " + args.raw(0) + "!";
    });

pp.registry().registerDirective(hello);
```

### Directive with Validation

```cpp
auto positive = ekipp::Directive::fluent()
    << ekipp::Directive::Name("positive")
    << ekipp::Directive::NumParams(1)
    << ekipp::Directive::Validator([](const ekipp::Arguments& args) {
        int value = args.get<int>(0);
        if (value <= 0) {
            throw ekipp::DirectiveError("value must be positive");
        }
    })
    << ekipp::Directive::Semantics([](ekipp::Arguments& args, ekipp::Context&) {
        return "Valid: " + args.raw(0);
    });
```

### Stateful Directive

```cpp
int counter = 0;

auto increment = ekipp::Directive::fluent()
    << ekipp::Directive::Name("counter")
    << ekipp::Directive::NumParams(0)
    << ekipp::Directive::Semantics([&counter](ekipp::Arguments&, ekipp::Context&) {
        return std::to_string(++counter);
    });
```

### Context-Aware Directive

```cpp
auto macro_count = ekipp::Directive::fluent()
    << ekipp::Directive::Name("macro_count")
    << ekipp::Directive::NumParams(0)
    << ekipp::Directive::Semantics([](ekipp::Arguments&, ekipp::Context& ctx) {
        if (!ctx.symbols) return std::string("0");
        return std::to_string(ctx.symbols->all().size());
    });
```

## Advanced Features

### Custom Syntax

Change the directive prefix:

```cpp
config.parse.directive_prefix = '#';
// Now use: #include("file.txt")#
```

Use different brackets:

```cpp
config.parse.default_left_bracket = '[';
config.parse.default_right_bracket = ']';
// Now use: @include["file.txt"]@
```

### Per-Directive Syntax

```cpp
auto special = ekipp::Directive::fluent()
    << ekipp::Directive::Name("special")
    << ekipp::Directive::Prefix('#')
    << ekipp::Directive::LeftBracket('{')
    << ekipp::Directive::RightBracket('}');
// Use: #special{args}#
```

### Directive Metadata

```cpp
auto dir = ekipp::Directive::fluent()
    << ekipp::Directive::Name("custom")
    << ekipp::Directive::Description("Custom directive")
    << ekipp::Directive::Category("utility")
    << ekipp::Directive::Metadata("version", ekipp::Value("1.0"))
    << ekipp::Directive::Metadata("author", ekipp::Value("John Doe"));
```

### Result Filtering

Post-process directive output:

```cpp
auto upper = ekipp::Directive::fluent()
    << ekipp::Directive::Name("upper")
    << ekipp::Directive::NumParams(1)
    << ekipp::Directive::Semantics([](ekipp::Arguments& args, ekipp::Context&) {
        return args.raw(0);
    })
    << ekipp::Directive::ResultFilter([](std::string result) {
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    });
```

### Embedding Hooks

Integrate scripting languages:

```cpp
// Lua integration example
auto lua_eval = ekipp::Directive::fluent()
    << ekipp::Directive::Name("lua")
    << ekipp::Directive::NumParams(1)
    << ekipp::Directive::Semantics([](ekipp::Arguments& args, ekipp::Context& ctx) {
        lua_State* L = static_cast<lua_State*>(
            ctx.properties["lua_state"].get<void*>()
        );
        luaL_dostring(L, args.raw(0).c_str());
        const char* result = lua_tostring(L, -1);
        return std::string(result ? result : "");
    });
```

## API Reference

### Core Classes

- **Preprocessor** - Main processing engine
- **Configuration** - Settings and limits
- **DirectiveRegistry** - Manages directives
- **SymbolTable** - Macro storage and expansion
- **Context** - Runtime state for directives
- **Arguments** - Directive arguments
- **Directive** - Directive definition
- **Value** - Generic value type

### Exception Types

- **Error** - Base exception class
- **ParseError** - Parsing errors
- **DirectiveError** - Directive execution errors
- **MacroError** - Macro expansion errors
- **SerializationError** - Serialization errors

### Directive Builder Methods

- `Name(string)` - Set directive name
- `NumParams(n)` - Exact parameter count
- `MinParams(n)` - Minimum parameters
- `MaxParams(n)` - Maximum parameters
- `IO(type)` - I/O type (r, w, rw)
- `Description(string)` - Description
- `Category(string)` - Category
- `Alias(string)` - Add alias
- `Semantics(lambda)` - Execution logic
- `Validator(lambda)` - Input validation
- `ResultFilter(lambda)` - Output transformation

## Examples

See the `examples/` directory for complete examples:

- `codegen.cpp` - Code generation with macros
- `custom_directive.cpp` - Creating custom directives
- `include_demo.cpp` - File inclusion
- `markdown_pp.cpp` - Markdown preprocessing

## Documentation

Full documentation is available in the `docs/` directory:

- **API Reference** - Generated with Doxygen
- **User Manual** - Five-chapter guide
  - Chapter 1: Introduction
  - Chapter 2: Basic Usage
  - Chapter 3: Directives
  - Chapter 4: Advanced Features
  - Chapter 5: Extending Ekipp

Generate documentation:

```bash
make docs
```

View documentation:

```bash
open docs/html/index.html
```

## License

Ekipp is released under the MIT License.

## Contributing

Contributions are welcome! Please ensure:

- Code follows C++17 standards
- All tests pass
- Documentation is updated
- Examples demonstrate new features

## Support

- GitHub Issues: Report bugs and request features
- Documentation: Read the manual and API reference
- Examples: Check the examples directory
