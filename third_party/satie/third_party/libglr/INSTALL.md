# Installing LibGLR

## CMake build

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_EXAMPLES=ON \
  -DBUILD_REWRITELIB=ON
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix /usr/local
```

## Important options

- `BUILD_TESTS` builds the unit tests.
- `BUILD_EXAMPLES` builds the sample programs.
- `BUILD_DOCUMENTATION` runs Doxygen and installs generated HTML/manpage output.
- `BUILD_DISAMBSTD` builds the standard disambiguation helpers.
- `BUILD_REWRITELIB` installs the standard GRL rewrite programs.
- `BUILD_SWIG_BINDINGS` builds the SWIG-based Python module when SWIG is found.
- `ENABLE_TEST_SANITIZERS` turns on address/undefined sanitizers for the test binaries.

## Installed artifacts

By default installation provides:

- headers under `include/glr/`
- `libglr` and its bundled `sfsexp` dependency under `lib/`
- generated Doxygen HTML and manpages when documentation is enabled
- standard rewrite library files under `share/libglr/rewritelib/`

## Documentation note

The repository no longer carries a hand-written `man/` directory. Generated
Doxygen manpages are the supported manual output.

## Alternative build systems

Starter specifications for Meson, Ninja, and Autotools are provided at the
repository root. They mirror the same source layout as the CMake build but are
kept separate so downstream packagers can choose the toolchain they prefer.

## Test workflow

The CMake build exposes a small test harness around CTest:

```bash
cmake --build build --target check
ctest --test-dir build -L legacy --output-on-failure
./scripts/test-library.sh --sanitizers --verbose
```

Current labels include `core`, `rewrite`, `disambiguation`, `integration`, and
`legacy`.
