# INSTALL

This document explains how to build, test, install, and verify EZDox.

## Requirements

EZDox builds as a C++20 project with CMake and Make. The repository vendors
its primary internal dependencies under `third_party/`.

Minimum tools:

- A C++20 compiler such as `g++` or `clang++`
- `cmake` 3.20 or newer
- `make`
- `tidy` for HTML validation
- `chktex` for LaTeX linting

Optional but useful:

- `ninja`
- `ctest`
- `pdflatex` if you want to compile generated LaTeX to PDF

## Quick Build

From the repository root:

```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

This produces:

- `build/libezdox.a`
- `build/ezdox-cli`
- `build/ezdox-tests`

## Run Tests

After building:

```bash
cd build
./ezdox-tests
ctest --output-on-failure
```

The repository’s fast validation path is `./ezdox-tests`; `ctest` runs the same
binary plus a few CLI smoke checks.

The unit test binary covers:

- config parsing
- Doxygen command loading
- source scanning
- markup rendering
- target rendering
- bundle operations
- CLI smoke behavior

## Generate EZDox’s Own Documentation

The repository is configured to document itself.

```bash
cd build
./ezdox-cli generate -C ../docs/EZDox.yaml -O ../docs/_build --template ../templates
```

This generates:

- `docs/_build/html/index.html`
- `docs/_build/latex/manual.tex`

Validate the outputs:

```bash
cd ..
tidy -qe docs/_build/html/index.html
chktex -q -n1 -n8 -n46 docs/_build/latex/manual.tex
```

`chktex` may emit a warning about a missing global resource file depending on
your local installation; the generated LaTeX can still lint cleanly.

## Install to System Prefix

To install under the default prefix:

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
```

Installed artifacts include:

- headers under `include/`
- `ezdox-cli`
- manifests under `share/ezdox/manifests`
- templates under `share/ezdox/templates`

## Install to a User Prefix

For a local, non-root install:

```bash
mkdir -p build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX="$HOME/.local"
make -j$(nproc)
make install
```

Then ensure your shell can find the binary:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

## EZDOX_HOME Layout

EZDox stores user data under `$EZDOX_HOME`.
The default is `~/.ezdox`.

Expected layout:

```text
~/.ezdox/
├── bundles/
├── cache/
├── markups/
└── targets/
```

Create and verify the layout with:

```bash
ezdox-cli doctor --fix
ezdox-cli paths --all
```

## Common Build Variants

### Disable Tests

```bash
cmake .. -DEZDOX_BUILD_TESTS=OFF
```

### Disable `inja`

If you want to turn off template rendering:

```bash
cmake .. -DEZDOX_USE_INJA=OFF
```

In that mode EZDox still generates fallback HTML and LaTeX output,
but the richer template pipeline is disabled.

## Troubleshooting

### Compiler errors about C++ standard

Make sure your compiler supports C++20 and that CMake is picking the right one:

```bash
cmake .. -DCMAKE_CXX_COMPILER=g++
```

### Missing generated docs

Run:

```bash
./build/ezdox-cli doctor
./build/ezdox-cli config validate -C docs/EZDox.yaml
```

### Installed binary not found

Check your prefix and PATH:

```bash
which ezdox-cli
echo "$PATH"
```

## Verification Checklist

A healthy install should satisfy all of the following:

```bash
./build/ezdox-tests
./build/ezdox-cli version
./build/ezdox-cli help
./build/ezdox-cli paths --all
./build/ezdox-cli generate -C docs/EZDox.yaml -O docs/_build --template templates
```

If all commands succeed, the EZDox installation is ready to use.
