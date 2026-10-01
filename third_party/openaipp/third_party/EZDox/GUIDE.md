# GUIDE

This guide shows how to use EZDox in a real project.
It focuses on the implemented workflow in this repository.

## What EZDox Does

EZDox scans source files for Doxygen-style comments, builds a document model,
passes that model through markup processors, and renders one or more output
targets such as HTML and LaTeX.

Core pipeline:

```text
source comments -> document model -> markup -> target renderer -> output files
```

## First Project Setup

Create a config file with the scaffold command:

```bash
./build/ezdox-cli config scaffold \
  -f yaml \
  -o EZDox.yaml \
  -p "MyProject" \
  -V "1.0.0" \
  -S src \
  -I include \
  -E build \
  -m Markdown \
  -t HTML \
  --with-commands \
  --with-pipelines
```

This creates a starter config you can edit.

## Example Source Comments

EZDox recognizes triple-slash comments, block comments, and trailing field
comments.

```cpp
/// @brief Compute a checksum.
/// @param input The input buffer.
/// @return The computed checksum.
std::uint32_t checksum(std::string_view input);
```

```cpp
/**
 * @brief Parse a project file.
 * @param path Path to the file.
 * @return Parsed project data.
 */
Project parse_project(const std::string& path);
```

```cpp
int value; ///< @brief A documented field.
```

## Minimal Config

A hand-written config can be as small as this:

```yaml
project: "MyProject"
version: "1.0.0"
sources:
  - src
includes:
  - include
excludes:
  - build
targets:
  - HTML
markups:
  - Markdown
```

## Validate Configuration

Before generation, validate the config:

```bash
./build/ezdox-cli config validate -C EZDox.yaml
```

Print the whole config:

```bash
./build/ezdox-cli config print -C EZDox.yaml
```

Print one key:

```bash
./build/ezdox-cli config print -C EZDox.yaml --key project
```

## Scan Before Generating

Use `find` to inspect what EZDox detects.

```bash
./build/ezdox-cli find -S src --summary
```

Filter by command:

```bash
./build/ezdox-cli find -S src -c "@param" --json
```

This is useful when debugging comment coverage.

## Generate Documentation

Generate HTML output:

```bash
./build/ezdox-cli generate -C EZDox.yaml -O build/docs -t HTML
```

Generate HTML and LaTeX with templates:

```bash
./build/ezdox-cli generate \
  -C EZDox.yaml \
  -O build/docs \
  -t HTML \
  -t LaTeX \
  --template templates \
  --clean \
  --profile
```

Output directories are target-specific:

```text
build/docs/
├── html/
│   └── index.html
└── latex/
    └── manual.tex
```

## Use the Built-in Templates

EZDox ships with HTML and LaTeX templates under `templates/`.
The HTML template understands:

- project name and version
- frontpage content
- manual chapters
- scraped API items

The LaTeX template currently aims for a stable, valid output path rather than
high-fidelity Markdown conversion.

## Document a Project Frontpage and Manual

You can include hand-written project docs before scraped API content.

Example config:

```yaml
project: "MyProject"
version: "1.0.0"
frontpage: docs/frontpage.md
manual: docs/manual
sources:
  - src
targets:
  - HTML
  - LaTeX
markups:
  - Markdown
```

In HTML output, EZDox renders:

1. frontpage
2. manual chapters
3. scraped docstrings

## Commands and Pipelines

Define commands inside config:

```yaml
commands:
  build-docs: "ezdox-cli generate -C EZDox.yaml -O build/docs"
  validate-docs: "ezdox-cli config validate -C EZDox.yaml"

pipelines:
  docs:
    - validate-docs
    - build-docs
```

Run one command:

```bash
./build/ezdox-cli run -C EZDox.yaml -n build-docs
```

Dry run a pipeline:

```bash
./build/ezdox-cli run -C EZDox.yaml -n docs --dry-run
```

Add environment variables:

```bash
./build/ezdox-cli run -C EZDox.yaml -n docs -e MODE=ci -e COLOR=never
```

## Bundle Workflow

Build a bundle from a directory:

```bash
./build/ezdox-cli bundle build \
  -s extensions/my-bundle \
  -o dist/my-bundle.ezb \
  -n my-bundle \
  -V 0.1.0 \
  -d "Example EZDox bundle"
```

Install it:

```bash
./build/ezdox-cli bundle install -b dist/my-bundle.ezb --force
```

Inspect installed bundles:

```bash
./build/ezdox-cli bundle list --long
./build/ezdox-cli bundle inspect -b dist/my-bundle.ezb --json
```

## Install Generated Docs

Copy generated docs to a destination:

```bash
./build/ezdox-cli install -O build/docs -d /var/www/docs --mode copy
```

Create a symlink instead:

```bash
./build/ezdox-cli install -O build/docs -d /var/www/docs --mode symlink
```

## Doctor and Paths

See where EZDox stores its runtime data:

```bash
./build/ezdox-cli paths --all
```

Fix missing directories:

```bash
./build/ezdox-cli doctor --fix
```

## Self-Hosted Example

This repository already documents itself.
You can regenerate its docs with:

```bash
./build/ezdox-cli generate -C docs/EZDox.yaml -O docs/_build --template templates
```

Validate them:

```bash
tidy -qe docs/_build/html/index.html
chktex -q -n1 -n8 -n46 docs/_build/latex/manual.tex
```

Some `chktex` installations print a resource-file warning before exiting
successfully.

## Recommended Workflow

For daily work, this sequence is usually enough:

```bash
./build/ezdox-cli config validate -C EZDox.yaml
./build/ezdox-cli find -S src --summary
./build/ezdox-cli generate -C EZDox.yaml -O build/docs --template templates --clean
./build/ezdox-cli install -O build/docs -d public/docs --mode copy
```

That gives you a simple and repeatable documentation pipeline.
