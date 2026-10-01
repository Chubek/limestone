# Chapter 02 — Build & Integration

## Requirements
- C++20 compiler.
- Include `Grafitt.hpp` for core library.
- Optional: `Grafitt-Plugin.hpp`, `Grafitt-Module.h`, `Grafitt-ModuleHost.hpp`.
- Optional integrations (auto-detected via `__has_include`):
  - MetaTk `DSLtk.hpp` (`GRAFITT_HAS_DSLTK`, aliased as `GRAFITT_HAS_DSLUTILS`)
    — powers Queryfitt PEG validation and DSL-tagged subgraph matching.
  - `SerdeTk.hpp` (`GRAFITT_HAS_SERDETK`) — serialization header. GBIN
    serialization is self-contained; this include is reserved for future
    SerdeTk-backed schema hooks.

## CMake
The top-level `CMakeLists.txt` exposes an `INTERFACE` target `Grafitt`
(`Grafitt::Grafitt`) with `cxx_std_20`, plus install rules, a pkg-config file
(`grafitt.pc`), and a package config (`GrafittConfig.cmake`). Build the
examples with:

```bash
cmake -S . -B build -DGRAFITT_BUILD_EXAMPLES=ON
cmake --build build -j
```

The `GRAFITT_USE_DSLTK` / `GRAFITT_USE_SERDETK` options wire the vendored
`third_party/MetaTk/DSLtk` and `third_party/SerdeTk` headers into the
interface when they are present.

## Minimal Include
```cpp
#include "Grafitt.hpp"
```

## Optional Headers
- `Grafitt-Plugin.hpp`: in-process C++ pass framework.
- `Grafitt-Module.h`: strict C ABI plugin contract.
- `Grafitt-ModuleHost.hpp`: host lifecycle manager for C ABI modules.

## Notes
- Keep include paths pointed at the repository `include/` directory.
- No linker step is required for core header-only usage.
