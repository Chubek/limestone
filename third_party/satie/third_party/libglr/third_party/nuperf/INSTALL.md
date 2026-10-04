# NuPERF Installation

## Requirements

- CMake >= 3.16
- C++17 compiler
- Optional: Doxygen (docs target)
- Optional: Lua toolchain (if building Lua module)
- Optional: SWIG (if generating SWIG bindings)

Vendored dependencies are consumed from `third_party/`.

## Configure

Default install-oriented configuration:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DNUPERF_BUILD_CLI=ON \
  -DNUPERF_BUILD_DOCS=ON \
  -DNUPERF_INSTALL_DOCS=ON \
  -DNUPERF_INSTALL_STDMETH=ON \
  -DNUPERF_INSTALL_STDDEF=ON
```

Optional feature toggles:

```bash
-DNUPERF_BUILD_LUA=ON
-DNUPERF_BUILD_SWIG_BINDINGS=ON
-DNUPERF_BUILD_TESTS=ON
-DNUPERF_BUILD_EXAMPLES=ON
```

## Build

```bash
cmake --build build -j
```

Build docs target:

```bash
cmake --build build --target docs
```

## Install

```bash
cmake --install build --prefix /usr/local
```

## Installed Artifacts

### Core

- library: `${prefix}/lib/libnuperf.*`
- headers: `${prefix}/include/nuperf/*.h`
- CLI: `${prefix}/bin/nuperf` (if enabled)

### Package metadata

- pkg-config: `${prefix}/lib/pkgconfig/nuperf.pc`
- CMake package:
  - `${prefix}/lib/cmake/nuperf/nuperfConfig.cmake`
  - `${prefix}/lib/cmake/nuperf/nuperfConfigVersion.cmake`
  - `${prefix}/lib/cmake/nuperf/nuperfTargets.cmake`

### Plugin source installation (default ON)

- `${prefix}/share/nuperf/stdmeth/...`
- `${prefix}/share/nuperf/stddef/...`

### Documentation (default ON if built)

- `${prefix}/share/doc/nuperf/...`

## Downstream CMake Consumption

```cmake
find_package(nuperf REQUIRED)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE nuperf::nuperf)
```

## Verification

```bash
${prefix}/bin/nuperf version
${prefix}/bin/nuperf list-methods
${prefix}/bin/nuperf list-targets
```

If docs were built/installed, confirm doc tree under `${prefix}/share/doc/nuperf`.
