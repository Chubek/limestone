# Building and Integrating {#manual_02_build}

Configure tests with `cmake -S . -B build -DEXOLANGTK_BUILD_TESTS=ON`, build
with `cmake --build build`, and run `ctest --test-dir build --output-on-failure`.
The project requires C99 and CMake 3.16 or newer. Documentation is optional:
set `EXOLANGTK_BUILD_DOCS=ON` and build the `exolangtk-docs` target with Doxygen.

For source integration, add the project with `add_subdirectory` and link
`ExolangTk::ExolangTk` or an individual subsystem target. Installed consumers
use `find_package(ExolangTk CONFIG REQUIRED)` with the same namespaced targets.
No library binary is built; targets provide include paths and link dependencies.

The default function qualifier is `static`. For a dedicated implementation
translation unit shared with other source files, define the subsystem's
`*_DEF` macro as `extern` consistently throughout the program. Define each
selected module's `*_IMPLEMENTATION` guard only in the implementation source,
before including its header. Implement required dependencies there too.
