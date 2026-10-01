: ${EZDOX_HOME:="$HOME/.ezdox"}
export EZDOX_HOME

_ezdox_config_path() {
  local candidate
  for candidate in EZDox.yaml EZDox.yam EZDox.json EZDox.sexp EZDox.xml; do
    [[ -f "$candidate" ]] && { print -r -- "$candidate"; return 0; }
  done
  return 1
}

ezdox-schema() {
  local schema="$EZDOX_HOME/manifests/EZDox.schema.json"
  [[ -f "$schema" ]] && { print -r -- "$schema"; return 0; }
  print -u2 -- "EZDox schema not found at $schema"
  return 1
}

ezdox-validate() {
  local config="${1:-$(_ezdox_config_path)}"
  [[ -n "$config" ]] || { print -u2 -- "No EZDox config found"; return 1; }
  ezdox-cli config validate -C "$config" || return $?

  local schema="$EZDOX_HOME/manifests/EZDox.schema.json"
  if [[ -f "$schema" ]] && command -v check-jsonschema >/dev/null 2>&1; then
    case "$config" in
      *.json|*.yaml|*.yml|*.yam) check-jsonschema --schemafile "$schema" "$config" ;;
    esac
  elif ! command -v check-jsonschema >/dev/null 2>&1; then
    print -u2 -- "Tip: install check-jsonschema for schema validation."
  fi
}

ezdox-build() {
  local config output template
  local -a targets template_args
  config=""
  output="docs/_build"
  template=""
  while (($#)); do
    case "$1" in
      -c|--config) config="$2"; shift 2 ;;
      -o|--output) output="$2"; shift 2 ;;
      --template) template="$2"; shift 2 ;;
      --html) targets+=(-t HTML); shift ;;
      --latex) targets+=(-t LaTeX); shift ;;
      *) print -u2 -- "unknown argument: $1"; return 2 ;;
    esac
  done
  [[ -n "$config" ]] || config="$(_ezdox_config_path)"
  [[ -n "$config" ]] || { print -u2 -- "No EZDox config found"; return 1; }
  (( ${#targets[@]} )) || targets=(-t HTML -t LaTeX)
  [[ -z "$template" ]] || template_args=(--template "$template")
  ezdox-cli generate -C "$config" -O "$output" "${template_args[@]}" "${targets[@]}"
}

ezdox-build-html() {
  ezdox-build --html "$@"
}

ezdox-build-latex() {
  ezdox-build --latex "$@"
}

ezdox-build-pdf() {
  local output="docs/_build"
  local -a pass_args
  while (($#)); do
    case "$1" in
      -o|--output) output="$2"; pass_args+=("$1" "$2"); shift 2 ;;
      *) pass_args+=("$1"); shift ;;
    esac
  done
  ezdox-build --latex "${pass_args[@]}" || return $?
  command -v latexmk >/dev/null 2>&1 || { print -u2 -- "latexmk is required to build PDF output"; return 1; }
  latexmk -pdf -interaction=nonstopmode -halt-on-error "$output/latex/manual.tex"
}

ezdox-new-build-script() {
  local path="${1:-docs/build.sh}"
  mkdir -p "${path:h}"
  cat > "$path" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"${root}/build/ezdox-cli" generate -C "${root}/docs/EZDox.yaml" -O "${root}/docs/_build" --template "${root}/templates" "$@"
EOF
  chmod +x "$path"
  print -r -- "Wrote $path"
}

ezdox-cd-home() {
  mkdir -p "$EZDOX_HOME"
  cd "$EZDOX_HOME"
}

_ezdox_cli_completion() {
  local -a commands targets
  commands=(help version paths config bundle find generate install run doctor)
  targets=(HTML LaTeX Manpage ROFF XML)
  _arguments \
    '1:command:(${commands[*]})' \
    '-C[EZDox config file]:config:_files' \
    '--config[EZDox config file]:config:_files' \
    '-O[Documentation output directory]:output:_files -/' \
    '--output[Documentation output directory]:output:_files -/' \
    '-t[Output target]:target:(${targets[*]})' \
    '--target[Output target]:target:(${targets[*]})' \
    '--template[Template root]:template:_files -/' \
    '--clean[Clean output before generating]' \
    '--profile[Print timing/profile details]'
}

if autoload -Uz compdef >/dev/null 2>&1; then
  compdef _ezdox_cli_completion ezdox-cli
fi

