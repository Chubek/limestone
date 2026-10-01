# Chapter 5 — Atoms and Node Kinds

`Atom::Value` is:

```cpp
std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, std::shared_ptr<List>>
```

`NodeKind` distinguishes `Nil`, `Bool`, `Integer`, `Float`, `String`, `Symbol`, and `List`. A string value and a symbol value both store `std::string`; their `NodeKind` is semantic.

Constructors select the kind. To construct a symbol explicitly:

```cpp
Atom symbol("name", NodeKind::Symbol);
Atom text("name", NodeKind::String);
```

`nil` is falsey. Boolean atoms use their stored value. Every other non-nil atom is truthy. Numeric accessors permit integer/float conversion and throw on nonnumeric kinds. `as_string` accepts strings and symbols. `as_list` requires `NodeKind::List`.

Accessors are checked at runtime with `std::runtime_error`. Use `is_*` predicates before extraction when input is not statically constrained.
