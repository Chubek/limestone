#!/usr/bin/env bash
set -eu

usage() {
  cat <<USAGE
Usage: $0 <language> [--output-dir=DIR] [--xfeats +feature ...]
Example: $0 Python --output-dir=pynuperf --xfeats +decorators +null_check_guard
USAGE
}

[ "$#" -ge 1 ] || { usage; exit 1; }
LANG_RAW="$1"; shift
LANG_CANON="$(printf '%s' "$LANG_RAW" | tr '[:upper:]' '[:lower:]')"
OUT_DIR="bindings/out"
XFEATS_FILE="$(dirname "$0")/XFeats.yaml"
REQ_FEATURES=""

while [ "$#" -gt 0 ]; do
  case "$1" in
    --output-dir=*) OUT_DIR="${1#*=}" ;;
    --xfeats)
      shift
      while [ "$#" -gt 0 ] && [ "${1#-}" = "$1" ]; do
        REQ_FEATURES="$REQ_FEATURES ${1#+}"
        shift
      done
      continue
      ;;
    *) echo "unknown argument: $1" >&2; usage; exit 1 ;;
  esac
  shift
done

[ -f "$XFEATS_FILE" ] || { echo "missing feature catalog: $XFEATS_FILE" >&2; exit 1; }
mkdir -p "$OUT_DIR"

allowed="$(awk -v lang="$LANG_CANON" '
  BEGIN { mode="" }
  /^  universal:[[:space:]]*$/ { mode="universal"; next }
  /^  [A-Za-z_][A-Za-z0-9_\-]*:[[:space:]]*$/ {
    key=$0; sub(/^  /, "", key); sub(/:[[:space:]]*$/, "", key)
    mode=(tolower(key)==lang?"lang":"")
    next
  }
  /^    - name:[[:space:]]*/ {
    if (mode=="universal" || mode=="lang") {
      name=$0; sub(/^    - name:[[:space:]]*/, "", name); gsub(/[[:space:]]+$/, "", name); print name
    }
  }
' "$XFEATS_FILE" | sort -u)"

SWIG_DEFS=""
for f in $REQ_FEATURES; do
  [ -n "$f" ] || continue
  echo "$allowed" | grep -qx "$f" || { echo "unsupported feature for $LANG_RAW: $f" >&2; exit 1; }
  macro="NUPERF_XFEAT_$(printf '%s' "$f" | tr '[:lower:]-' '[:upper:]_')"
  SWIG_DEFS="$SWIG_DEFS -D$macro"
done

case "$LANG_CANON" in
  python)
    swig -python -outdir "$OUT_DIR" $SWIG_DEFS -Iinclude -o "$OUT_DIR/nuperf_wrap.c" "$(dirname "$0")/nuperf.i"
    ;;
  csharp|cs)
    swig -csharp -outdir "$OUT_DIR" $SWIG_DEFS -Iinclude -o "$OUT_DIR/nuperf_wrap.cxx" "$(dirname "$0")/nuperf.i"
    ;;
  *)
    echo "unsupported language: $LANG_RAW" >&2
    exit 1
    ;;
esac

echo "Generated bindings in $OUT_DIR"
