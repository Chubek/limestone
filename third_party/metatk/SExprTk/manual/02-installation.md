# Chapter 2 — Installation and Build Model

SExprTk is an interface library:

```cmake
add_subdirectory(SExprTk)
target_link_libraries(app PRIVATE SExprTk)
```

The target exports the `include` directory and requires C++20. Parsing, serialization, XAS, analyzers, transformers, and package structures have no runtime dependency.

Optional backends are selected by CMake:

- `-DSEXPRTK_WITH_S7=ON` enables S7 when `third_party/S7/s7.c` exists;
- `-DSEXPRTK_WITH_LUA=ON` enables QaMRpp when its headers exist.

Consumers that use a backend receive the corresponding compile definition and include/link settings through the project helper. A dependency-free consumer should leave both backends disabled or unavailable.

Direct compilation requires the include path:

```sh
c++ -std=c++20 -I SExprTk/include app.cpp
```

`SExprTk.hpp` is header-only, but optional runtime headers and libraries remain external build inputs. Do not define backend macros manually unless their dependencies are installed and linkable.
