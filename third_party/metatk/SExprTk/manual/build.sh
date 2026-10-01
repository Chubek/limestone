#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
dist="${root}/dist"
pdf="${1:-${dist}/SExprTk-Manual.pdf}"
case "${pdf}" in
  *.pdf) html="${pdf%.pdf}.html" ;;
  *) html="${pdf}.html"; pdf="${pdf}.pdf" ;;
esac
mkdir -p "$(dirname "${pdf}")" "$(dirname "${html}")"
tmp="$(mktemp)"
trap 'rm -f "${tmp}"' EXIT

for chapter in "${root}"/[0-9][0-9]-*.md; do
  cat "${chapter}" >> "${tmp}"
  printf '\n\n\\newpage\n\n' >> "${tmp}"
done

pandoc "${tmp}" --from=gfm --standalone --toc \
  --metadata title="SExprTk Manual" \
  --metadata author="SExprTk" \
  -o "${html}"

if command -v xelatex >/dev/null 2>&1 && command -v kpsewhich >/dev/null 2>&1 &&
   [[ -n "$(kpsewhich xelatex.fmt 2>/dev/null || true)" ]]; then
  pandoc "${tmp}" --from=gfm --standalone --toc \
    --metadata title="SExprTk Manual" \
    --metadata author="SExprTk" \
    --pdf-engine=xelatex -o "${pdf}"
elif command -v pdflatex >/dev/null 2>&1 && command -v kpsewhich >/dev/null 2>&1 &&
     [[ -n "$(kpsewhich pdflatex.fmt 2>/dev/null || true)" ]]; then
  pandoc "${tmp}" --from=gfm --standalone --toc \
    --metadata title="SExprTk Manual" \
    --metadata author="SExprTk" \
    --pdf-engine=pdflatex -o "${pdf}"
elif [[ -f "${root}/markdown_to_pdf.py" ]]; then
  python3 "${root}/markdown_to_pdf.py" "${tmp}" "${pdf}"
else
  echo "error: no functional PDF renderer found" >&2
  exit 1
fi
printf 'wrote %s\nwrote %s\n' "${html}" "${pdf}"
