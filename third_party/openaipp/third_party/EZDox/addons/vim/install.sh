#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: addons/vim/install.sh [--vim] [--nvim]

Install the EZDox Vimscript addon into XDG config directories.

Options:
  --vim    Install to $XDG_CONFIG_HOME/vim
  --nvim   Install to $XDG_CONFIG_HOME/nvim
  -h, --help
           Show this help text
EOF
}

install_vim=0
install_nvim=0

while (($# > 0)); do
  case "$1" in
    --vim)
      install_vim=1
      ;;
    --nvim)
      install_nvim=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

if ((install_vim == 0 && install_nvim == 0)); then
  echo "error: choose at least one of --vim or --nvim" >&2
  usage >&2
  exit 2
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
config_home="${XDG_CONFIG_HOME:-$HOME/.config}"

copy_addon() {
  local dest="$1"
  mkdir -p "$dest/plugin" "$dest/autoload" "$dest/after" "$dest/doc"
  cp -R "$script_dir/plugin/." "$dest/plugin/"
  cp -R "$script_dir/autoload/." "$dest/autoload/"
  cp -R "$script_dir/after/." "$dest/after/"
  cp -R "$script_dir/doc/." "$dest/doc/"
  echo "Installed EZDox Vim addon to $dest"
}

if ((install_vim)); then
  copy_addon "$config_home/vim"
fi

if ((install_nvim)); then
  copy_addon "$config_home/nvim"
fi
