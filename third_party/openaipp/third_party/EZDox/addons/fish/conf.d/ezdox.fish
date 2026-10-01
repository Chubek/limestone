set -q EZDOX_HOME; or set -gx EZDOX_HOME "$HOME/.ezdox"

function __ezdox_config_path --description 'Find the nearest EZDox config file'
    for candidate in EZDox.yaml EZDox.yam EZDox.json EZDox.sexp EZDox.xml
        if test -f "$candidate"
            echo "$candidate"
            return 0
        end
    end
    return 1
end

function ezdox-schema --description 'Print the installed EZDox JSON Schema path'
    set -l schema "$EZDOX_HOME/manifests/EZDox.schema.json"
    if test -f "$schema"
        echo "$schema"
        return 0
    end
    echo "EZDox schema not found at $schema" >&2
    return 1
end

function ezdox-validate --description 'Validate an EZDox config with ezdox-cli and optional schema tools'
    set -l config
    if test (count $argv) -gt 0
        set config $argv[1]
    else
        set config (__ezdox_config_path)
    end
    if test -z "$config"
        echo "No EZDox config found" >&2
        return 1
    end

    ezdox-cli config validate -C "$config"; or return $status

    set -l schema "$EZDOX_HOME/manifests/EZDox.schema.json"
    if test -f "$schema"; and command -v check-jsonschema >/dev/null 2>&1
        switch "$config"
            case '*.json' '*.yaml' '*.yml' '*.yam'
                check-jsonschema --schemafile "$schema" "$config"
        end
    else if not command -v check-jsonschema >/dev/null 2>&1
        echo "Tip: install check-jsonschema for schema validation." >&2
    end
end

function ezdox-build --description 'Generate EZDox docs for selected targets'
    argparse 'c/config=' 'o/output=' html latex template= -- $argv; or return 2
    set -l config "$_flag_config"
    test -n "$config"; or set config (__ezdox_config_path)
    test -n "$config"; or begin; echo "No EZDox config found" >&2; return 1; end
    set -l output "$_flag_output"
    test -n "$output"; or set output docs/_build
    set -l targets
    set -q _flag_html; and set -a targets -t HTML
    set -q _flag_latex; and set -a targets -t LaTeX
    test (count $targets) -gt 0; or set targets -t HTML -t LaTeX
    set -l template_args
    set -q _flag_template; and set template_args --template "$_flag_template"
    ezdox-cli generate -C "$config" -O "$output" $template_args $targets
end

function ezdox-build-html --description 'Generate EZDox HTML docs'
    ezdox-build --html $argv
end

function ezdox-build-latex --description 'Generate EZDox LaTeX docs'
    ezdox-build --latex $argv
end

function ezdox-build-pdf --description 'Generate EZDox LaTeX docs and compile PDF with latexmk'
    argparse 'c/config=' 'o/output=' template= -- $argv; or return 2
    set -l output "$_flag_output"
    test -n "$output"; or set output docs/_build
    set -l pass_args --latex -o "$output"
    set -q _flag_config; and set -a pass_args -c "$_flag_config"
    set -q _flag_template; and set -a pass_args --template "$_flag_template"
    ezdox-build $pass_args; or return $status
    if not command -v latexmk >/dev/null 2>&1
        echo "latexmk is required to build PDF output" >&2
        return 1
    end
    command latexmk -pdf -interaction=nonstopmode -halt-on-error "$output/latex/manual.tex"
end

function ezdox-new-build-script --description 'Create a docs/build.sh wrapper'
    set -l path docs/build.sh
    if test (count $argv) -gt 0
        set path $argv[1]
    end
    mkdir -p (dirname "$path")
    printf '%s\n' \
        '#!/usr/bin/env bash' \
        'set -euo pipefail' \
        'root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"' \
        '"${root}/build/ezdox-cli" generate -C "${root}/docs/EZDox.yaml" -O "${root}/docs/_build" --template "${root}/templates" "$@"' \
        > "$path"
    chmod +x "$path"
    echo "Wrote $path"
end

function ezdox-cd-home --description 'Change directory to $EZDOX_HOME'
    mkdir -p "$EZDOX_HOME"
    cd "$EZDOX_HOME"
end
