#!/usr/bin/env python3
import sys
from pathlib import Path


def pdf_escape(text: str) -> str:
    return text.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")


def wrap(text: str, width: int = 92):
    words = text.split()
    line = []
    current = 0
    for word in words:
        extra = len(word) if not line else len(word) + 1
        if current + extra > width:
            yield " ".join(line)
            line = [word]
            current = len(word)
        else:
            line.append(word)
            current += extra
    if line:
        yield " ".join(line)


def build_pages(lines, max_lines=56):
    page = []
    for line in lines:
        if len(page) >= max_lines:
            yield page
            page = []
        page.append(line)
    if page:
        yield page


def make_pdf(text: str) -> bytes:
    lines = []
    for raw in text.splitlines():
        raw = raw.rstrip()
        if not raw:
            lines.append("")
            continue
        if raw.startswith("```"):
            lines.append(raw)
            continue
        lines.extend(wrap(raw))

    objects = []
    pages = []
    font_obj = 1
    objects.append(b"<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>")

    for page_lines in build_pages(lines):
        content = ["BT", "/F1 10 Tf", "50 790 Td", "12 TL"]
        first = True
        for line in page_lines:
            escaped = pdf_escape(line)
            if first:
                content.append(f"({escaped}) Tj")
                first = False
            else:
                content.append(f"T* ({escaped}) Tj")
        content.append("ET")
        content_bytes = "\n".join(content).encode("latin-1", "replace")
        content_obj_id = len(objects) + 1
        objects.append(f"<< /Length {len(content_bytes)} >>\nstream\n".encode() + content_bytes + b"\nendstream")
        page_obj_id = len(objects) + 1
        pages.append(page_obj_id)
        objects.append(
            f"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents {content_obj_id} 0 R /Resources << /Font << /F1 {font_obj} 0 R >> >> >>".encode()
        )

    kids = " ".join(f"{pid} 0 R" for pid in pages)
    pages_obj = f"<< /Type /Pages /Kids [{kids}] /Count {len(pages)} >>".encode()
    catalog_obj = b"<< /Type /Catalog /Pages 2 0 R >>"

    objects = [catalog_obj, pages_obj] + objects

    offsets = []
    out = bytearray(b"%PDF-1.4\n")
    for i, obj in enumerate(objects, start=1):
        offsets.append(len(out))
        out.extend(f"{i} 0 obj\n".encode())
        out.extend(obj)
        out.extend(b"\nendobj\n")
    xref_offset = len(out)
    out.extend(f"xref\n0 {len(objects)+1}\n".encode())
    out.extend(b"0000000000 65535 f \n")
    for off in offsets:
        out.extend(f"{off:010d} 00000 n \n".encode())
    out.extend(f"trailer\n<< /Size {len(objects)+1} /Root 1 0 R >>\nstartxref\n{xref_offset}\n%%EOF\n".encode())
    return bytes(out)


def main():
    src = Path(sys.argv[1])
    dst = Path(sys.argv[2])
    dst.write_bytes(make_pdf(src.read_text(encoding="utf-8")))


if __name__ == "__main__":
    main()

