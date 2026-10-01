# EZDox Zsh Addon

Quality-of-life helpers for EZDox.

## Install

Source `ezdox.plugin.zsh` from `.zshrc`, or install this directory with your
preferred Zsh plugin manager.

```zsh
source /path/to/ezdoc/addons/zsh/ezdox.plugin.zsh
```

## Functions

- `ezdox-schema` prints `$EZDOX_HOME/manifests/EZDox.schema.json`.
- `ezdox-validate [CONFIG]` runs CLI validation and uses `check-jsonschema` when available.
- `ezdox-build [--html] [--latex] [-c CONFIG] [-o OUTPUT] [--template DIR]` generates docs.
- `ezdox-build-html`, `ezdox-build-latex`, and `ezdox-build-pdf` wrap common targets.
- `ezdox-new-build-script [PATH]` writes a small documentation build wrapper.
- `ezdox-cd-home` jumps to `$EZDOX_HOME`.

Optional external tools are checked with `command -v`; install `check-jsonschema`
for schema validation and `latexmk` for PDF output.
