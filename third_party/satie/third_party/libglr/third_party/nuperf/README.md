# NuPERF

NuPERF is a minimal perfect hash table generator with a stable C API, a C++ core, plugin-based methods/targets, Lua integration via sol2, SWIG bindings, and a CLI frontend.

## Core Capabilities

- Build static hash artifacts from deterministic keysets.
- Emit output through target plugins (default `stddef` emits C-friendly arrays).
- Select construction methods through method plugins (default `stdmeth`).
- Integrate from C/C++, Lua, or SWIG-generated bindings.
- Build with CMake; package exports available for downstream CMake consumers.

## Repository Layout

- `include/nuperf/` public API headers.
- `src/core/` API entry points, keyset/table flows.
- `src/plugins/` built-in plugins (`stdmeth`, `stddef`).
- `src/lua/` Lua module bindings.
- `src/main.cpp` CLI implementation.
- `bindings/` SWIG interface + XFeats-driven generator script.
- `stdmeth/` and `stddef/` plugin source trees for installation/distribution.
- `docs/manual/` end-user and extension docs.
- `cmake/` package-config and dependency wiring.

## Quick Start

### Build

```bash
cmake -S . -B build \
  -DNUPERF_BUILD_CLI=ON \
  -DNUPERF_BUILD_DOCS=ON
cmake --build build
```

### CLI usage

```bash
./build/nuperf version
./build/nuperf list-methods
./build/nuperf list-targets
./build/nuperf build -i keys.txt -o table.h -m stdmeth -t stddef --option array_name=my_table
```

### C API flow

1. `nuperf_init()`
2. `nuperf_keyset_create()`
3. add keys to keyset
4. `nuperf_table_create()`
5. `nuperf_table_set_keyset()`
6. optional: set method/target/options
7. `nuperf_table_build()`
8. `nuperf_table_emit_file()` or `nuperf_table_emit_buffer()`
9. destroy resources + `nuperf_shutdown()`

## Build Options

- `NUPERF_BUILD_CLI` (default `ON`)
- `NUPERF_BUILD_LUA` (default `OFF`)
- `NUPERF_BUILD_SWIG_BINDINGS` (default `OFF`)
- `NUPERF_BUILD_TESTS` (default `OFF`)
- `NUPERF_BUILD_EXAMPLES` (default `OFF`)
- `NUPERF_BUILD_DOCS` (default `ON`)
- `NUPERF_INSTALL_DOCS` (default `ON`)
- `NUPERF_INSTALL_STDMETH` (default `ON`)
- `NUPERF_INSTALL_STDDEF` (default `ON`)

## Packaging

Install exports:

- `lib/cmake/nuperf/nuperfConfig.cmake`
- `lib/cmake/nuperf/nuperfConfigVersion.cmake`
- `lib/cmake/nuperf/nuperfTargets.cmake`
- `lib/pkgconfig/nuperf.pc`

Consumer example:

```cmake
find_package(nuperf REQUIRED)
target_link_libraries(my_app PRIVATE nuperf::nuperf)
```

## Bindings

SWIG generation (XFeats-driven macros):

```bash
./bindings/generate-bindings.sh Python --output-dir=pynuperf --xfeats +null_check_guard +enum_mapping
```

Lua module is built when `NUPERF_BUILD_LUA=ON`.

## Documentation

- API/manual sources: `docs/manual/`
- Doxygen target: `docs`

```bash
cmake --build build --target docs
```

## Stability Notes

- C API is the compatibility boundary.
- Public API functions are expected to return `nuperf_status_t` and avoid exceptions across C boundaries.
- Plugin registry enumeration and buffer-size contracts should remain deterministic and consistent.

## License

See `LICENSE`.
