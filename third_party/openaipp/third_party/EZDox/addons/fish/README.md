# EZDox Fish Addon

Fisher-compatible quality-of-life helpers for EZDox.

## Install

```fish
fisher install /path/to/ezdoc/addons/fish
```

## Functions

- `ezdox-schema` prints `$EZDOX_HOME/manifests/EZDox.schema.json`.
- `ezdox-validate [CONFIG]` runs `ezdox-cli config validate` and uses `check-jsonschema` when available.
- `ezdox-build [--html] [--latex] [-c CONFIG] [-o OUTPUT] [--template DIR]` generates docs.
- `ezdox-build-html` and `ezdox-build-latex` are target-specific wrappers.
- `ezdox-build-pdf` generates LaTeX and runs `latexmk`.
- `ezdox-new-build-script [PATH]` writes a small `docs/build.sh` wrapper.
- `ezdox-cd-home` jumps to `$EZDOX_HOME`.

Optional external tools are detected with `command -v`; install `check-jsonschema`
for JSON Schema validation and `latexmk` for PDF output.

