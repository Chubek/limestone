/**
 * @file ekipp.hpp
 * @brief Ekipp - Modern C++17 Text Preprocessor Library
 * @version 2.0.0
 * 
 * Ekipp is a powerful, extensible text preprocessing library that provides
 * macro expansion and directive-based text transformation capabilities.
 * 
 * @author The Ekipp Contributors
 * @date 2025
 * @copyright MIT License
 */
#ifndef EKIPP_HPP
#define EKIPP_HPP

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace ekipp {

/**
 * @defgroup core Core Components
 * @brief Core classes and types for text preprocessing
 */

// ============================================================================
// Version
// ============================================================================

inline constexpr int version_major = 1;
inline constexpr int version_minor = 1;
inline constexpr int version_patch = 0;
inline constexpr const char* version_string = "1.1.0";

// ============================================================================
// Exceptions
// ============================================================================

/**
 * @ingroup core
 * @brief Base exception class for all Ekipp errors
 */
class Error : public std::runtime_error {
public:
    explicit Error(const std::string& msg) : std::runtime_error(msg) {}
};

/**
 * @ingroup core
 * @brief Exception thrown during text parsing
 */
class ParseError : public Error {
public:
    explicit ParseError(const std::string& msg) : Error(msg) {}
};

/**
 * @ingroup core
 * @brief Exception thrown during directive execution
 */
class DirectiveError : public Error {
public:
    explicit DirectiveError(const std::string& msg) : Error(msg) {}
};

/**
 * @ingroup core
 * @brief Exception thrown during serialization/deserialization
 */
class SerializationError : public Error {
public:
    explicit SerializationError(const std::string& msg) : Error(msg) {}
};

/**
 * @ingroup core
 * @brief Exception thrown during macro expansion
 */
class MacroError : public Error {
public:
    explicit MacroError(const std::string& msg) : Error(msg) {}
};

// ============================================================================
// Source location
// ============================================================================

/**
 * @ingroup core
 * @brief Represents a location in source text
 * 
 * Used for error reporting and tracking the origin of text during processing.
 */
struct SourceLocation {
    std::string source_name;
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;

    std::string to_string() const {
        std::ostringstream oss;
        if (!source_name.empty()) {
            oss << source_name << ":";
        }
        oss << line << ":" << column;
        return oss.str();
    }
};

// ============================================================================
// Utility
// ============================================================================

namespace detail {

inline bool is_space(char c) {
    return !!std::isspace(static_cast<unsigned char>(c));
}

inline bool is_ident_start(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return std::isalpha(uc) || c == '_';
}

inline bool is_ident_char(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) || c == '_';
}

inline std::string trim(std::string s) {
    while (!s.empty() && is_space(s.front())) s.erase(s.begin());
    while (!s.empty() && is_space(s.back())) s.pop_back();
    return s;
}

inline std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline std::string json_escape(const std::string& s) {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
            case '\"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    oss << "\\u"
                        << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c))
                        << std::dec;
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

inline std::string quoted(const std::string& s) {
    return "\"" + json_escape(s) + "\"";
}

} // namespace detail

// ============================================================================
// Dynamic value
// ============================================================================

/**
 * @ingroup core
 * @brief Generic value type for storing different data types
 * 
 * Value can hold:
 * - null (monostate), boolean, integer (int64_t), floating point (double)
 * - string, array of values, object (map of string to value)
 * 
 * Provides type-safe access and conversion methods.
 */

class Value {
public:
    using array_type = std::vector<Value>;
    using object_type = std::map<std::string, Value>;
    using storage_type = std::variant<
        std::monostate,
        bool,
        std::int64_t,
        double,
        std::string,
        array_type,
        object_type
    >;

    Value() = default;
    Value(std::nullptr_t) : data_(std::monostate{}) {}
    Value(bool v) : data_(v) {}
    Value(int v) : data_(static_cast<std::int64_t>(v)) {}
#if !defined(__LP64__) || defined(_WIN32)
    Value(long v) : data_(static_cast<std::int64_t>(v)) {}
#endif
#if LLONG_MAX != INT64_MAX
    Value(long long v) : data_(static_cast<std::int64_t>(v)) {}
#endif
    Value(std::int64_t v) : data_(v) {}
    Value(double v) : data_(v) {}
    Value(const char* v) : data_(std::string(v ? v : "")) {}
    Value(std::string v) : data_(std::move(v)) {}
    Value(array_type v) : data_(std::move(v)) {}
    Value(object_type v) : data_(std::move(v)) {}

    const storage_type& storage() const { return data_; }
    storage_type& storage() { return data_; }

    bool is_null() const { return std::holds_alternative<std::monostate>(data_); }
    bool is_bool() const { return std::holds_alternative<bool>(data_); }
    bool is_int() const { return std::holds_alternative<std::int64_t>(data_); }
    bool is_double() const { return std::holds_alternative<double>(data_); }
    bool is_string() const { return std::holds_alternative<std::string>(data_); }
    bool is_array() const { return std::holds_alternative<array_type>(data_); }
    bool is_object() const { return std::holds_alternative<object_type>(data_); }

    template <typename T>
    const T& get() const {
        return std::get<T>(data_);
    }

    template <typename T>
    T& get() {
        return std::get<T>(data_);
    }

    std::string to_string() const {
        return std::visit([](const auto& x) -> std::string {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return "";
            } else if constexpr (std::is_same_v<T, bool>) {
                return x ? "true" : "false";
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return std::to_string(x);
            } else if constexpr (std::is_same_v<T, double>) {
                std::ostringstream oss;
                oss << x;
                return oss.str();
            } else if constexpr (std::is_same_v<T, std::string>) {
                return x;
            } else if constexpr (std::is_same_v<T, array_type>) {
                std::ostringstream oss;
                oss << "[";
                for (std::size_t i = 0; i < x.size(); ++i) {
                    if (i) oss << ", ";
                    oss << x[i].to_string();
                }
                oss << "]";
                return oss.str();
            } else {
                std::ostringstream oss;
                oss << "{";
                bool first = true;
                for (const auto& kv : x) {
                    if (!first) oss << ", ";
                    first = false;
                    oss << kv.first << ": " << kv.second.to_string();
                }
                oss << "}";
                return oss.str();
            }
        }, data_);
    }

private:
    storage_type data_;
};

// ============================================================================
// Forward declarations
// ============================================================================

class Preprocessor;
class SymbolTable;
class Directive;
class DirectiveRegistry;
class DirectiveSerializer;
class Arguments;
class Context;

// ============================================================================
// Configuration
// ============================================================================

struct Limits {
    std::size_t max_input_size = 16 * 1024 * 1024;
    std::size_t max_output_size = 64 * 1024 * 1024;
    std::size_t max_expansion_depth = 128;
    std::size_t max_arguments = 256;
    std::size_t max_directive_name_length = 128;
};

struct ParseStyle {
    char directive_prefix = '@';
    char default_left_bracket = '(';
    char default_right_bracket = ')';
    char argument_separator = ',';
    bool allow_whitespace_after_name = false;
    bool allow_bare_directives = false;
    bool allow_quoted_bare_arguments = true;
};

struct SerializationStyle {
    bool pretty = true;
    int indent = 2;
};

struct Parameters {
    std::vector<std::filesystem::path> include_dirs;
    std::unordered_map<std::string, Value> extras;
};

class Configuration {
public:
    Limits limits;
    ParseStyle parse;
    SerializationStyle serialization;
    Parameters params;

    static Configuration create() {
        return Configuration{};
    }
};

namespace Config {
inline Configuration create() {
    return Configuration::create();
}
} // namespace Config

// ============================================================================
// Arguments
// ============================================================================

class Arguments {
public:
    struct Entry {
        std::string raw;
        Value parsed;
        SourceLocation location;
    };

    Arguments() = default;

    explicit Arguments(std::vector<Entry> entries)
        : entries_(std::move(entries)) {}

    std::size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }

    const Entry& operator[](std::size_t i) const {
        if (i >= entries_.size()) throw DirectiveError("argument index out of range");
        return entries_[i];
    }

    const std::string& raw(std::size_t i) const {
        return (*this)[i].raw;
    }

    const Value& value(std::size_t i) const {
        return (*this)[i].parsed;
    }

    template <typename T>
    T get(std::size_t i) const {
        return convert<T>(value(i));
    }

    std::vector<std::string> raw_all() const {
        std::vector<std::string> out;
        out.reserve(entries_.size());
        for (const auto& e : entries_) out.push_back(e.raw);
        return out;
    }

    const std::vector<Entry>& entries() const {
        return entries_;
    }

private:
    std::vector<Entry> entries_;

    template <typename T>
    static T convert(const Value& v) {
        if constexpr (std::is_same_v<T, std::string>) {
            if (v.is_string()) return v.get<std::string>();
            return v.to_string();
        } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
            if (v.is_string()) return std::filesystem::path(v.get<std::string>());
            return std::filesystem::path(v.to_string());
        } else if constexpr (std::is_same_v<T, bool>) {
            if (v.is_bool()) return v.get<bool>();
            auto s = detail::lower(v.to_string());
            if (s == "true" || s == "1") return true;
            if (s == "false" || s == "0") return false;
            throw DirectiveError("cannot convert argument to bool");
        } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            if (v.is_int()) return static_cast<T>(v.get<std::int64_t>());
            if (v.is_double()) return static_cast<T>(v.get<double>());
            auto s = detail::trim(v.to_string());
            std::int64_t out = 0;
            auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
            if (ec != std::errc()) throw DirectiveError("cannot convert argument to integer");
            return static_cast<T>(out);
        } else if constexpr (std::is_floating_point_v<T>) {
            if (v.is_double()) return static_cast<T>(v.get<double>());
            if (v.is_int()) return static_cast<T>(v.get<std::int64_t>());
            auto s = detail::trim(v.to_string());
            char* end = nullptr;
            double d = std::strtod(s.c_str(), &end);
            if (end != s.c_str() + s.size()) throw DirectiveError("cannot convert argument to floating point");
            return static_cast<T>(d);
        } else if constexpr (std::is_same_v<T, Value>) {
            return v;
        } else {
            static_assert(!sizeof(T*), "unsupported conversion in Arguments::get");
        }
    }
};

// ============================================================================
// Context
// ============================================================================

class Context {
public:
    Preprocessor* preprocessor = nullptr;
    Configuration* configuration = nullptr;
    SymbolTable* symbols = nullptr;
    SourceLocation location;
    std::size_t expansion_depth = 0;
    std::unordered_map<std::string, Value> properties;

    void set(const std::string& key, Value value) {
        properties[key] = std::move(value);
    }

    bool has(const std::string& key) const {
        return properties.find(key) != properties.end();
    }

    const Value& get(const std::string& key) const {
        auto it = properties.find(key);
        if (it == properties.end()) throw DirectiveError("missing context property: " + key);
        return it->second;
    }
};

// ============================================================================
// Symbol table / macro system
// ============================================================================

class SymbolTable {
public:
    struct ObjectMacro {
        std::string name;
        std::string body;
        bool enabled = true;
        SourceLocation defined_at;
        std::unordered_map<std::string, Value> metadata;
    };

    struct FunctionMacro {
        std::string name;
        std::vector<std::string> parameters;
        std::string body;
        bool variadic = false;
        bool enabled = true;
        SourceLocation defined_at;
        std::unordered_map<std::string, Value> metadata;
    };

    using macro_variant = std::variant<ObjectMacro, FunctionMacro>;

    SymbolTable() = default;

    void clear() {
        macros_.clear();
    }

    bool empty() const {
        return macros_.empty();
    }

    bool is_defined(const std::string& name) const {
        return macros_.find(name) != macros_.end();
    }

    void undefine(const std::string& name) {
        macros_.erase(name);
    }

    void define_object(std::string name, std::string body,
                       SourceLocation at = {},
                       std::unordered_map<std::string, Value> metadata = {}) {
        ObjectMacro m;
        m.name = std::move(name);
        m.body = std::move(body);
        m.defined_at = std::move(at);
        m.metadata = std::move(metadata);
        macros_[m.name] = std::move(m);
    }

    void define_function(std::string name,
                         std::vector<std::string> parameters,
                         std::string body,
                         bool variadic = false,
                         SourceLocation at = {},
                         std::unordered_map<std::string, Value> metadata = {}) {
        FunctionMacro m;
        m.name = std::move(name);
        m.parameters = std::move(parameters);
        m.body = std::move(body);
        m.variadic = variadic;
        m.defined_at = std::move(at);
        m.metadata = std::move(metadata);
        macros_[m.name] = std::move(m);
    }

    const macro_variant* find(const std::string& name) const {
        auto it = macros_.find(name);
        if (it == macros_.end()) return nullptr;
        return &it->second;
    }

    macro_variant* find(const std::string& name) {
        auto it = macros_.find(name);
        if (it == macros_.end()) return nullptr;
        return &it->second;
    }

    std::vector<std::string> names() const {
        std::vector<std::string> out;
        out.reserve(macros_.size());
        for (const auto& kv : macros_) out.push_back(kv.first);
        std::sort(out.begin(), out.end());
        return out;
    }

    std::string expand_object(const ObjectMacro& m) const {
        return m.body;
    }

    std::string expand_function(const FunctionMacro& m, const std::vector<std::string>& args) const {
        if (!m.variadic && args.size() != m.parameters.size()) {
            throw MacroError("macro " + m.name + " expects " + std::to_string(m.parameters.size()) +
                             " arguments, got " + std::to_string(args.size()));
        }
        if (m.variadic && args.size() < m.parameters.size()) {
            throw MacroError("variadic macro " + m.name + " requires at least " +
                             std::to_string(m.parameters.size()) + " arguments");
        }

        std::string out = m.body;
        for (std::size_t i = 0; i < m.parameters.size(); ++i) {
            replace_identifier(out, m.parameters[i], args[i]);
        }

        if (m.variadic) {
            std::vector<std::string> tail;
            for (std::size_t i = m.parameters.size(); i < args.size(); ++i) tail.push_back(args[i]);
            std::string joined;
            for (std::size_t i = 0; i < tail.size(); ++i) {
                if (i) joined += ", ";
                joined += tail[i];
            }
            replace_identifier(out, "__VA_ARGS__", joined);
        }

        return out;
    }

    std::string expand(const std::string& name, const std::vector<std::string>& args = {}) const {
        const auto* mv = find(name);
        if (!mv) throw MacroError("undefined macro: " + name);

        if (std::holds_alternative<ObjectMacro>(*mv)) {
            if (!args.empty()) throw MacroError("object-like macro " + name + " does not take arguments");
            return expand_object(std::get<ObjectMacro>(*mv));
        }
        return expand_function(std::get<FunctionMacro>(*mv), args);
    }

    const std::unordered_map<std::string, macro_variant>& all() const {
        return macros_;
    }

private:
    std::unordered_map<std::string, macro_variant> macros_;

    static void replace_identifier(std::string& text, const std::string& ident, const std::string& replacement) {
        if (ident.empty()) return;
        std::string out;
        out.reserve(text.size());

        for (std::size_t i = 0; i < text.size();) {
            if (detail::is_ident_start(text[i])) {
                std::size_t j = i + 1;
                while (j < text.size() && detail::is_ident_char(text[j])) ++j;
                std::string_view tok(text.data() + i, j - i);
                if (tok == ident) out += replacement;
                else out.append(tok.begin(), tok.end());
                i = j;
            } else {
                out.push_back(text[i++]);
            }
        }

        text.swap(out);
    }
};

// ============================================================================
// Directive specification
// ============================================================================

enum class IOType {
    none,
    read,
    write,
    readwrite
};

inline std::string to_string(IOType io) {
    switch (io) {
        case IOType::none: return "none";
        case IOType::read: return "r";
        case IOType::write: return "w";
        case IOType::readwrite: return "rw";
    }
    return "none";
}

inline IOType parse_io(const std::string& s) {
    auto x = detail::lower(detail::trim(s));
    if (x == "none" || x.empty()) return IOType::none;
    if (x == "r" || x == "read") return IOType::read;
    if (x == "w" || x == "write") return IOType::write;
    if (x == "rw" || x == "wr" || x == "readwrite") return IOType::readwrite;
    throw SerializationError("unknown IO type: " + s);
}

struct ArgumentSpec {
    std::string name;
    std::string type = "string";
    bool optional = false;
    std::optional<Value> default_value;
    std::unordered_map<std::string, Value> metadata;
};

/**
 * @ingroup core
 * @brief Directive definition and builder
 * 
 * Directives are the primary extension mechanism in Ekipp. A directive
 * is a named operation that takes arguments and produces output.
 * 
 * Directives can be created using the fluent builder API:
 * @code
 * auto hello = ekipp::Directive::fluent()
 *     << ekipp::Directive::Name("hello")
 *     << ekipp::Directive::NumParams(1)
 *     << ekipp::Directive::Semantics([](ekipp::Arguments& args, ekipp::Context&) {
 *         return "Hello, " + args.raw(0) + "!";
 *     });
 * 
 * pp.registry().registerDirective(hello);
 * @endcode
 * 
 * @see DirectiveRegistry
 * @see Arguments
 * @see Context
 */
class Directive {
public:
    using semantics_type = std::function<std::string(Arguments&, Context&)>;
    using validator_type = std::function<void(const Arguments&, const Context&)>;
    using result_filter_type = std::function<std::string(std::string, const Context&)>;

    struct Spec {
        std::string name;
        char prefix = '@';
        char left_bracket = '(';
        char right_bracket = ')';
        char separator = ',';
        std::optional<std::size_t> exact_num_params;
        std::optional<std::size_t> min_params;
        std::optional<std::size_t> max_params;
        IOType io = IOType::none;
        bool allow_nested = true;
        bool trim_arguments = true;
        bool enabled = true;
        std::string description;
        std::string category;
        std::vector<std::string> aliases;
        std::vector<ArgumentSpec> arguments;
        std::unordered_map<std::string, Value> metadata;
    };

    struct Name {
        std::string value;
        explicit Name(std::string v) : value(std::move(v)) {}
    };

    struct Prefix {
        char value;
        explicit Prefix(char v) : value(v) {}
    };

    struct LeftBracket {
        char value;
        explicit LeftBracket(char v) : value(v) {}
    };

    struct RightBracket {
        char value;
        explicit RightBracket(char v) : value(v) {}
    };

    struct Separator {
        char value;
        explicit Separator(char v) : value(v) {}
    };

    struct NumParams {
        std::size_t value;
        explicit NumParams(std::size_t v) : value(v) {}
    };

    struct MinParams {
        std::size_t value;
        explicit MinParams(std::size_t v) : value(v) {}
    };

    struct MaxParams {
        std::size_t value;
        explicit MaxParams(std::size_t v) : value(v) {}
    };

    struct IO {
        std::string value;
        explicit IO(std::string v) : value(std::move(v)) {}
    };

    struct Description {
        std::string value;
        explicit Description(std::string v) : value(std::move(v)) {}
    };

    struct Category {
        std::string value;
        explicit Category(std::string v) : value(std::move(v)) {}
    };

    struct Alias {
        std::string value;
        explicit Alias(std::string v) : value(std::move(v)) {}
    };

    struct AllowNested {
        bool value;
        explicit AllowNested(bool v) : value(v) {}
    };

    struct TrimArguments {
        bool value;
        explicit TrimArguments(bool v) : value(v) {}
    };

    struct Enabled {
        bool value;
        explicit Enabled(bool v) : value(v) {}
    };

    struct Metadata {
        std::string key;
        Value value;
        Metadata(std::string k, Value v) : key(std::move(k)), value(std::move(v)) {}
    };

    struct Arg {
        ArgumentSpec spec;
        explicit Arg(ArgumentSpec s) : spec(std::move(s)) {}
    };

    struct Validator {
        validator_type fn;
        explicit Validator(validator_type f) : fn(std::move(f)) {}
    };

    struct Semantics {
        semantics_type fn;
        explicit Semantics(semantics_type f) : fn(std::move(f)) {}
    };

    struct ResultFilter {
        result_filter_type fn;
        explicit ResultFilter(result_filter_type f) : fn(std::move(f)) {}
    };

    Directive() = default;

    explicit Directive(Spec s)
        : spec_(std::move(s)) {}

    static Directive create(const Configuration& = Configuration{}) {
        return Directive{};
    }

    static Directive named(std::string name) {
        Directive d;
        d.spec_.name = std::move(name);
        return d;
    }

    static Directive build(std::string name,
                           semantics_type semantics,
                           std::optional<std::size_t> exact_arity = std::nullopt) {
        Directive d;
        d.spec_.name = std::move(name);
        d.semantics_ = std::move(semantics);
        d.spec_.exact_num_params = exact_arity;
        return d;
    }

    static Directive fluent() {
        return Directive{};
    }

    const Spec& spec() const { return spec_; }
    Spec& spec() { return spec_; }

    const std::string& name() const { return spec_.name; }
    char prefix() const { return spec_.prefix; }
    char left_bracket() const { return spec_.left_bracket; }
    char right_bracket() const { return spec_.right_bracket; }
    char separator() const { return spec_.separator; }

    Directive& name(std::string v) { spec_.name = std::move(v); return *this; }
    Directive& prefix(char v) { spec_.prefix = v; return *this; }
    Directive& brackets(char l, char r) { spec_.left_bracket = l; spec_.right_bracket = r; return *this; }
    Directive& separator(char v) { spec_.separator = v; return *this; }
    Directive& arity(std::size_t n) { spec_.exact_num_params = n; spec_.min_params.reset(); spec_.max_params.reset(); return *this; }
    Directive& min_arity(std::size_t n) { spec_.min_params = n; spec_.exact_num_params.reset(); return *this; }
    Directive& max_arity(std::size_t n) { spec_.max_params = n; spec_.exact_num_params.reset(); return *this; }
    Directive& io(IOType io) { spec_.io = io; return *this; }
    Directive& io(std::string io) { spec_.io = parse_io(io); return *this; }
    Directive& description(std::string v) { spec_.description = std::move(v); return *this; }
    Directive& category(std::string v) { spec_.category = std::move(v); return *this; }
    Directive& alias(std::string v) { spec_.aliases.push_back(std::move(v)); return *this; }
    Directive& allow_nested(bool v = true) { spec_.allow_nested = v; return *this; }
    Directive& trim_arguments(bool v = true) { spec_.trim_arguments = v; return *this; }
    Directive& enabled(bool v = true) { spec_.enabled = v; return *this; }
    Directive& metadata(std::string k, Value v) { spec_.metadata[std::move(k)] = std::move(v); return *this; }
    Directive& argument(ArgumentSpec arg) { spec_.arguments.push_back(std::move(arg)); return *this; }
    Directive& validator(validator_type fn) { validator_ = std::move(fn); return *this; }
    Directive& semantics(semantics_type fn) { semantics_ = std::move(fn); return *this; }
    Directive& filter(result_filter_type fn) { filter_ = std::move(fn); return *this; }

    Directive& operator<<(const Name& x) { spec_.name = x.value; return *this; }
    Directive& operator<<(const Prefix& x) { spec_.prefix = x.value; return *this; }
    Directive& operator<<(const LeftBracket& x) { spec_.left_bracket = x.value; return *this; }
    Directive& operator<<(const RightBracket& x) { spec_.right_bracket = x.value; return *this; }
    Directive& operator<<(const Separator& x) { spec_.separator = x.value; return *this; }
    Directive& operator<<(const NumParams& x) { spec_.exact_num_params = x.value; spec_.min_params.reset(); spec_.max_params.reset(); return *this; }
    Directive& operator<<(const MinParams& x) { spec_.min_params = x.value; spec_.exact_num_params.reset(); return *this; }
    Directive& operator<<(const MaxParams& x) { spec_.max_params = x.value; spec_.exact_num_params.reset(); return *this; }
    Directive& operator<<(const IO& x) { spec_.io = parse_io(x.value); return *this; }
    Directive& operator<<(const Description& x) { spec_.description = x.value; return *this; }
    Directive& operator<<(const Category& x) { spec_.category = x.value; return *this; }
    Directive& operator<<(const Alias& x) { spec_.aliases.push_back(x.value); return *this; }
    Directive& operator<<(const AllowNested& x) { spec_.allow_nested = x.value; return *this; }
    Directive& operator<<(const TrimArguments& x) { spec_.trim_arguments = x.value; return *this; }
    Directive& operator<<(const Enabled& x) { spec_.enabled = x.value; return *this; }
    Directive& operator<<(const Metadata& x) { spec_.metadata[x.key] = x.value; return *this; }
    Directive& operator<<(const Arg& x) { spec_.arguments.push_back(x.spec); return *this; }
    Directive& operator<<(const Validator& x) { validator_ = x.fn; return *this; }
    Directive& operator<<(const Semantics& x) { semantics_ = x.fn; return *this; }
    Directive& operator<<(const ResultFilter& x) { filter_ = x.fn; return *this; }

    void validate(const Arguments& args, const Context& ctx) const {
        check_arity(args.size());
        if (validator_) validator_(args, ctx);
    }

    std::string invoke(Arguments& args, Context& ctx) const {
        if (!spec_.enabled) throw DirectiveError("directive disabled: " + spec_.name);
        if (!semantics_) throw DirectiveError("directive has no semantics: " + spec_.name);
        validate(args, ctx);
        auto result = semantics_(args, ctx);
        if (filter_) result = filter_(std::move(result), ctx);
        return result;
    }

    bool matches_name(const std::string& candidate) const {
        if (candidate == spec_.name) return true;
        return std::find(spec_.aliases.begin(), spec_.aliases.end(), candidate) != spec_.aliases.end();
    }

private:
    Spec spec_;
    validator_type validator_;
    semantics_type semantics_;
    result_filter_type filter_;

    void check_arity(std::size_t n) const {
        if (spec_.exact_num_params && n != *spec_.exact_num_params) {
            throw DirectiveError("directive " + spec_.name + " expects exactly " +
                                 std::to_string(*spec_.exact_num_params) + " argument(s)");
        }
        if (spec_.min_params && n < *spec_.min_params) {
            throw DirectiveError("directive " + spec_.name + " expects at least " +
                                 std::to_string(*spec_.min_params) + " argument(s)");
        }
        if (spec_.max_params && n > *spec_.max_params) {
            throw DirectiveError("directive " + spec_.name + " expects at most " +
                                 std::to_string(*spec_.max_params) + " argument(s)");
        }
    }
};

// ============================================================================
// Directive builders
// ============================================================================

class DirectiveBuilder {
public:
    DirectiveBuilder() = default;
    explicit DirectiveBuilder(std::string name) {
        directive_.name(std::move(name));
    }

    static DirectiveBuilder create(std::string name = {}) {
        return DirectiveBuilder(std::move(name));
    }

    DirectiveBuilder& named(std::string v) { directive_.name(std::move(v)); return *this; }
    DirectiveBuilder& prefix(char c) { directive_.prefix(c); return *this; }
    DirectiveBuilder& brackets(char l, char r) { directive_.brackets(l, r); return *this; }
    DirectiveBuilder& separator(char c) { directive_.separator(c); return *this; }
    DirectiveBuilder& arity(std::size_t n) { directive_.arity(n); return *this; }
    DirectiveBuilder& min_arity(std::size_t n) { directive_.min_arity(n); return *this; }
    DirectiveBuilder& max_arity(std::size_t n) { directive_.max_arity(n); return *this; }
    DirectiveBuilder& io(IOType io) { directive_.io(io); return *this; }
    DirectiveBuilder& io(std::string io) { directive_.io(std::move(io)); return *this; }
    DirectiveBuilder& description(std::string s) { directive_.description(std::move(s)); return *this; }
    DirectiveBuilder& category(std::string s) { directive_.category(std::move(s)); return *this; }
    DirectiveBuilder& alias(std::string s) { directive_.alias(std::move(s)); return *this; }
    DirectiveBuilder& allow_nested(bool v = true) { directive_.allow_nested(v); return *this; }
    DirectiveBuilder& trim_arguments(bool v = true) { directive_.trim_arguments(v); return *this; }
    DirectiveBuilder& enabled(bool v = true) { directive_.enabled(v); return *this; }
    DirectiveBuilder& metadata(std::string k, Value v) { directive_.metadata(std::move(k), std::move(v)); return *this; }
    DirectiveBuilder& argument(ArgumentSpec arg) { directive_.argument(std::move(arg)); return *this; }
    DirectiveBuilder& validator(Directive::validator_type fn) { directive_.validator(std::move(fn)); return *this; }
    DirectiveBuilder& semantics(Directive::semantics_type fn) { directive_.semantics(std::move(fn)); return *this; }
    DirectiveBuilder& filter(Directive::result_filter_type fn) { directive_.filter(std::move(fn)); return *this; }

    Directive build() const { return directive_; }
    operator Directive() const { return directive_; }

private:
    Directive directive_;
};

namespace directives {

inline DirectiveBuilder make(std::string name = {}) {
    return DirectiveBuilder::create(std::move(name));
}

inline ArgumentSpec arg(std::string name,
                        std::string type = "string",
                        bool optional = false,
                        std::optional<Value> default_value = std::nullopt) {
    ArgumentSpec s;
    s.name = std::move(name);
    s.type = std::move(type);
    s.optional = optional;
    s.default_value = std::move(default_value);
    return s;
}

} // namespace directives

// ============================================================================
// Registry
// ============================================================================

/**
 * @ingroup core
 * @brief Registry for managing available directives
 * 
 * The DirectiveRegistry stores all registered directives and provides
 * lookup by name. Directives can be registered, unregistered, and queried.
 * 
 * @see Directive
 * @see Preprocessor
 */

class DirectiveRegistry {
public:
    void clear() {
        directives_.clear();
    }

    void registerDirective(const Directive& d) {
        if (d.name().empty()) throw DirectiveError("cannot register unnamed directive");
        directives_[d.name()] = d;
    }

    void registerDirective(Directive&& d) {
        if (d.name().empty()) throw DirectiveError("cannot register unnamed directive");
        directives_[d.name()] = std::move(d);
    }

    bool unregisterDirective(const std::string& name) {
        return directives_.erase(name) > 0;
    }

    bool contains(const std::string& name) const {
        if (directives_.find(name) != directives_.end()) return true;
        for (const auto& kv : directives_) {
            if (kv.second.matches_name(name)) return true;
        }
        return false;
    }

    const Directive* find(const std::string& name) const {
        auto it = directives_.find(name);
        if (it != directives_.end()) return &it->second;
        for (const auto& kv : directives_) {
            if (kv.second.matches_name(name)) return &kv.second;
        }
        return nullptr;
    }

    Directive* find(const std::string& name) {
        auto it = directives_.find(name);
        if (it != directives_.end()) return &it->second;
        for (auto& kv : directives_) {
            if (kv.second.matches_name(name)) return &kv.second;
        }
        return nullptr;
    }

    std::vector<std::string> names() const {
        std::vector<std::string> out;
        out.reserve(directives_.size());
        for (const auto& kv : directives_) out.push_back(kv.first);
        std::sort(out.begin(), out.end());
        return out;
    }

    const std::unordered_map<std::string, Directive>& all() const {
        return directives_;
    }

private:
    std::unordered_map<std::string, Directive> directives_;
};

// ============================================================================
// Serialization
// ============================================================================

class DirectiveSerializer {
public:
    static std::string serialize_value(const Value& v, bool pretty = false, int indent = 2, int level = 0) {
        return std::visit([&](const auto& x) -> std::string {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                return x ? "true" : "false";
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return std::to_string(x);
            } else if constexpr (std::is_same_v<T, double>) {
                std::ostringstream oss;
                oss << x;
                return oss.str();
            } else if constexpr (std::is_same_v<T, std::string>) {
                return detail::quoted(x);
            } else if constexpr (std::is_same_v<T, Value::array_type>) {
                std::ostringstream oss;
                oss << "[";
                if (!x.empty()) {
                    if (pretty) oss << "\n";
                    for (std::size_t i = 0; i < x.size(); ++i) {
                        if (pretty) oss << std::string((level + 1) * indent, ' ');
                        oss << serialize_value(x[i], pretty, indent, level + 1);
                        if (i + 1 != x.size()) oss << ",";
                        if (pretty) oss << "\n";
                    }
                    if (pretty) oss << std::string(level * indent, ' ');
                }
                oss << "]";
                return oss.str();
            } else {
                std::ostringstream oss;
                oss << "{";
                if (!x.empty()) {
                    if (pretty) oss << "\n";
                    std::size_t i = 0;
                    for (const auto& kv : x) {
                        if (pretty) oss << std::string((level + 1) * indent, ' ');
                        oss << detail::quoted(kv.first) << ":" << (pretty ? " " : "")
                            << serialize_value(kv.second, pretty, indent, level + 1);
                        if (++i != x.size()) oss << ",";
                        if (pretty) oss << "\n";
                    }
                    if (pretty) oss << std::string(level * indent, ' ');
                }
                oss << "}";
                return oss.str();
            }
        }, v.storage());
    }

    static Value deserialize_value(const std::string& text) {
        class Parser {
        public:
            explicit Parser(std::string_view sv) : s(sv) {}

            Value parse() {
                skip();
                auto v = parse_value();
                skip();
                if (pos != s.size()) throw SerializationError("trailing characters in value");
                return v;
            }

        private:
            std::string_view s;
            std::size_t pos = 0;

            void skip() {
                while (pos < s.size() && detail::is_space(s[pos])) ++pos;
            }

            char peek() const {
                return pos < s.size() ? s[pos] : '\0';
            }

            char get() {
                return pos < s.size() ? s[pos++] : '\0';
            }

            void expect(char c) {
                if (get() != c) throw SerializationError(std::string("expected '") + c + "'");
            }

            Value parse_value() {
                skip();
                char c = peek();
                if (c == '"') return Value(parse_string());
                if (c == '{') return Value(parse_object());
                if (c == '[') return Value(parse_array());
                if (c == 't') { expect_word("true"); return Value(true); }
                if (c == 'f') { expect_word("false"); return Value(false); }
                if (c == 'n') { expect_word("null"); return Value(nullptr); }
                if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();
                throw SerializationError("invalid JSON value");
            }

            void expect_word(const char* w) {
                while (*w) {
                    if (get() != *w++) throw SerializationError("invalid token");
                }
            }

            std::string parse_string() {
                expect('"');
                std::string out;
                while (pos < s.size()) {
                    char c = get();
                    if (c == '"') return out;
                    if (c == '\\') {
                        char e = get();
                        switch (e) {
                            case '"': out.push_back('"'); break;
                            case '\\': out.push_back('\\'); break;
                            case '/': out.push_back('/'); break;
                            case 'b': out.push_back('\b'); break;
                            case 'f': out.push_back('\f'); break;
                            case 'n': out.push_back('\n'); break;
                            case 'r': out.push_back('\r'); break;
                            case 't': out.push_back('\t'); break;
                            default: throw SerializationError("unsupported escape sequence");
                        }
                    } else {
                        out.push_back(c);
                    }
                }
                throw SerializationError("unterminated string");
            }

            Value parse_number() {
                std::size_t start = pos;
                if (peek() == '-') ++pos;
                while (std::isdigit(static_cast<unsigned char>(peek()))) ++pos;
                bool is_double = false;
                if (peek() == '.') {
                    is_double = true;
                    ++pos;
                    while (std::isdigit(static_cast<unsigned char>(peek()))) ++pos;
                }
                if (peek() == 'e' || peek() == 'E') {
                    is_double = true;
                    ++pos;
                    if (peek() == '+' || peek() == '-') ++pos;
                    while (std::isdigit(static_cast<unsigned char>(peek()))) ++pos;
                }
                std::string num(s.substr(start, pos - start));
                if (is_double) return Value(std::stod(num));
                return Value(static_cast<std::int64_t>(std::stoll(num)));
            }

            Value::array_type parse_array() {
                expect('[');
                skip();
                Value::array_type arr;
                if (peek() == ']') {
                    ++pos;
                    return arr;
                }
                while (true) {
                    arr.push_back(parse_value());
                    skip();
                    if (peek() == ']') {
                        ++pos;
                        return arr;
                    }
                    expect(',');
                    skip();
                }
            }

            Value::object_type parse_object() {
                expect('{');
                skip();
                Value::object_type obj;
                if (peek() == '}') {
                    ++pos;
                    return obj;
                }
                while (true) {
                    skip();
                    if (peek() != '"') throw SerializationError("expected object key");
                    auto key = parse_string();
                    skip();
                    expect(':');
                    skip();
                    obj.emplace(std::move(key), parse_value());
                    skip();
                    if (peek() == '}') {
                        ++pos;
                        return obj;
                    }
                    expect(',');
                    skip();
                }
            }
        };

        return Parser(text).parse();
    }

    static std::string serialize_argument_spec(const ArgumentSpec& a, bool pretty = true, int indent = 2, int level = 0) {
        Value::object_type obj;
        obj["name"] = a.name;
        obj["type"] = a.type;
        obj["optional"] = a.optional;
        if (a.default_value) obj["default"] = *a.default_value;
        Value::object_type meta;
        for (const auto& kv : a.metadata) meta[kv.first] = kv.second;
        obj["metadata"] = meta;
        return serialize_value(obj, pretty, indent, level);
    }

    static std::string serialize(const Directive& d, bool pretty = true, int indent = 2) {
        Value::object_type obj;
        obj["name"] = d.spec().name;
        obj["prefix"] = std::string(1, d.spec().prefix);
        obj["left_bracket"] = std::string(1, d.spec().left_bracket);
        obj["right_bracket"] = std::string(1, d.spec().right_bracket);
        obj["separator"] = std::string(1, d.spec().separator);
        if (d.spec().exact_num_params) obj["exact_num_params"] = static_cast<std::int64_t>(*d.spec().exact_num_params);
        if (d.spec().min_params) obj["min_params"] = static_cast<std::int64_t>(*d.spec().min_params);
        if (d.spec().max_params) obj["max_params"] = static_cast<std::int64_t>(*d.spec().max_params);
        obj["io"] = to_string(d.spec().io);
        obj["allow_nested"] = d.spec().allow_nested;
        obj["trim_arguments"] = d.spec().trim_arguments;
        obj["enabled"] = d.spec().enabled;
        obj["description"] = d.spec().description;
        obj["category"] = d.spec().category;

        Value::array_type aliases;
        for (const auto& a : d.spec().aliases) aliases.emplace_back(a);
        obj["aliases"] = aliases;

        Value::array_type args;
        for (const auto& a : d.spec().arguments) {
            Value::object_type arg;
            arg["name"] = a.name;
            arg["type"] = a.type;
            arg["optional"] = a.optional;
            if (a.default_value) arg["default"] = *a.default_value;
            Value::object_type meta;
            for (const auto& kv : a.metadata) meta[kv.first] = kv.second;
            arg["metadata"] = meta;
            args.emplace_back(arg);
        }
        obj["arguments"] = args;

        Value::object_type meta;
        for (const auto& kv : d.spec().metadata) meta[kv.first] = kv.second;
        obj["metadata"] = meta;

        return serialize_value(obj, pretty, indent);
    }

    static Directive deserialize(const std::string& text) {
        auto root = deserialize_value(text);
        if (!root.is_object()) throw SerializationError("directive JSON must be an object");

        const auto& obj = root.get<Value::object_type>();
        Directive d;

        auto get_string = [&](const std::string& k, const std::string& def = "") -> std::string {
            auto it = obj.find(k);
            if (it == obj.end()) return def;
            if (!it->second.is_string()) throw SerializationError("field " + k + " must be a string");
            return it->second.get<std::string>();
        };

        auto get_bool = [&](const std::string& k, bool def = false) -> bool {
            auto it = obj.find(k);
            if (it == obj.end()) return def;
            if (!it->second.is_bool()) throw SerializationError("field " + k + " must be a bool");
            return it->second.get<bool>();
        };

        auto get_int_opt = [&](const std::string& k) -> std::optional<std::size_t> {
            auto it = obj.find(k);
            if (it == obj.end()) return std::nullopt;
            if (!it->second.is_int()) throw SerializationError("field " + k + " must be an integer");
            return static_cast<std::size_t>(it->second.get<std::int64_t>());
        };

        d.name(get_string("name"));
        auto p = get_string("prefix", "@");
        auto l = get_string("left_bracket", "(");
        auto r = get_string("right_bracket", ")");
        auto s = get_string("separator", ",");
        if (p.empty() || l.empty() || r.empty() || s.empty()) throw SerializationError("single-char fields cannot be empty");
        d.prefix(p.front()).brackets(l.front(), r.front()).separator(s.front());

        if (auto x = get_int_opt("exact_num_params")) d.arity(*x);
        if (auto x = get_int_opt("min_params")) d.min_arity(*x);
        if (auto x = get_int_opt("max_params")) d.max_arity(*x);

        d.io(get_string("io", "none"));
        d.allow_nested(get_bool("allow_nested", true));
        d.trim_arguments(get_bool("trim_arguments", true));
        d.enabled(get_bool("enabled", true));
        d.description(get_string("description"));
        d.category(get_string("category"));

        auto it_aliases = obj.find("aliases");
        if (it_aliases != obj.end()) {
            if (!it_aliases->second.is_array()) throw SerializationError("aliases must be an array");
            for (const auto& v : it_aliases->second.get<Value::array_type>()) {
                if (!v.is_string()) throw SerializationError("alias must be a string");
                d.alias(v.get<std::string>());
            }
        }

        auto it_args = obj.find("arguments");
        if (it_args != obj.end()) {
            if (!it_args->second.is_array()) throw SerializationError("arguments must be an array");
            for (const auto& v : it_args->second.get<Value::array_type>()) {
                if (!v.is_object()) throw SerializationError("argument spec must be an object");
                const auto& ao = v.get<Value::object_type>();
                ArgumentSpec as;
                auto f = ao.find("name");
                if (f != ao.end() && f->second.is_string()) as.name = f->second.get<std::string>();
                f = ao.find("type");
                if (f != ao.end() && f->second.is_string()) as.type = f->second.get<std::string>();
                f = ao.find("optional");
                if (f != ao.end() && f->second.is_bool()) as.optional = f->second.get<bool>();
                f = ao.find("default");
                if (f != ao.end()) as.default_value = f->second;
                f = ao.find("metadata");
                if (f != ao.end() && f->second.is_object()) {
                    for (const auto& kv : f->second.get<Value::object_type>()) as.metadata[kv.first] = kv.second;
                }
                d.argument(std::move(as));
            }
        }

        auto it_meta = obj.find("metadata");
        if (it_meta != obj.end()) {
            if (!it_meta->second.is_object()) throw SerializationError("metadata must be an object");
            for (const auto& kv : it_meta->second.get<Value::object_type>()) d.metadata(kv.first, kv.second);
        }

        return d;
    }

    static std::string serialize_registry(const DirectiveRegistry& reg, bool pretty = true, int indent = 2) {
        Value::array_type arr;
        for (const auto& name : reg.names()) {
            const auto* d = reg.find(name);
            arr.emplace_back(deserialize_value(serialize(*d, false)));
        }
        return serialize_value(arr, pretty, indent);
    }

    static std::vector<Directive> deserialize_registry(const std::string& text) {
        auto v = deserialize_value(text);
        if (!v.is_array()) throw SerializationError("registry JSON must be an array");
        std::vector<Directive> out;
        for (const auto& item : v.get<Value::array_type>()) {
            out.push_back(deserialize(serialize_value(item, false)));
        }
        return out;
    }

    static std::string serialize_symbols(const SymbolTable& symbols, bool pretty = true, int indent = 2) {
        Value::array_type arr;
        for (const auto& name : symbols.names()) {
            const auto* m = symbols.find(name);
            if (!m) continue;
            Value::object_type obj;
            if (std::holds_alternative<SymbolTable::ObjectMacro>(*m)) {
                const auto& om = std::get<SymbolTable::ObjectMacro>(*m);
                obj["kind"] = "object";
                obj["name"] = om.name;
                obj["body"] = om.body;
                obj["enabled"] = om.enabled;
                Value::object_type meta;
                for (const auto& kv : om.metadata) meta[kv.first] = kv.second;
                obj["metadata"] = meta;
            } else {
                const auto& fm = std::get<SymbolTable::FunctionMacro>(*m);
                obj["kind"] = "function";
                obj["name"] = fm.name;
                obj["body"] = fm.body;
                obj["variadic"] = fm.variadic;
                obj["enabled"] = fm.enabled;
                Value::array_type params;
                for (const auto& p : fm.parameters) params.emplace_back(p);
                obj["parameters"] = params;
                Value::object_type meta;
                for (const auto& kv : fm.metadata) meta[kv.first] = kv.second;
                obj["metadata"] = meta;
            }
            arr.emplace_back(obj);
        }
        return serialize_value(arr, pretty, indent);
    }

    static void deserialize_symbols(const std::string& text, SymbolTable& symbols) {
        auto v = deserialize_value(text);
        if (!v.is_array()) throw SerializationError("symbol JSON must be an array");
        for (const auto& item : v.get<Value::array_type>()) {
            if (!item.is_object()) throw SerializationError("symbol entry must be object");
            const auto& o = item.get<Value::object_type>();
            auto kind_it = o.find("kind");
            auto name_it = o.find("name");
            auto body_it = o.find("body");
            if (kind_it == o.end() || name_it == o.end() || body_it == o.end()) {
                throw SerializationError("symbol entry missing required fields");
            }
            std::string kind = kind_it->second.get<std::string>();
            std::string name = name_it->second.get<std::string>();
            std::string body = body_it->second.get<std::string>();

            if (kind == "object") {
                symbols.define_object(name, body);
            } else if (kind == "function") {
                std::vector<std::string> params;
                auto pit = o.find("parameters");
                if (pit != o.end() && pit->second.is_array()) {
                    for (const auto& p : pit->second.get<Value::array_type>()) params.push_back(p.get<std::string>());
                }
                bool variadic = false;
                auto vit = o.find("variadic");
                if (vit != o.end() && vit->second.is_bool()) variadic = vit->second.get<bool>();
                symbols.define_function(name, params, body, variadic);
            } else {
                throw SerializationError("unknown macro kind: " + kind);
            }
        }
    }
};

// ============================================================================
// Preprocessor
// ============================================================================

/**
 * @ingroup core
 * @brief Main text preprocessing engine
 * 
 * The Preprocessor class is the primary entry point for text processing.
 * It orchestrates directive execution, macro expansion, and text transformation.
 * 
 * Example usage:
 * @code
 * ekipp::Preprocessor pp;
 * ekipp::directive_bank::register_all(pp.registry());
 * 
 * std::string input = "@define(NAME, World)@Hello, @NAME@!";
 * std::string output = pp.process(input);
 * // Output: "Hello, World!"
 * @endcode
 */

class Preprocessor {
public:
    Preprocessor()
        : config_(Configuration::create()) {}

    explicit Preprocessor(Configuration cfg)
        : config_(std::move(cfg)) {}

    Configuration& configuration() { return config_; }
    const Configuration& configuration() const { return config_; }

    DirectiveRegistry& registry() { return registry_; }
    const DirectiveRegistry& registry() const { return registry_; }

    SymbolTable& symbols() { return symbols_; }
    const SymbolTable& symbols() const { return symbols_; }

    void registerDirective(const Directive& d) {
        registry_.registerDirective(d);
    }

    void registerDirective(Directive&& d) {
        registry_.registerDirective(std::move(d));
    }

    std::string process(const std::string& input, const std::string& source_name = {}) {
        if (input.size() > config_.limits.max_input_size) {
            throw ParseError("input exceeds max_input_size");
        }
        return process_impl(input, source_name, 0);
    }

    std::string expand_macros(const std::string& input, std::size_t depth = 0) const {
        if (depth > config_.limits.max_expansion_depth) {
            throw MacroError("maximum macro expansion depth exceeded");
        }

        std::string out;
        out.reserve(input.size());

        for (std::size_t i = 0; i < input.size();) {
            if (!detail::is_ident_start(input[i])) {
                out.push_back(input[i++]);
                continue;
            }

            std::size_t j = i + 1;
            while (j < input.size() && detail::is_ident_char(input[j])) ++j;
            std::string ident = input.substr(i, j - i);

            const auto* m = symbols_.find(ident);
            if (!m) {
                out += ident;
                i = j;
                continue;
            }

            if (std::holds_alternative<SymbolTable::ObjectMacro>(*m)) {
                const auto& om = std::get<SymbolTable::ObjectMacro>(*m);
                if (!om.enabled) {
                    out += ident;
                    i = j;
                    continue;
                }
                out += expand_macros(om.body, depth + 1);
                i = j;
                continue;
            }

            const auto& fm = std::get<SymbolTable::FunctionMacro>(*m);
            if (!fm.enabled) {
                out += ident;
                i = j;
                continue;
            }

            std::size_t k = j;
            while (k < input.size() && detail::is_space(input[k])) ++k;
            if (k >= input.size() || input[k] != '(') {
                out += ident;
                i = j;
                continue;
            }

            std::size_t consumed = k;
            auto args = parse_call_arguments(input, consumed, '(', ')', ',');
            std::string exp = symbols_.expand_function(fm, args);
            out += expand_macros(exp, depth + 1);
            i = consumed;
        }

        return out;
    }

private:
    Configuration config_;
    DirectiveRegistry registry_;
    SymbolTable symbols_;

    std::string process_impl(const std::string& input, const std::string& source_name, std::size_t depth) {
        if (depth > config_.limits.max_expansion_depth) {
            throw ParseError("maximum directive expansion depth exceeded");
        }

        std::string output;
        output.reserve(input.size());

        std::size_t line = 1;
        std::size_t col = 1;

        for (std::size_t i = 0; i < input.size();) {
            if (input[i] != config_.parse.directive_prefix) {
                output.push_back(input[i]);
                advance(input[i], line, col);
                ++i;
                continue;
            }

            SourceLocation loc;
            loc.source_name = source_name;
            loc.offset = i;
            loc.line = line;
            loc.column = col;

            auto maybe = try_parse_directive(input, i, loc, depth);
            if (!maybe) {
                output.push_back(input[i]);
                advance(input[i], line, col);
                ++i;
                continue;
            }

            const auto consumed = maybe->consumed;
            for (std::size_t k = i; k < consumed; ++k) advance(input[k], line, col);
            output += maybe->replacement;
            i = consumed;

            if (output.size() > config_.limits.max_output_size) {
                throw ParseError("output exceeds max_output_size");
            }
        }

        output = expand_macros(output);
        return output;
    }

    struct ParseResult {
        std::string replacement;
        std::size_t consumed = 0;
    };

    std::optional<ParseResult> try_parse_directive(const std::string& input,
                                                   std::size_t start,
                                                   const SourceLocation& loc,
                                                   std::size_t depth) {
        if (input[start] != config_.parse.directive_prefix) return std::nullopt;

        std::size_t i = start + 1;
        if (i >= input.size() || !detail::is_ident_start(input[i])) return std::nullopt;

        std::size_t name_start = i;
        while (i < input.size() && detail::is_ident_char(input[i])) ++i;
        std::string name = input.substr(name_start, i - name_start);

        const Directive* dir = registry_.find(name);
        if (!dir) return std::nullopt;
        if (dir->prefix() != input[start]) return std::nullopt;

        std::size_t p = i;
        if (config_.parse.allow_whitespace_after_name) {
            while (p < input.size() && detail::is_space(input[p])) ++p;
        }

        if (p >= input.size() || input[p] != dir->left_bracket()) {
            if (config_.parse.allow_bare_directives &&
                (!dir->spec().exact_num_params || *dir->spec().exact_num_params == 0)) {
                Arguments args;
                Context ctx;
                ctx.preprocessor = this;
                ctx.configuration = &config_;
                ctx.symbols = &symbols_;
                ctx.location = loc;
                ctx.expansion_depth = depth;
                auto result = dir->invoke(args, ctx);
                return ParseResult{result, p};
            }
            return std::nullopt;
        }

        std::size_t consumed = p;
        auto raw_args = parse_call_arguments(input, consumed, dir->left_bracket(), dir->right_bracket(), dir->separator());

        if (raw_args.size() > config_.limits.max_arguments) {
            throw ParseError("too many directive arguments");
        }

        std::vector<Arguments::Entry> entries;
        entries.reserve(raw_args.size());
        for (const auto& raw0 : raw_args) {
            std::string raw = dir->spec().trim_arguments ? detail::trim(raw0) : raw0;
            Arguments::Entry e;
            e.raw = raw;
            e.location = loc;
            e.parsed = parse_argument_value(raw);
            entries.push_back(std::move(e));
        }

        Arguments args(std::move(entries));
        Context ctx;
        ctx.preprocessor = this;
        ctx.configuration = &config_;
        ctx.symbols = &symbols_;
        ctx.location = loc;
        ctx.expansion_depth = depth;

        auto result = dir->invoke(args, ctx);
        result = process_impl(result, source_name_for_nested(loc), depth + 1);
        return ParseResult{result, consumed};
    }

    static std::string source_name_for_nested(const SourceLocation& loc) {
        return loc.source_name;
    }

    static void advance(char c, std::size_t& line, std::size_t& col) {
        if (c == '\n') {
            ++line;
            col = 1;
        } else {
            ++col;
        }
    }

    Value parse_argument_value(const std::string& raw) const {
        auto s = detail::trim(raw);
        if (s.empty()) return Value("");

        if ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')) {
            return Value(unquote(s));
        }

        if (detail::lower(s) == "true") return Value(true);
        if (detail::lower(s) == "false") return Value(false);

        {
            std::int64_t v = 0;
            auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
            if (ec == std::errc() && p == s.data() + s.size()) return Value(v);
        }

        char* end = nullptr;
        double d = std::strtod(s.c_str(), &end);
        if (end == s.c_str() + s.size() && s.find_first_of(".eE") != std::string::npos) {
            return Value(d);
        }

        return Value(s);
    }

    static std::string unquote(const std::string& s) {
        if (s.size() < 2) return s;
        char q = s.front();
        std::string out;
        out.reserve(s.size() - 2);
        for (std::size_t i = 1; i + 1 < s.size(); ++i) {
            char c = s[i];
            if (c == '\\' && i + 1 < s.size() - 1) {
                char n = s[++i];
                switch (n) {
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case '\\': out.push_back('\\'); break;
                    case '\'': out.push_back('\''); break;
                    case '"': out.push_back('"'); break;
                    default: out.push_back(n); break;
                }
            } else {
                out.push_back(c);
            }
        }
        (void)q;
        return out;
    }

    static std::vector<std::string> parse_call_arguments(const std::string& s,
                                                         std::size_t& pos,
                                                         char left,
                                                         char right,
                                                         char sep) {
        if (pos >= s.size() || s[pos] != left) throw ParseError("expected opening bracket");
        ++pos;

        std::vector<std::string> args;
        std::string cur;
        int nesting = 0;
        bool in_string = false;
        char string_quote = '\0';

        while (pos < s.size()) {
            char c = s[pos++];

            if (in_string) {
                cur.push_back(c);
                if (c == '\\') {
                    if (pos < s.size()) cur.push_back(s[pos++]);
                    continue;
                }
                if (c == string_quote) {
                    in_string = false;
                    string_quote = '\0';
                }
                continue;
            }

            if (c == '"' || c == '\'') {
                in_string = true;
                string_quote = c;
                cur.push_back(c);
                continue;
            }

            if (c == left) {
                ++nesting;
                cur.push_back(c);
                continue;
            }

            if (c == right) {
                if (nesting == 0) {
                    if (!cur.empty() || !args.empty()) args.push_back(cur);
                    return args;
                }
                --nesting;
                cur.push_back(c);
                continue;
            }

            if (c == sep && nesting == 0) {
                args.push_back(cur);
                cur.clear();
                continue;
            }

            cur.push_back(c);
        }

        throw ParseError("unterminated argument list");
    }
};

// ============================================================================
// Facade helpers
// ============================================================================

class Library {
public:
    Configuration configuration;
    DirectiveRegistry directives;
    SymbolTable symbols;

    static Library create() {
        return Library{};
    }

    Preprocessor make_preprocessor() const {
        Preprocessor p(configuration);
        for (const auto& name : directives.names()) {
            const auto* d = directives.find(name);
            if (d) p.registerDirective(*d);
        }
        for (const auto& name : symbols.names()) {
            const auto* mv = symbols.find(name);
            if (!mv) continue;
            if (std::holds_alternative<SymbolTable::ObjectMacro>(*mv)) {
                const auto& m = std::get<SymbolTable::ObjectMacro>(*mv);
                p.symbols().define_object(m.name, m.body, m.defined_at, m.metadata);
            } else {
                const auto& m = std::get<SymbolTable::FunctionMacro>(*mv);
                p.symbols().define_function(m.name, m.parameters, m.body, m.variadic, m.defined_at, m.metadata);
            }
        }
        return p;
    }
};

// ============================================================================
// Define utilities
// ============================================================================

namespace define {

inline void object(SymbolTable& st, const std::string& name, const std::string& body,
                   SourceLocation loc = {}) {
    st.define_object(name, body, loc);
}

inline void function(SymbolTable& st, const std::string& name,
                     std::vector<std::string> params,
                     const std::string& body,
                     bool variadic = false,
                     SourceLocation loc = {}) {
    st.define_function(name, std::move(params), body, variadic, loc);
}

inline bool exists(const SymbolTable& st, const std::string& name) {
    return st.is_defined(name);
}

inline void undef(SymbolTable& st, const std::string& name) {
    st.undefine(name);
}

} // namespace define

} // namespace ekipp

#endif // EKIPP_HPP
