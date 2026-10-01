# Installing ExolangTk

ExolangTk is distributed as C99 headers plus a CMake interface target. There is
no compiled ExolangTk library to link.

## Requirements

- A C99 compiler and standard C library.
- CMake 3.16 or newer for the CMake workflow.
- Doxygen only if you want to build the API manual.

## Use from a checkout

The simplest option is to add the repository to your project:

```cmake
add_subdirectory(path/to/ExolangTk)
target_link_libraries(my_runtime PRIVATE ExolangTk)
```

Or compile directly with the include directory:

```sh
cc -std=c99 -I/path/to/ExolangTk/include my_runtime.c -o my_runtime
```

## Configure and install

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build
```

Installation copies headers and exports a relocatable CMake package under
`${CMAKE_INSTALL_LIBDIR}/cmake/ExolangTk`. Consumers can use:

```cmake
find_package(ExolangTk 0.1 CONFIG REQUIRED)
target_link_libraries(my_runtime PRIVATE ExolangTk::ExolangTk)
```

The narrower targets `ExolangTk::InteropTk`, `ExolangTk::FFItk`,
`ExolangTk::DebugTk`, and `ExolangTk::ExtensionTk` are also available in
both installed packages and source builds. They propagate C99, include paths,
subsystem dependencies, and the dynamic loader link library where applicable.
Set `CMAKE_PREFIX_PATH` to the installation prefix when configuring a consumer.

For a user-local install:

```sh
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

Then add the installed include directory to your compiler, for example:

```sh
cc -std=c99 -I"$HOME/.local/include" my_runtime.c -o my_runtime
```

## Tests and documentation

Enable the repository smoke test during configuration:

```sh
cmake -S . -B build -DEXOLANGTK_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Enable Doxygen output when Doxygen is available:

```sh
cmake -S . -B build -DEXOLANGTK_BUILD_DOCS=ON
cmake --build build --target exolangtk-docs
```

Generated HTML is placed in `build/docs/doxygen/html/`.

## Consuming headers correctly

The default qualifier macros expand to `static`, which supports a self-contained
translation unit. For calls across translation units, define the relevant
qualifier (`ITK_DEF`, `FFI_DEF`, `DTK_DEF`, or `ETK_DEF`) as `extern` consistently
in every translation unit. Define implementation guards only in the source
file that supplies the function bodies, before its first include. Implement
any dependency functions that the selected module calls as well.

For example, use `ETK_DEF=extern` throughout a project, and put this in one
implementation source file:

```c
#define ETK_VERSION_IMPLEMENTATION
#include "ExtensionTk/etk_version.h"
```

Other source files include that header without the implementation guard.
