# EZDox

EZDox is a manifest-driven documentation generator for C++ projects.
It is designed as a practical Doxygen-compatible alternative with a simpler,
extensible architecture built around config files, bundles, markups, targets,
and a small CLI.

## Features

- Scans C++ sources for Doxygen-style comments such as `@brief`, `@param`, and `@return`
- Loads recognized Doxygen commands from `manifests/doxygen-commands.yaml`
- Parses config files in YAML and JSON via `nlohmann-json` and `yaml-cpp`
- Supports built-in markups: Markdown, ReStructuredText, Docbook, XWiki
- Supports built-in targets: HTML, LaTeX, Manpage, ROFF, XML
- Extension bundle commands are registered but not yet implemented (no zip backend)
- Uses `inja` templates for richer HTML and LaTeX rendering
- Can document itself, including frontpage + manual + scraped API docs

## Repository Layout

```text
.
├── cli/
├── docs/
├── include/
├── manifests/
├── src/
├── stdmarkup/
├── stdtarget/
├── templates/
├── tests/
└── third_party/
```

Important directories:

- `cli/` — CLI entry point and command handling
- `docs/` — self-hosted EZDox documentation config and manual
- `include/` — public headers
- `manifests/` — CLI and Doxygen command manifests
- `src/` — core implementation
- `templates/` — HTML and LaTeX templates
- `tests/` — unit and smoke coverage

## Build

```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Main outputs:

- `build/ezdox-cli`
- `build/ezdox-tests`
- `build/libezdox.a`

## Test

```bash
cd build
./ezdox-tests
ctest --output-on-failure
```

## Quick Start

Generate a starter config:

```bash
./build/ezdox-cli config scaffold \
  -f yaml \
  -o EZDox.yaml \
  -p "MyProject" \
  -V "1.0.0" \
  -S src \
  -I include \
  -m Markdown \
  -t HTML
```

Generate docs:

```bash
./build/ezdox-cli generate -C EZDox.yaml -O build/docs --template templates
```

Open:

```text
build/docs/html/index.html
```

## Self Documentation

EZDox is configured to document itself.

```bash
./build/ezdox-cli generate -C docs/EZDox.yaml -O docs/_build --template templates
```

This renders:

- `docs/frontpage.md`
- all files under `docs/manual/`
- scraped API docs from `include/`, `src/`, `cli/`, `stdmarkup/`, and `stdtarget/`

## Validation

Validate generated documentation with:

```bash
tidy -qe docs/_build/html/index.html
chktex -q -n1 -n8 -n46 docs/_build/latex/manual.tex
```

`chktex` may print a harmless resource-file warning on some systems.

## CLI Surface

Implemented command families:

- `help`
- `version`
- `paths`
- `config scaffold|validate|print|run`
- `bundle build|install|list|remove|inspect`
- `find`
- `generate`
- `install`
- `run`
- `doctor`

## Environment

EZDox uses `$EZDOX_HOME`, defaulting to `~/.ezdox`.
Key path helpers:

```bash
./build/ezdox-cli paths --all
./build/ezdox-cli doctor --fix
```

## Bundles

Example bundle workflow:

```bash
./build/ezdox-cli bundle build -s extensions/my-bundle -o dist/my-bundle.ezb -n my-bundle -V 0.1.0
./build/ezdox-cli bundle install -b dist/my-bundle.ezb --force
./build/ezdox-cli bundle list --long
```

## Documentation Files

Top-level user docs:

- `README.md`
- `INSTALL.md`
- `GUIDE.md`

Project manual:

- `docs/frontpage.md`
- `docs/manual/01-introduction.md`
- `docs/manual/02-configuration.md`
- `docs/manual/03-cli-usage.md`
- `docs/manual/04-markups.md`
- `docs/manual/05-targets.md`
- `docs/manual/06-bundles.md`
- `docs/manual/07-doxygen-commands.md`
- `docs/manual/08-source-scanning.md`
- `docs/manual/09-pipelines.md`
- `docs/manual/10-templates.md`
- `docs/manual/11-installation.md`
- `docs/manual/12-troubleshooting.md`

## Status

Current repository state includes:

- working build system
- operational CLI
- basic core model
- template-based HTML/LaTeX generation
- self-hosted documentation generation
- passing `ezdox-tests`
- clean `tidy` and `chktex` validation for generated self-doc output

## See Also

- `INSTALL.md` for build and install instructions
- `GUIDE.md` for end-to-end usage
- `docs/EZDox.yaml` for the repository’s own doc config
